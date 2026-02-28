/**
 * @file buzzer.c
 * @brief 无源蜂鸣器 (MLT-5020) 驱动，使用 LEDC PWM
 *
 * MLT-5020 是无源磁式蜂鸣器，共振频率 4000 Hz。
 * 需要方波 PWM 信号发声。蜂鸣器通过 GPIO46 上的 AO3400A N-MOSFET 驱动——
 * LEDC PWM 输出控制 MOSFET 栅极，切换蜂鸣器线圈电流。
 *
 * 数据手册关键参数：
 *   - 共振频率：4000 ± 200 Hz
 *   - 额定电压：3.0 Vo-p
 *   - 工作电压：2~4 Vo-p
 *   - 线圈电阻：12 ± 3 Ω
 *   - 推荐驱动：共振频率下 1/2 占空比方波
 */

#include "buzzer.h"
#include "bsp/board_def.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

static const char* TAG = "hal_buzzer";

/* LEDC 配置常量 */
#define BUZZER_LEDC_TIMER    LEDC_TIMER_0
#define BUZZER_LEDC_MODE     LEDC_LOW_SPEED_MODE
#define BUZZER_LEDC_CHANNEL  LEDC_CHANNEL_0
#define BUZZER_LEDC_DUTY_RES LEDC_TIMER_10_BIT /* 0 – 1023 */
#define BUZZER_DUTY_50_PCT   512               /* 1024 的 50% */

static bool s_initialized = false;

esp_err_t hal_buzzer_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    esp_err_t ret;

    /* ── 配置 LEDC 定时器 ────────────────────────────────────────── */
    ledc_timer_config_t timer_cfg = {
        .speed_mode = BUZZER_LEDC_MODE,
        .duty_resolution = BUZZER_LEDC_DUTY_RES,
        .timer_num = BUZZER_LEDC_TIMER,
        .freq_hz = BUZZER_DEFAULT_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ret = ledc_timer_config(&timer_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC timer config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* ── 配置 LEDC 通道 ──────────────────────────────────────────── */
    ledc_channel_config_t chan_cfg = {
        .speed_mode = BUZZER_LEDC_MODE,
        .channel = BUZZER_LEDC_CHANNEL,
        .timer_sel = BUZZER_LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = PIN_BUZZER,
        .duty = 0, /* 初始静音（0% 占空比 = 不切换） */
        .hpoint = 0,
    };
    ret = ledc_channel_config(&chan_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC channel config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Buzzer initialized: GPIO%d, LEDC timer=%d ch=%d, default %d Hz", PIN_BUZZER, BUZZER_LEDC_TIMER,
             BUZZER_LEDC_CHANNEL, BUZZER_DEFAULT_FREQ_HZ);
    return ESP_OK;
}

esp_err_t hal_buzzer_tone_start(uint32_t freq_hz)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret;

    /* 设置频率 */
    ret = ledc_set_freq(BUZZER_LEDC_MODE, BUZZER_LEDC_TIMER, freq_hz);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ledc_set_freq(%lu) failed: %s", (unsigned long)freq_hz, esp_err_to_name(ret));
        return ret;
    }

    /* 设置 50% 占空比（方波） */
    ret = ledc_set_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL, BUZZER_DUTY_50_PCT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ledc_set_duty failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 应用占空比更改 */
    ret = ledc_update_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ledc_update_duty failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGD(TAG, "Tone started: %lu Hz, duty=%d/1024", (unsigned long)freq_hz, BUZZER_DUTY_50_PCT);
    return ESP_OK;
}

esp_err_t hal_buzzer_tone_stop(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    /* 占空比设为 0（静音） */
    esp_err_t ret = ledc_set_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL, 0);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = ledc_update_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL);
    return ret;
}

esp_err_t hal_buzzer_tone(uint32_t freq_hz, uint32_t duration_ms)
{
    esp_err_t ret = hal_buzzer_tone_start(freq_hz);
    if (ret != ESP_OK) {
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    return hal_buzzer_tone_stop();
}

esp_err_t hal_buzzer_beep(uint32_t duration_ms)
{
    return hal_buzzer_tone(BUZZER_DEFAULT_FREQ_HZ, duration_ms);
}

esp_err_t hal_buzzer_gpio_toggle_test(uint32_t freq_hz, uint32_t duration_ms)
{
    /*
     * 诊断：绕过 LEDC，以软件方式手动翻转蜂鸣器 GPIO。
     * 用于独立于 LEDC 外设验证 MOSFET + 蜂鸣器电路。
     *
     * 注意：此操作会临时将 GPIO46 重新配置为普通输出，
     * 如需恢复 LEDC 操作，之后应调用 hal_buzzer_deinit() + hal_buzzer_init()。
     */
    ESP_LOGI(TAG, "GPIO toggle test: %lu Hz for %lu ms", (unsigned long)freq_hz, (unsigned long)duration_ms);

    /* 先反初始化 LEDC 以释放 GPIO */
    if (s_initialized) {
        ledc_stop(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL, 0);
    }

    /* 配置为普通 GPIO 输出 */
    gpio_reset_pin(PIN_BUZZER);
    gpio_set_direction(PIN_BUZZER, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_BUZZER, 0);

    /* 计算半周期（微秒） */
    uint32_t half_period_us = 500000 / freq_hz; /* 1e6 / (2 * freq) */
    uint32_t total_toggles = (uint32_t)((uint64_t)freq_hz * 2 * duration_ms / 1000);

    for (uint32_t i = 0; i < total_toggles; i++) {
        gpio_set_level(PIN_BUZZER, (i & 1) ? 0 : 1);
        esp_rom_delay_us(half_period_us);
    }

    gpio_set_level(PIN_BUZZER, 0);

    /* 标记为未初始化，因为已接管 GPIO */
    s_initialized = false;

    return ESP_OK;
}

esp_err_t hal_buzzer_deinit(void)
{
    if (!s_initialized) {
        /* 即使未通过 LEDC 初始化，也确保引脚复位 */
        gpio_set_level(PIN_BUZZER, 0);
        gpio_reset_pin(PIN_BUZZER);
        return ESP_OK;
    }

    /* 停止通道（输出低电平） */
    ledc_stop(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL, 0);

    gpio_reset_pin(PIN_BUZZER);
    s_initialized = false;

    ESP_LOGI(TAG, "Buzzer deinitialized");
    return ESP_OK;
}
