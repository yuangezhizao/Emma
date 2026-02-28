/**
 * @file rgb_led.h
 * @brief WS2812 RGB LED 硬件抽象层，通过 RMT 外设驱动
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 RGB LED (WS2812) 驱动
 * @return ESP_OK 表示成功
 */
esp_err_t hal_rgb_led_init(void);

/**
 * @brief 设置 RGB LED 颜色
 * @param red   红色通道 (0-255)
 * @param green 绿色通道 (0-255)
 * @param blue  蓝色通道 (0-255)
 * @return ESP_OK 表示成功
 */
esp_err_t hal_rgb_led_set(uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief 关闭 RGB LED
 * @return ESP_OK 表示成功
 */
esp_err_t hal_rgb_led_off(void);

/**
 * @brief 反初始化 RGB LED 驱动
 * @return ESP_OK 表示成功
 */
esp_err_t hal_rgb_led_deinit(void);

#ifdef __cplusplus
}
#endif
