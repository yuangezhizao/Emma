/**
 * @file encoder.h
 * @brief 旋转编码器 (SIQ-02FVS3) BSP 驱动，使用硬件 PCNT 正交解码
 *
 * SIQ-02FVS3 是 Samzo 机械增量式旋转编码器，15 个定位点，
 * 双相正交输出 (A/B)，带按压开关。
 * 公共引脚 (C) 接地；A 和 B 引脚需要上拉。
 *
 * 解码策略（使用 ESP32-S3 PCNT 硬件外设）：
 * - A 通道 (GPIO18) 作为边沿信号，仅检测下降沿
 * - B 通道 (GPIO3) 作为电平信号，PCNT 硬件在 A 下降沿
 *   瞬间以系统时钟速度读取 B 电平判断方向
 * - B=HIGH → 顺时针 (CW，+1)，B=LOW → 逆时针 (CCW，-1)
 * - 硬件毛刺滤波器 (10us) 过滤机械接触抖动
 * - 每个定位点精确产生 ±1 变化量
 *
 * 同时提供 B 通道 GPIO 中断计数器，用于诊断 B 通道是否有信号。
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化旋转编码器驱动（PCNT 硬件解码）
 *
 * 配置 A/B 通道的 PCNT 正交解码和按钮的 GPIO，
 * 同时在 B 通道注册 GPIO 中断用于诊断。
 *
 * @return ESP_OK 表示成功
 */
esp_err_t hal_encoder_init(void);

/**
 * @brief 编码器轮询（PCNT 模式下为空操作）
 *
 * 保留此接口以兼容测试框架调用。
 * PCNT 模式下计数由硬件自动更新，无需手动轮询。
 *
 * @return ESP_OK 表示成功
 */
esp_err_t hal_encoder_poll(void);

/**
 * @brief 获取当前编码器位置（以定位点为单位）
 *
 * 顺时针旋转位置递增，逆时针旋转位置递减。
 * 读取 PCNT 硬件计数器的当前值。
 *
 * @param count 存储位置值的指针
 * @return ESP_OK 表示成功
 */
esp_err_t hal_encoder_get_count(int* count);

/**
 * @brief 清零/重置编码器位置
 * @return ESP_OK 表示成功
 */
esp_err_t hal_encoder_clear_count(void);

/**
 * @brief 读取 A 和 B 通道的原始 GPIO 电平（诊断用）
 * @param level_a 存储 A 通道电平（0 或 1）的指针，可为 NULL
 * @param level_b 存储 B 通道电平（0 或 1）的指针，可为 NULL
 * @return ESP_OK 表示成功
 */
esp_err_t hal_encoder_get_raw_levels(int* level_a, int* level_b);

/**
 * @brief 读取编码器按压按钮状态
 * @return 按钮按下时返回 true（低电平有效）
 */
bool hal_encoder_button_pressed(void);

/**
 * @brief 获取 B 通道 GPIO 中断计数（诊断用）
 *
 * 返回自初始化以来 B 通道 (GPIO3) 上 ANYEDGE 中断触发的次数。
 * 若旋转编码器后返回值为 0，表示 B 通道无信号变化（硬件问题）。
 * 若返回值 > 0，表示 B 通道有信号但可能脉冲太短，
 * 被之前的 5ms 软件轮询错过了。
 *
 * @return B 通道中断触发次数
 */
uint32_t hal_encoder_get_b_isr_count(void);

/**
 * @brief 反初始化旋转编码器驱动
 *
 * 停止并释放 PCNT 单元和通道，移除 GPIO 中断，释放资源。
 *
 * @return ESP_OK 表示成功
 */
esp_err_t hal_encoder_deinit(void);

#ifdef __cplusplus
}
#endif
