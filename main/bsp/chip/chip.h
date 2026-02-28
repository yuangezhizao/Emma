/**
 * @file chip.h
 * @brief ESP32-S3 芯片诊断硬件抽象层
 *
 * 读取芯片内部温度传感器和内存信息，
 * 用于验证芯片基本功能和 PSRAM 状态。
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 芯片诊断信息结构体
 */
typedef struct {
    float temperature_celsius; /**< 芯片内部温度 (°C) */
    uint32_t internal_heap;    /**< 内部 SRAM 可用堆（字节） */
    uint32_t psram_total;      /**< PSRAM 总容量（字节），0 表示无 PSRAM */
    uint32_t psram_free;       /**< PSRAM 可用容量（字节） */
} hal_chip_info_t;

/**
 * @brief 读取芯片内部温度
 *
 * 使用 ESP32-S3 内置温度传感器读取芯片温度。
 * 每次调用会安装传感器→使能→读取→清理，开销较大但适合一次性诊断。
 *
 * @param temperature_c 输出温度值（°C）
 * @return ESP_OK 表示成功
 */
esp_err_t hal_chip_read_temperature(float* temperature_c);

/**
 * @brief 读取芯片诊断信息（温度 + 内存）
 *
 * @param info 待填充的芯片诊断信息结构体指针
 * @return ESP_OK 表示成功
 */
esp_err_t hal_chip_get_info(hal_chip_info_t* info);

#ifdef __cplusplus
}
#endif
