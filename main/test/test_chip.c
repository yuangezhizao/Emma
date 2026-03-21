/**
 * @file test_chip.c
 * @brief 工厂测试：ESP32-S3 芯片诊断
 *
 * 验证项：
 * - 内部温度传感器可正常读取
 * - 温度在合理范围内 (-10°C ~ 85°C)
 * - 内部 SRAM 堆信息可正常获取
 * - PSRAM 信息报告（不影响通过/失败）
 */

#include "factory_test.h"
#include "../bsp/chip/chip.h"
#include "esp_log.h"

static const char* TAG = "test_chip";

#define TEMP_MIN_CELSIUS (-10.0f)
#define TEMP_MAX_CELSIUS (85.0f)

test_result_t test_chip_run(void)
{
    hal_chip_info_t info;
    esp_err_t ret = hal_chip_get_info(&info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FAIL: Cannot read chip info: %s", esp_err_to_name(ret));
        return TEST_FAIL;
    }

    bool pass = true;

    /* ── 温度检查 ─────────────────────────────────────────────── */
    ESP_LOGI(TAG, "Chip temperature: %.1f C", info.temperature_celsius);

    if (info.temperature_celsius < TEMP_MIN_CELSIUS || info.temperature_celsius > TEMP_MAX_CELSIUS) {
        ESP_LOGE(TAG, "FAIL: Temperature %.1f C out of range [%.0f, %.0f]", info.temperature_celsius, TEMP_MIN_CELSIUS,
                 TEMP_MAX_CELSIUS);
        pass = false;
    } else {
        ESP_LOGI(TAG, "Temperature within normal range");
    }

    /* ── 内存信息报告 ─────────────────────────────────────────── */
    ESP_LOGI(TAG, "Internal SRAM free: %lu bytes (%.1f KB)", (unsigned long)info.internal_heap,
             info.internal_heap / 1024.0f);

    if (info.psram_total > 0) {
        ESP_LOGI(TAG, "PSRAM: total=%lu bytes (%.1f MB), free=%lu bytes", (unsigned long)info.psram_total,
                 info.psram_total / (1024.0f * 1024.0f), (unsigned long)info.psram_free);
    } else {
        ESP_LOGI(TAG, "PSRAM: not detected (this board may not have PSRAM)");
    }

    return pass ? TEST_PASS : TEST_FAIL;
}
