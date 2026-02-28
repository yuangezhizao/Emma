/**
 * @file test_encoder.c
 * @brief 工厂测试：旋转编码器 (SIQ-02FVS3)
 *
 * 测试流程：
 * 1. 初始化编码器驱动（PCNT 硬件正交解码）
 * 2. 打印初始 GPIO 电平用于诊断
 * 3. 等待 8 秒，期间监控位置变化和 B 通道中断计数
 * 4. 检测到旋转 -> PASS，未检测到 -> FAIL
 *
 * 编码器计数由 PCNT 硬件自动更新。
 * 测试循环只需周期性读取 PCNT 计数值并显示变化。
 */

#include "factory_test.h"
#include "../bsp/encoder/encoder.h"
#include "bsp/board_def.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "test_encoder";

#define TEST_DURATION_MS 8000
#define DIAG_INTERVAL_US 1000000 /* 每 1 秒打印一次诊断 */

test_result_t test_encoder_run(void)
{
    esp_err_t ret = hal_encoder_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FAIL: Encoder init failed: %s", esp_err_to_name(ret));
        return TEST_FAIL;
    }

    /* ── 打印初始 GPIO 电平 ─────────────────────────────────────── */
    int level_a = -1, level_b = -1;
    hal_encoder_get_raw_levels(&level_a, &level_b);
    ESP_LOGI(TAG, "Initial GPIO levels: A(GPIO%d)=%d, B(GPIO%d)=%d", PIN_ENCODER_A, level_a, PIN_ENCODER_B, level_b);
    ESP_LOGI(TAG, ">>> Rotate encoder BOTH directions within %d seconds <<<", TEST_DURATION_MS / 1000);

    hal_encoder_clear_count();

    bool rotation_detected = false;
    bool cw_detected = false;
    bool ccw_detected = false;
    int last_count = 0;

    /* 使用 esp_timer 精确计时 */
    int64_t start_us = esp_timer_get_time();
    int64_t end_us = start_us + (int64_t)TEST_DURATION_MS * 1000;
    int64_t last_diag_us = start_us;

    while (esp_timer_get_time() < end_us) {
        /*
         * PCNT 模式下 poll 为空操作，计数由硬件自动更新。
         * 保留调用以兼容接口。
         */
        hal_encoder_poll();

        /* 检查位置变化 */
        int count = 0;
        hal_encoder_get_count(&count);

        if (count != last_count) {
            int step = count - last_count;
            const char* dir = (step > 0) ? "CW" : "CCW";
            ESP_LOGI(TAG, "  Encoder position: %d (step: %+d, %s)", count, step, dir);
            rotation_detected = true;
            if (step > 0) {
                cw_detected = true;
            } else {
                ccw_detected = true;
            }
            last_count = count;
        }

        /* 每秒打印一次诊断信息（含 B 通道中断计数） */
        int64_t now = esp_timer_get_time();
        if (now - last_diag_us >= DIAG_INTERVAL_US) {
            int a, b;
            hal_encoder_get_raw_levels(&a, &b);
            uint32_t b_isr = hal_encoder_get_b_isr_count();
            int elapsed_s = (int)((now - start_us) / 1000000);
            ESP_LOGI(TAG, "  [%ds] A=%d B=%d, position=%d, B_ISR=%lu", elapsed_s, a, b, last_count,
                     (unsigned long)b_isr);
            last_diag_us = now;
        }

        /* 让出 CPU 1 个 tick（~10ms） */
        vTaskDelay(1);
    }

    /* ── 最终结果 ───────────────────────────────────────────────── */
    hal_encoder_get_raw_levels(&level_a, &level_b);
    uint32_t final_b_isr = hal_encoder_get_b_isr_count();
    ESP_LOGI(TAG, "Final: A=%d B=%d, position=%d (CW:%s, CCW:%s), B_ISR=%lu", level_a, level_b, last_count,
             cw_detected ? "yes" : "no", ccw_detected ? "yes" : "no", (unsigned long)final_b_isr);

    hal_encoder_deinit();

    if (rotation_detected) {
        ESP_LOGI(TAG, "PASS: Encoder rotation detected (position = %d)", last_count);
        return TEST_PASS;
    } else {
        ESP_LOGE(TAG, "FAIL: No encoder rotation detected within %d seconds", TEST_DURATION_MS / 1000);
        return TEST_FAIL;
    }
}
