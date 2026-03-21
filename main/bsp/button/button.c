/**
 * @file button.c
 * @brief BOOT 按钮 (GPIO0) 驱动实现
 *
 * GPIO0 低电平有效，外部上拉。直接读取引脚电平。
 */

#include "button.h"
#include "bsp/board_def.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char* TAG = "hal_button";
static bool s_initialized = false;

esp_err_t hal_button_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    gpio_config_t io_cfg = {
        .pin_bit_mask = (1ULL << PIN_BOOT_KEY),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_config(&io_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "BOOT button initialized on GPIO%d", PIN_BOOT_KEY);
    return ESP_OK;
}

bool hal_button_is_pressed(void)
{
    if (!s_initialized) {
        return false;
    }
    return gpio_get_level(PIN_BOOT_KEY) == 0; /* 低电平有效 */
}

esp_err_t hal_button_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    gpio_reset_pin(PIN_BOOT_KEY);
    s_initialized = false;

    ESP_LOGI(TAG, "BOOT button deinitialized");
    return ESP_OK;
}
