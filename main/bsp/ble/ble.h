/**
 * @file ble.h
 * @brief BLE 蓝牙扫描硬件抽象层
 *
 * 使用 ESP32-S3 的 Bluedroid BLE GAP 扫描周围蓝牙设备，
 * 用于验证蓝牙射频前端和天线工作状态。
 *
 * 扫描参数：
 * - 模式：主动扫描（发送 Scan Request）
 * - 间隔：0x50 (50ms)，窗口：0x30 (30ms)
 * - 扫描时长：由调用者指定（建议 5 秒）
 * - 去重：基于 BDA 地址（最多跟踪 64 个唯一设备）
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 最多跟踪的唯一设备数量 */
#define HAL_BLE_MAX_DEVICES 64

/**
 * @brief BLE 扫描结果摘要
 */
typedef struct {
    uint16_t device_count; /**< 扫描到的唯一设备数量 */
    int8_t best_rssi;      /**< 最强信号的 RSSI (dBm) */
    uint8_t best_addr[6];  /**< 最强信号设备的蓝牙地址 */
    char best_name[32];    /**< 最强信号设备的名称（如有） */
} hal_ble_scan_result_t;

/**
 * @brief 初始化 BLE（仅 BLE 模式）
 *
 * 释放 Classic BT 内存 → 初始化/启用 BT Controller →
 * 初始化/启用 Bluedroid → 注册 GAP 回调。
 *
 * @return ESP_OK 表示成功
 */
esp_err_t hal_ble_init(void);

/**
 * @brief 执行 BLE 设备扫描
 *
 * 启动 GAP 扫描，使用 EventGroup 同步等待扫描完成。
 * 扫描期间自动去重（基于 BDA 地址）。
 *
 * @param duration_sec 扫描时长（秒）
 * @param result 扫描结果摘要
 * @return ESP_OK 表示成功
 */
esp_err_t hal_ble_scan(uint32_t duration_sec, hal_ble_scan_result_t* result);

/**
 * @brief 反初始化 BLE
 *
 * 禁用/清理 Bluedroid → 禁用/清理 BT Controller。
 *
 * @return ESP_OK 表示成功
 */
esp_err_t hal_ble_deinit(void);

#ifdef __cplusplus
}
#endif
