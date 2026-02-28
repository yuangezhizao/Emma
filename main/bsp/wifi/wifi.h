/**
 * @file wifi.h
 * @brief WiFi 扫描硬件抽象层
 *
 * 使用 ESP32-S3 的 WiFi STA 模式扫描周围 AP，
 * 用于验证 WiFi 射频前端和天线工作状态。
 *
 * 扫描所有 2.4 GHz 信道（1-13），报告 AP 数量和信号强度。
 * RSSI 值可作为天线性能的参考指标。
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 最多记录的 AP 数量 */
#define HAL_WIFI_MAX_AP 20

/**
 * @brief WiFi AP 扫描结果摘要
 */
typedef struct {
    uint16_t ap_count;    /**< 扫描到的 AP 数量 */
    int8_t best_rssi;     /**< 最强信号的 RSSI (dBm) */
    char best_ssid[33];   /**< 最强信号 AP 的 SSID */
    uint8_t best_channel; /**< 最强信号 AP 的信道 */
} hal_wifi_scan_result_t;

/**
 * @brief 初始化 WiFi（STA 模式）
 *
 * 初始化网络接口、事件循环、WiFi 驱动，启动 STA 模式。
 * 需要在调用前确保 NVS 已初始化。
 *
 * @return ESP_OK 表示成功
 */
esp_err_t hal_wifi_init(void);

/**
 * @brief 执行 WiFi AP 扫描
 *
 * 主动扫描所有 2.4 GHz 信道，阻塞直到扫描完成。
 * 返回 AP 数量和最强信号 AP 的详细信息。
 *
 * @param result 扫描结果摘要
 * @return ESP_OK 表示成功
 */
esp_err_t hal_wifi_scan(hal_wifi_scan_result_t* result);

/**
 * @brief 反初始化 WiFi
 *
 * 停止 WiFi 并释放资源。
 *
 * @return ESP_OK 表示成功
 */
esp_err_t hal_wifi_deinit(void);

#ifdef __cplusplus
}
#endif
