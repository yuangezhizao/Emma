/**
 * @file test_rgb_led.c
 * @brief 工厂测试：WS2812 RGB LED
 *
 * 依次显示红、绿、蓝、白，然后熄灭。
 * 需操作员目视检查。
 */

#include "factory_test.h"
#include "../bsp/rgb_led/rgb_led.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "test_rgb_led";

#define LED_DELAY_MS 500

test_result_t test_rgb_led_run(void)
{
    esp_err_t ret = hal_rgb_led_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FAIL: RGB LED init failed: %s", esp_err_to_name(ret));
        return TEST_FAIL;
    }

    ESP_LOGI(TAG, "Cycling RGB LED colors (visual inspection)...");

    /* 红色 */
    ESP_LOGI(TAG, "  -> RED");
    ret = hal_rgb_led_set(255, 0, 0);
    if (ret != ESP_OK)
        goto fail;
    vTaskDelay(pdMS_TO_TICKS(LED_DELAY_MS));

    /* 绿色 */
    ESP_LOGI(TAG, "  -> GREEN");
    ret = hal_rgb_led_set(0, 255, 0);
    if (ret != ESP_OK)
        goto fail;
    vTaskDelay(pdMS_TO_TICKS(LED_DELAY_MS));

    /* 蓝色 */
    ESP_LOGI(TAG, "  -> BLUE");
    ret = hal_rgb_led_set(0, 0, 255);
    if (ret != ESP_OK)
        goto fail;
    vTaskDelay(pdMS_TO_TICKS(LED_DELAY_MS));

    /* 白色 */
    ESP_LOGI(TAG, "  -> WHITE");
    ret = hal_rgb_led_set(255, 255, 255);
    if (ret != ESP_OK)
        goto fail;
    vTaskDelay(pdMS_TO_TICKS(LED_DELAY_MS));

    /* 熄灭 */
    hal_rgb_led_off();
    ESP_LOGI(TAG, "  -> OFF");

    hal_rgb_led_deinit();
    ESP_LOGI(TAG, "PASS: RGB LED test completed (visual check required)");
    return TEST_PASS;

fail:
    ESP_LOGE(TAG, "FAIL: RGB LED set failed: %s", esp_err_to_name(ret));
    hal_rgb_led_deinit();
    return TEST_FAIL;
}
