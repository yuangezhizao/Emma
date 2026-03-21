/**
 * @file test_ble.c
 * @brief 工厂测试：BLE 蓝牙设备扫描
 *
 * 验证项：
 * - BLE 协议栈（BT Controller + Bluedroid）可正常初始化
 * - GAP 扫描功能正常工作
 * - 报告扫描到的唯一设备数量和最强信号
 *
 * 通过条件：BLE 协议栈初始化和扫描过程正常完成
 * （即使未扫描到设备也通过，因为附近不一定有 BLE 设备）
 */

#include "factory_test.h"
#include "../bsp/ble/ble.h"
#include "esp_log.h"

static const char* TAG = "test_ble";

#define BLE_SCAN_DURATION_SEC 5

test_result_t test_ble_run(void)
{
    /* ── 初始化 BLE ──────────────────────────────────────────── */
    esp_err_t ret = hal_ble_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FAIL: BLE init failed: %s", esp_err_to_name(ret));
        return TEST_FAIL;
    }

    /* ── 执行扫描 ─────────────────────────────────────────────── */
    hal_ble_scan_result_t result;
    ret = hal_ble_scan(BLE_SCAN_DURATION_SEC, &result);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FAIL: BLE scan failed: %s", esp_err_to_name(ret));
        hal_ble_deinit();
        return TEST_FAIL;
    }

    /* ── 报告结果 ─────────────────────────────────────────────── */
    ESP_LOGI(TAG, "Unique BLE devices found: %d", result.device_count);

    if (result.device_count > 0) {
        ESP_LOGI(TAG, "Best signal: %02X:%02X:%02X:%02X:%02X:%02X (RSSI:%d dBm) %s", result.best_addr[0],
                 result.best_addr[1], result.best_addr[2], result.best_addr[3], result.best_addr[4],
                 result.best_addr[5], result.best_rssi, result.best_name[0] ? result.best_name : "(no name)");
    } else {
        ESP_LOGI(TAG, "No BLE devices in range (this is OK for factory test)");
    }

    /* ── 清理 ─────────────────────────────────────────────────── */
    hal_ble_deinit();

    /* 通过条件：BLE 协议栈初始化和扫描正常完成即通过 */
    ESP_LOGI(TAG, "BLE RF front-end OK");
    return TEST_PASS;
}
