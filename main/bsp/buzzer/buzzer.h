/**
 * @file buzzer.h
 * @brief 无源蜂鸣器 (MLT-5020) 硬件抽象层，通过 LEDC PWM 经 N-MOSFET 驱动
 *
 * MLT-5020 是无源（外部驱动）磁式蜂鸣器，共振频率 4000 ± 200 Hz。
 * 需要 50% 占空比的方波 PWM 信号发声。
 * 蜂鸣器通过 GPIO46 上的 AO3400A N-MOSFET 驱动。
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** MLT-5020 共振频率 */
#define BUZZER_DEFAULT_FREQ_HZ 4000

/**
 * @brief 初始化蜂鸣器驱动（配置 LEDC 定时器 + 通道）
 * @return ESP_OK 表示成功
 */
esp_err_t hal_buzzer_init(void);

/**
 * @brief 以指定频率播放音调（非阻塞，启动音调）
 * @param freq_hz 频率（Hz），推荐 MLT-5020 使用 4000
 * @return ESP_OK 表示成功
 */
esp_err_t hal_buzzer_tone_start(uint32_t freq_hz);

/**
 * @brief 停止当前音调
 * @return ESP_OK 表示成功
 */
esp_err_t hal_buzzer_tone_stop(void);

/**
 * @brief 以指定频率播放音调，持续指定时长（阻塞）
 * @param freq_hz 频率（Hz）
 * @param duration_ms 持续时间（毫秒）
 * @return ESP_OK 表示成功
 */
esp_err_t hal_buzzer_tone(uint32_t freq_hz, uint32_t duration_ms);

/**
 * @brief 以默认共振频率 (4000 Hz) 蜂鸣指定时长
 * @param duration_ms 持续时间（毫秒）
 * @return ESP_OK 表示成功
 */
esp_err_t hal_buzzer_beep(uint32_t duration_ms);

/**
 * @brief 通过原始 GPIO 翻转测试蜂鸣器（软件方波），用于独立验证 MOSFET/蜂鸣器电路
 * @param freq_hz  近似频率（Hz）
 * @param duration_ms 持续时间（毫秒）
 * @return ESP_OK 表示成功
 */
esp_err_t hal_buzzer_gpio_toggle_test(uint32_t freq_hz, uint32_t duration_ms);

/**
 * @brief 反初始化蜂鸣器驱动
 * @return ESP_OK 表示成功
 */
esp_err_t hal_buzzer_deinit(void);

#ifdef __cplusplus
}
#endif
