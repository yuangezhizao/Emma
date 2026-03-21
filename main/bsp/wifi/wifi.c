/**
 * @file wifi.c
 * @brief WiFi 扫描驱动实现
 *
 * 初始化 WiFi STA 模式，执行主动扫描，收集 AP 信息。
 * 用于工厂测试验证射频前端和天线。
 *
 * 扫描参数：
 * - 类型：主动扫描（发送 Probe Request）
 * - 信道：所有 2.4 GHz 信道
 * - 每个信道停留：100-300ms
 * - 包含隐藏 SSID
 * - 预计总扫描时间：~3-5 秒
 *
 * WiFi 驱动依赖 NVS 存储 RF 校准数据，
 * 需要在调用 hal_wifi_init() 前调用 nvs_flash_init()。
 */

#include "wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include <string.h>

static const char* TAG = "hal_wifi";

static bool s_initialized = false;
static esp_netif_t* s_sta_netif = NULL;

esp_err_t hal_wifi_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    esp_err_t ret;

    /* ── 初始化网络接口（幂等，重复调用安全） ──────────────────── */
    ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Netif init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* ── 创建默认事件循环（幂等） ──────────────────────────────── */
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Event loop create failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* ── 创建默认 WiFi STA 网络接口 ───────────────────────────── */
    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (s_sta_netif == NULL) {
        ESP_LOGE(TAG, "Failed to create default WiFi STA netif");
        return ESP_FAIL;
    }

    /* ── 初始化 WiFi 驱动 ─────────────────────────────────────── */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* ── 配置为 STA 模式并启动 ────────────────────────────────── */
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi set mode failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi start failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "WiFi initialized in STA mode");
    return ESP_OK;
}

esp_err_t hal_wifi_scan(hal_wifi_scan_result_t* result)
{
    if (!s_initialized || result == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(result, 0, sizeof(*result));
    result->best_rssi = -128; /* 最差 RSSI 初始值 */

    /* ── 配置扫描参数：主动扫描所有信道 ───────────────────────── */
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,        /* 扫描所有信道 */
        .show_hidden = true, /* 包括隐藏 SSID */
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time =
            {
                .active =
                    {
                        .min = 100, /* 每个信道最少 100ms */
                        .max = 300, /* 每个信道最多 300ms */
                    },
            },
    };

    ESP_LOGI(TAG, "Starting WiFi AP scan...");

    /* 阻塞扫描，完成后返回 */
    esp_err_t ret = esp_wifi_scan_start(&scan_config, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi scan start failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* ── 获取扫描结果数量 ─────────────────────────────────────── */
    uint16_t ap_count = 0;
    ret = esp_wifi_scan_get_ap_num(&ap_count);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get AP count: %s", esp_err_to_name(ret));
        return ret;
    }

    result->ap_count = ap_count;
    ESP_LOGI(TAG, "Scan complete: %d AP(s) found", ap_count);

    if (ap_count == 0) {
        return ESP_OK;
    }

    /* ── 获取详细记录（最多 HAL_WIFI_MAX_AP 个） ──────────────── */
    uint16_t max_records = (ap_count < HAL_WIFI_MAX_AP) ? ap_count : HAL_WIFI_MAX_AP;
    wifi_ap_record_t* ap_records = calloc(max_records, sizeof(wifi_ap_record_t));
    if (ap_records == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for AP records");
        return ESP_ERR_NO_MEM;
    }

    ret = esp_wifi_scan_get_ap_records(&max_records, ap_records);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get AP records: %s", esp_err_to_name(ret));
        free(ap_records);
        return ret;
    }

    /* ── 查找最强信号的 AP 并打印列表 ─────────────────────────── */
    for (int i = 0; i < max_records; i++) {
        ESP_LOGI(TAG, "  [%2d] %-32s  CH:%2d  RSSI:%d dBm", i + 1, ap_records[i].ssid, ap_records[i].primary,
                 ap_records[i].rssi);
        if (ap_records[i].rssi > result->best_rssi) {
            result->best_rssi = ap_records[i].rssi;
            strncpy(result->best_ssid, (char*)ap_records[i].ssid, sizeof(result->best_ssid) - 1);
            result->best_channel = ap_records[i].primary;
        }
    }

    free(ap_records);
    return ESP_OK;
}

esp_err_t hal_wifi_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    esp_wifi_stop();
    esp_wifi_deinit();

    if (s_sta_netif) {
        esp_netif_destroy(s_sta_netif);
        s_sta_netif = NULL;
    }

    s_initialized = false;
    ESP_LOGI(TAG, "WiFi deinitialized");
    return ESP_OK;
}
