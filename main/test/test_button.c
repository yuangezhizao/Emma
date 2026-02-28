/**
 * @file test_button.c
 * @brief 工厂测试：BOOT 按钮 (GPIO0)
 *
 * 等待最多 5 秒，让操作员按下 BOOT 按钮。
 * 报告是否检测到按钮按压。
 */

#include "factory_test.h"
#include "../bsp/button/button.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "test_button";

#define TEST_DURATION_MS 5000
#define POLL_INTERVAL_MS 50

test_result_t test_button_run(void)
{
    esp_err_t ret = hal_button_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FAIL: Button init failed: %s", esp_err_to_name(ret));
        return TEST_FAIL;
    }

    ESP_LOGI(TAG, "Press BOOT button within %d seconds...", TEST_DURATION_MS / 1000);

    bool pressed = false;
    int iterations = TEST_DURATION_MS / POLL_INTERVAL_MS;

    for (int i = 0; i < iterations; i++) {
        if (hal_button_is_pressed()) {
            ESP_LOGI(TAG, "  BOOT button PRESSED");
            pressed = true;
            /* 等待释放 */
            while (hal_button_is_pressed()) {
                vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
            }
            ESP_LOGI(TAG, "  BOOT button RELEASED");
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
    }

    hal_button_deinit();

    if (pressed) {
        ESP_LOGI(TAG, "PASS: BOOT button press detected");
        return TEST_PASS;
    } else {
        ESP_LOGW(TAG, "SKIP: BOOT button not pressed within timeout");
        return TEST_SKIP;
    }
}
