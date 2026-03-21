/**
 * @file ble.c
 * @brief BLE 蓝牙扫描驱动实现
 *
 * 使用 Bluedroid BLE GAP 接口扫描周围蓝牙设备。
 * 用于工厂测试验证蓝牙射频前端和天线。
 *
 * 实现细节：
 * - ESP32-S3 硬件仅支持 BLE，不支持经典蓝牙；启动时释放控制器中
 *   预留的经典蓝牙内存（虽然硬件不支持，但控制器固件仍会预留）
 * - GAP 回调中基于 BDA 地址去重（最多跟踪 64 个唯一设备）
 * - 使用 FreeRTOS EventGroup 同步等待扫描完成
 * - 扫描参数：主动扫描，间隔 0x50，窗口 0x30
 *
 * BLE 控制器和 Bluedroid 协议栈依赖 NVS 存储蓝牙绑定等数据，
 * 需要在调用 hal_ble_init() 前调用 nvs_flash_init()。
 */

#include "ble.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <string.h>

static const char* TAG = "hal_ble";

/* ── 扫描完成事件标志 ─────────────────────────────────────────── */
#define BLE_SCAN_DONE_BIT BIT0

static EventGroupHandle_t s_scan_event_group = NULL;
static bool s_initialized = false;

/* ── 去重用的设备列表 ─────────────────────────────────────────── */
static struct {
    uint8_t addr[6]; /**< BDA 地址 */
    int8_t rssi;     /**< 最后一次收到的 RSSI */
    char name[32];   /**< 设备名称（如有） */
} s_devices[HAL_BLE_MAX_DEVICES];

static uint16_t s_device_count = 0;
static int8_t s_best_rssi = -128;
static int s_best_index = -1;

/**
 * @brief 在去重列表中查找设备
 * @return 设备索引，-1 表示未找到
 */
static int find_device(const uint8_t* addr)
{
    for (int i = 0; i < s_device_count; i++) {
        if (memcmp(s_devices[i].addr, addr, 6) == 0) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief 从 EIR 数据中提取设备名称
 */
static void extract_device_name(const uint8_t* eir, size_t eir_len, char* name_out, size_t name_max)
{
    if (eir == NULL || eir_len == 0) {
        return;
    }

    size_t offset = 0;
    while (offset < eir_len) {
        uint8_t len = eir[offset];
        if (len == 0 || offset + len >= eir_len) {
            break;
        }
        uint8_t type = eir[offset + 1];
        /* 完整本地名称或缩短名称 */
        if (type == 0x09 || type == 0x08) {
            size_t name_len = len - 1;
            if (name_len > name_max - 1) {
                name_len = name_max - 1;
            }
            memcpy(name_out, &eir[offset + 2], name_len);
            name_out[name_len] = '\0';
            return;
        }
        offset += len + 1;
    }
}

/**
 * @brief GAP 事件回调
 */
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param)
{
    switch (event) {
        case ESP_GAP_BLE_SCAN_RESULT_EVT: {
            esp_ble_gap_cb_param_t* scan_result = param;

            switch (scan_result->scan_rst.search_evt) {
                case ESP_GAP_SEARCH_INQ_RES_EVT: {
                    /* 收到扫描结果 —— 去重并记录 */
                    uint8_t* bda = scan_result->scan_rst.bda;
                    int8_t rssi = scan_result->scan_rst.rssi;
                    int idx = find_device(bda);

                    if (idx >= 0) {
                        /* 已知设备，更新 RSSI */
                        s_devices[idx].rssi = rssi;
                    } else if (s_device_count < HAL_BLE_MAX_DEVICES) {
                        /* 新设备，添加到列表 */
                        idx = s_device_count;
                        memcpy(s_devices[idx].addr, bda, 6);
                        s_devices[idx].rssi = rssi;
                        s_devices[idx].name[0] = '\0';

                        /* 尝试从 EIR 提取设备名称 */
                        extract_device_name(scan_result->scan_rst.ble_adv, scan_result->scan_rst.adv_data_len,
                                            s_devices[idx].name, sizeof(s_devices[idx].name));

                        s_device_count++;
                    }

                    /* 更新最强信号 */
                    if (idx >= 0 && rssi > s_best_rssi) {
                        s_best_rssi = rssi;
                        s_best_index = idx;
                    }
                    break;
                }
                case ESP_GAP_SEARCH_INQ_CMPL_EVT:
                    /* 扫描完成 */
                    ESP_LOGI(TAG, "BLE scan complete");
                    if (s_scan_event_group) {
                        xEventGroupSetBits(s_scan_event_group, BLE_SCAN_DONE_BIT);
                    }
                    break;
                default:
                    break;
            }
            break;
        }
        case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
            ESP_LOGD(TAG, "Scan parameters set");
            break;
        default:
            break;
    }
}

esp_err_t hal_ble_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    esp_err_t ret;

    /* ── 释放经典蓝牙预留内存（ESP32-S3 硬件不支持经典蓝牙，
     *    但控制器固件仍会预留该区域，释放可回收内存） ──────── */
    ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Classic BT memory release failed (may already be released): %s", esp_err_to_name(ret));
        /* 非致命错误，继续 */
    }

    /* ── 初始化 BT Controller（BLE 模式） ─────────────────────── */
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BT controller init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BT controller enable failed: %s", esp_err_to_name(ret));
        esp_bt_controller_deinit();
        return ret;
    }

    /* ── 初始化 Bluedroid ─────────────────────────────────────── */
    ret = esp_bluedroid_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Bluedroid init failed: %s", esp_err_to_name(ret));
        esp_bt_controller_disable();
        esp_bt_controller_deinit();
        return ret;
    }

    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Bluedroid enable failed: %s", esp_err_to_name(ret));
        esp_bluedroid_deinit();
        esp_bt_controller_disable();
        esp_bt_controller_deinit();
        return ret;
    }

    /* ── 注册 GAP 回调 ───────────────────────────────────────── */
    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GAP callback register failed: %s", esp_err_to_name(ret));
        esp_bluedroid_disable();
        esp_bluedroid_deinit();
        esp_bt_controller_disable();
        esp_bt_controller_deinit();
        return ret;
    }

    /* ── 创建事件组 ──────────────────────────────────────────── */
    s_scan_event_group = xEventGroupCreate();
    if (s_scan_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        esp_bluedroid_disable();
        esp_bluedroid_deinit();
        esp_bt_controller_disable();
        esp_bt_controller_deinit();
        return ESP_ERR_NO_MEM;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "BLE initialized (BLE-only mode)");
    return ESP_OK;
}

esp_err_t hal_ble_scan(uint32_t duration_sec, hal_ble_scan_result_t* result)
{
    if (!s_initialized || result == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(result, 0, sizeof(*result));
    result->best_rssi = -128;

    /* ── 重置设备列表 ─────────────────────────────────────────── */
    s_device_count = 0;
    s_best_rssi = -128;
    s_best_index = -1;
    memset(s_devices, 0, sizeof(s_devices));

    /* ── 设置扫描参数 ─────────────────────────────────────────── */
    esp_ble_scan_params_t scan_params = {
        .scan_type = BLE_SCAN_TYPE_ACTIVE,
        .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
        .scan_interval = 0x50,                        /* 50ms */
        .scan_window = 0x30,                          /* 30ms */
        .scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE, /* 不过滤重复，由我们自己去重 */
    };

    esp_err_t ret = esp_ble_gap_set_scan_params(&scan_params);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set scan params failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* ── 清除事件标志并启动扫描 ──────────────────────────────── */
    xEventGroupClearBits(s_scan_event_group, BLE_SCAN_DONE_BIT);

    ESP_LOGI(TAG, "Starting BLE scan for %lu seconds...", (unsigned long)duration_sec);
    ret = esp_ble_gap_start_scanning(duration_sec);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BLE scan start failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* ── 等待扫描完成（超时 = 扫描时长 + 3 秒余量） ──────────── */
    EventBits_t bits = xEventGroupWaitBits(s_scan_event_group, BLE_SCAN_DONE_BIT, pdTRUE, pdTRUE,
                                           pdMS_TO_TICKS((duration_sec + 3) * 1000));

    if (!(bits & BLE_SCAN_DONE_BIT)) {
        ESP_LOGW(TAG, "BLE scan timed out");
        esp_ble_gap_stop_scanning();
    }

    /* ── 填充结果 ─────────────────────────────────────────────── */
    result->device_count = s_device_count;

    if (s_best_index >= 0) {
        result->best_rssi = s_devices[s_best_index].rssi;
        memcpy(result->best_addr, s_devices[s_best_index].addr, 6);
        strncpy(result->best_name, s_devices[s_best_index].name, sizeof(result->best_name) - 1);
    }

    /* ── 打印扫描到的设备列表 ─────────────────────────────────── */
    ESP_LOGI(TAG, "BLE scan result: %d unique device(s)", s_device_count);
    for (int i = 0; i < s_device_count && i < 20; i++) {
        ESP_LOGI(TAG, "  [%2d] %02X:%02X:%02X:%02X:%02X:%02X  RSSI:%d dBm  %s", i + 1, s_devices[i].addr[0],
                 s_devices[i].addr[1], s_devices[i].addr[2], s_devices[i].addr[3], s_devices[i].addr[4],
                 s_devices[i].addr[5], s_devices[i].rssi, s_devices[i].name[0] ? s_devices[i].name : "(no name)");
    }
    if (s_device_count > 20) {
        ESP_LOGI(TAG, "  ... and %d more device(s)", s_device_count - 20);
    }

    return ESP_OK;
}

esp_err_t hal_ble_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();

    if (s_scan_event_group) {
        vEventGroupDelete(s_scan_event_group);
        s_scan_event_group = NULL;
    }

    s_initialized = false;
    ESP_LOGI(TAG, "BLE deinitialized");
    return ESP_OK;
}
