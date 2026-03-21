/**
 * @file flash.h
 * @brief 外部 SPI Flash 信息读取硬件抽象层
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Flash 信息结构体
 */
typedef struct {
    uint32_t size_bytes;      /**< Flash 容量（字节） */
    uint32_t manufacturer_id; /**< 制造商 ID */
    uint32_t device_id;       /**< 设备 ID */
} hal_flash_info_t;

/**
 * @brief 读取 Flash 芯片信息
 * @param info 待填充的 Flash 信息结构体指针
 * @return ESP_OK 表示成功
 */
esp_err_t hal_flash_get_info(hal_flash_info_t* info);

#ifdef __cplusplus
}
#endif
