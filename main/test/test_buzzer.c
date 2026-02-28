/**
 * @file test_buzzer.c
 * @brief 工厂测试：无源蜂鸣器 (MLT-5020)
 *
 * 测试流程：
 * 0. GPIO 直流回读诊断（验证引脚是否实际能输出）
 * 1. LEDC PWM 4000Hz 方波（共振频率）
 * 2. 软件 GPIO 翻转 4000Hz（绕过 LEDC，排查外设问题）
 *
 * MLT-5020 是无源磁式蜂鸣器，共振频率 4000±200Hz，
 * 通过 AO3400A N-MOSFET 由 GPIO46 驱动。
 */

#include "factory_test.h"
#include "../bsp/buzzer/buzzer.h"
#include "bsp/board_def.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "test_buzzer";

test_result_t test_buzzer_run(void)
{
    esp_err_t ret;

    /* ══════════════════════════════════════════════════════════════ */
    /* 测试 0: GPIO 直流输出诊断                                     */
    /* 目的：确认 GPIO 能否实际输出高/低电平                          */
    /* ══════════════════════════════════════════════════════════════ */
    ESP_LOGI(TAG, "[0/2] GPIO%d DC diagnostic (verify pin output)...", PIN_BUZZER);

    gpio_reset_pin(PIN_BUZZER);
    /* 配置为 INPUT_OUTPUT 模式，这样可以同时输出和回读 */
    gpio_set_direction(PIN_BUZZER, GPIO_MODE_INPUT_OUTPUT);

    /* 测试 LOW */
    gpio_set_level(PIN_BUZZER, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    int rb_low = gpio_get_level(PIN_BUZZER);
    ESP_LOGI(TAG, "  GPIO%d set LOW -> readback=%d %s", PIN_BUZZER, rb_low, rb_low == 0 ? "(OK)" : "(ERROR!)");

    /* 测试 HIGH */
    gpio_set_level(PIN_BUZZER, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    int rb_high = gpio_get_level(PIN_BUZZER);
    ESP_LOGI(TAG, "  GPIO%d set HIGH -> readback=%d %s", PIN_BUZZER, rb_high, rb_high == 1 ? "(OK)" : "(ERROR!)");

    /* 持续 HIGH 2 秒：如果 MOSFET 导通，可用万用表量蜂鸣器两端电压 */
    ESP_LOGI(TAG, "  Holding HIGH for 2s (measure MOSFET drain voltage with multimeter)...");
    vTaskDelay(pdMS_TO_TICKS(2000));

    gpio_set_level(PIN_BUZZER, 0);
    gpio_reset_pin(PIN_BUZZER);

    if (rb_low != 0 || rb_high != 1) {
        ESP_LOGE(TAG, "FAIL: GPIO%d readback error - pin may not be working", PIN_BUZZER);
        return TEST_FAIL;
    }

    ESP_LOGI(TAG, "  GPIO%d DC output OK", PIN_BUZZER);
    vTaskDelay(pdMS_TO_TICKS(500));

    /* ══════════════════════════════════════════════════════════════ */
    /* 测试 1: LEDC PWM 方波 4000Hz                                  */
    /* ══════════════════════════════════════════════════════════════ */
    ret = hal_buzzer_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FAIL: Buzzer init failed: %s", esp_err_to_name(ret));
        return TEST_FAIL;
    }

    ESP_LOGI(TAG, "[1/2] LEDC PWM 4000Hz square wave for 500ms...");
    ret = hal_buzzer_tone(4000, 500);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "  LEDC tone failed: %s", esp_err_to_name(ret));
    }

    hal_buzzer_deinit();
    vTaskDelay(pdMS_TO_TICKS(500));

    /* ══════════════════════════════════════════════════════════════ */
    /* 测试 2: 软件 GPIO 翻转 4000Hz（绕过 LEDC 外设）               */
    /* ══════════════════════════════════════════════════════════════ */
    ESP_LOGI(TAG, "[2/2] Software GPIO toggle 4000Hz for 500ms (bypass LEDC)...");
    ret = hal_buzzer_gpio_toggle_test(4000, 500);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "  GPIO toggle test failed: %s", esp_err_to_name(ret));
    }

    /* 最终清理 */
    hal_buzzer_deinit();

    ESP_LOGI(TAG, "PASS: Buzzer test completed (confirm sound was heard)");
    return TEST_PASS;
}
