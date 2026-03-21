/**
 * @file button.h
 * @brief BOOT 按钮 (GPIO0) 硬件抽象层，低电平有效
 */

#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 BOOT 按钮 GPIO
 * @return ESP_OK 表示成功
 */
esp_err_t hal_button_init(void);

/**
 * @brief 读取 BOOT 按钮状态
 * @return 按钮按下时返回 true（低电平有效，GPIO0 = 0）
 */
bool hal_button_is_pressed(void);

/**
 * @brief 反初始化按钮 GPIO
 * @return ESP_OK 表示成功
 */
esp_err_t hal_button_deinit(void);

#ifdef __cplusplus
}
#endif
