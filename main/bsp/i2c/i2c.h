/**
 * @file i2c.h
 * @brief I2C 总线硬件抽象层（扫描工具）
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 I2C 主机总线
 * @return ESP_OK 表示成功
 */
esp_err_t hal_i2c_init(void);

/**
 * @brief 扫描 I2C 总线并报告发现的设备
 * @param found_addrs 存储找到的 7 位地址的数组
 * @param max_addrs   最大存储地址数
 * @param num_found   存储实际找到数量的指针
 * @return ESP_OK 表示成功
 */
esp_err_t hal_i2c_scan(uint8_t* found_addrs, uint8_t max_addrs, uint8_t* num_found);

/**
 * @brief 反初始化 I2C 总线
 * @return ESP_OK 表示成功
 */
esp_err_t hal_i2c_deinit(void);

#ifdef __cplusplus
}
#endif
