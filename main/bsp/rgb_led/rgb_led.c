/**
 * @file rgb_led.c
 * @brief WS2812 RGB LED 驱动实现，使用 led_strip 托管组件
 */

#include "rgb_led.h"
#include "bsp/board_def.h"
#include "led_strip.h"
#include "esp_log.h"

static const char* TAG = "hal_rgb_led";

static led_strip_handle_t s_led_strip = NULL;

esp_err_t hal_rgb_led_init(void)
{
    if (s_led_strip != NULL) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    led_strip_config_t strip_config = {
        .strip_gpio_num = PIN_RGB_DATA,
        .max_leds = 1,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags.invert_out = false,
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, /* 10 MHz */
        .flags.with_dma = false,
    };

    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &s_led_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LED strip: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 初始关闭 */
    led_strip_clear(s_led_strip);

    ESP_LOGI(TAG, "RGB LED initialized on GPIO%d", PIN_RGB_DATA);
    return ESP_OK;
}

esp_err_t hal_rgb_led_set(uint8_t red, uint8_t green, uint8_t blue)
{
    if (s_led_strip == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = led_strip_set_pixel(s_led_strip, 0, red, green, blue);
    if (ret != ESP_OK) {
        return ret;
    }
    return led_strip_refresh(s_led_strip);
}

esp_err_t hal_rgb_led_off(void)
{
    if (s_led_strip == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return led_strip_clear(s_led_strip);
}

esp_err_t hal_rgb_led_deinit(void)
{
    if (s_led_strip == NULL) {
        return ESP_OK;
    }

    led_strip_clear(s_led_strip);
    esp_err_t ret = led_strip_del(s_led_strip);
    s_led_strip = NULL;

    ESP_LOGI(TAG, "RGB LED deinitialized");
    return ret;
}
