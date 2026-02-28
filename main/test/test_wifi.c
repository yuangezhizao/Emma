/**
 * @file test_wifi.c
 * @brief 工厂测试：WiFi 2.4 GHz AP 扫描
 *
 * 验证项：
 * - WiFi STA 模式可正常初始化
 * - 能够扫描到至少 1 个 AP
 * - 报告最强信号的 AP 信息和信号质量评估
 *
 * 信号质量标准：
 * - Excellent: RSSI > -50 dBm
 * - Good:      RSSI > -70 dBm
 * - Fair:      RSSI > -85 dBm
 * - Weak:      RSSI <= -85 dBm
 *
 * 通过条件：扫描到至少 1 个 AP
 */

#include "factory_test.h"
#include "../bsp/wifi/wifi.h"
#include "esp_log.h"

static const char* TAG = "test_wifi";

/**
 * @brief 根据 RSSI 值返回信号质量描述
 */
static const char* rssi_quality(int8_t rssi)
{
    if (rssi > -50)
        return "Excellent";
    if (rssi > -70)
        return "Good";
    if (rssi > -85)
        return "Fair";
    return "Weak";
}

test_result_t test_wifi_run(void)
{
    /* ── 初始化 WiFi ──────────────────────────────────────────── */
    esp_err_t ret = hal_wifi_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FAIL: WiFi init failed: %s", esp_err_to_name(ret));
        return TEST_FAIL;
    }

    /* ── 执行扫描 ─────────────────────────────────────────────── */
    hal_wifi_scan_result_t result;
    ret = hal_wifi_scan(&result);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FAIL: WiFi scan failed: %s", esp_err_to_name(ret));
        hal_wifi_deinit();
        return TEST_FAIL;
    }

    /* ── 报告结果 ─────────────────────────────────────────────── */
    ESP_LOGI(TAG, "AP count: %d", result.ap_count);

    if (result.ap_count > 0) {
        ESP_LOGI(TAG, "Best AP: \"%s\" (CH:%d, RSSI:%d dBm, %s)", result.best_ssid, result.best_channel,
                 result.best_rssi, rssi_quality(result.best_rssi));
    }

    /* ── 清理 ─────────────────────────────────────────────────── */
    hal_wifi_deinit();

    /* 通过条件：至少扫描到 1 个 AP */
    if (result.ap_count == 0) {
        ESP_LOGE(TAG, "FAIL: No AP found (check WiFi antenna)");
        return TEST_FAIL;
    }

    ESP_LOGI(TAG, "WiFi RF front-end OK");
    return TEST_PASS;
}
