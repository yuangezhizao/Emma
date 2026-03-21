/**
 * @file factory_test.c
 * @brief 工厂测试运行器 - 执行所有测试用例并打印总结
 *
 * 职责：
 * - 初始化 NVS（WiFi RF 校准数据和 BLE 绑定信息均存储在 NVS）
 * - 打印系统信息横幅（芯片型号、Flash 大小、MAC 地址等）
 * - 管理 8 个测试用例注册表并按序执行
 * - 打印测试结果总结（通过/失败/跳过）
 *
 * 测试顺序：Flash → 芯片诊断 → WiFi → BLE → RGB LED →
 *           编码器 → 按钮 → I2C
 * 注意：蜂鸣器测试已注释掉，当前仅执行 8 项测试
 */

#include "factory_test.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <inttypes.h>
#include <stdio.h>

static const char* TAG = "factory_test";

/* ── 测试用例注册表（按执行顺序排列） ────────────────────────────── */
static const test_case_t s_tests[] = {
    {"SPI Flash", test_flash_run},
    {"Chip Diagnostics", test_chip_run},
    {"WiFi (2.4 GHz Scan)", test_wifi_run},
    {"BLE (Device Scan)", test_ble_run},
    {"RGB LED (WS2812)", test_rgb_led_run},
    {"Buzzer (PWM)", test_buzzer_run},
    {"Rotary Encoder (PCNT)", test_encoder_run},
    {"BOOT Button (GPIO0)", test_button_run},
    {"I2C Bus Scan", test_i2c_run},
};

#define NUM_TESTS (sizeof(s_tests) / sizeof(s_tests[0]))

/**
 * @brief 将测试结果枚举转换为字符串
 */
static const char* result_to_str(test_result_t r)
{
    switch (r) {
        case TEST_PASS:
            return "PASS";
        case TEST_FAIL:
            return "FAIL";
        case TEST_SKIP:
            return "SKIP";
        default:
            return "????";
    }
}

/**
 * @brief 打印系统信息横幅
 *
 * 显示芯片型号、核心数、修订版、功能特性、
 * Flash 大小、MAC 地址、堆信息和 SDK 版本。
 */
static void print_system_info(void)
{
    printf("\n");
    printf("========================================================\n");
    printf("  Emma - Factory Test Firmware\n");
    printf("  Board: Mini HMI Core Board (ESP32-S3) V1.0\n");
    printf("========================================================\n");

    /* ── 芯片信息 ─────────────────────────────────────────────── */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("  Chip:     %s, %d core(s), rev v%d.%d\n", CONFIG_IDF_TARGET, chip_info.cores, chip_info.revision / 100,
           chip_info.revision % 100);

    printf("  Features:");
    if (chip_info.features & CHIP_FEATURE_WIFI_BGN)
        printf(" WiFi");
    if (chip_info.features & CHIP_FEATURE_BLE)
        printf(" BLE");
    printf("\n");

    /* ── Flash 信息 ───────────────────────────────────────────── */
    uint32_t flash_size = 0;
    if (esp_flash_get_size(NULL, &flash_size) == ESP_OK) {
        printf("  Flash:    %" PRIu32 " MB %s\n", flash_size / (1024 * 1024),
               (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "(embedded)" : "(external)");
    }

    /* ── MAC 地址 ─────────────────────────────────────────────── */
    uint8_t mac[6];
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {
        printf("  MAC:      %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    /* ── 堆内存和 SDK 版本 ────────────────────────────────────── */
    printf("  Heap:     %" PRIu32 " bytes free\n", esp_get_minimum_free_heap_size());
    printf("  IDF:      %s\n", esp_get_idf_version());
    printf("  Compiled: %s %s\n", __DATE__, __TIME__);
    printf("========================================================\n\n");
}

void factory_test_run_all(void)
{
    /* ══════════════════════════════════════════════════════════════
     *  NVS 初始化（WiFi RF 校准数据和 BLE 绑定信息均存储在 NVS）
     * ══════════════════════════════════════════════════════════════ */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition truncated, erasing...");
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(ret));
        printf("FATAL: NVS init failed, cannot continue\n");
        return;
    }
    ESP_LOGI(TAG, "NVS initialized");

    /* ── 打印系统信息 ─────────────────────────────────────────── */
    print_system_info();

    /* ══════════════════════════════════════════════════════════════
     *  依次运行所有测试
     * ══════════════════════════════════════════════════════════════ */
    test_result_t results[NUM_TESTS];
    int pass_count = 0;
    int fail_count = 0;
    int skip_count = 0;

    for (int i = 0; i < (int)NUM_TESTS; i++) {
        printf("--------------------------------------------------------\n");
        printf("  [%d/%d] Running: %s\n", i + 1, (int)NUM_TESTS, s_tests[i].name);
        printf("--------------------------------------------------------\n");

        results[i] = s_tests[i].run();

        switch (results[i]) {
            case TEST_PASS:
                pass_count++;
                break;
            case TEST_FAIL:
                fail_count++;
                break;
            case TEST_SKIP:
                skip_count++;
                break;
        }

        printf("  Result: %s\n\n", result_to_str(results[i]));

        /* 测试间短暂延迟，让串口输出刷新 */
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    /* ══════════════════════════════════════════════════════════════
     *  测试总结报告
     * ══════════════════════════════════════════════════════════════ */
    printf("\n");
    printf("========================================================\n");
    printf("  FACTORY TEST SUMMARY\n");
    printf("========================================================\n");

    for (int i = 0; i < (int)NUM_TESTS; i++) {
        printf("  [%s] %s\n", result_to_str(results[i]), s_tests[i].name);
    }

    printf("--------------------------------------------------------\n");
    printf("  Total: %d | Pass: %d | Fail: %d | Skip: %d\n", (int)NUM_TESTS, pass_count, fail_count, skip_count);
    printf("========================================================\n");

    if (fail_count == 0) {
        ESP_LOGI(TAG, "ALL TESTS PASSED!");
    } else {
        ESP_LOGE(TAG, "%d TEST(S) FAILED!", fail_count);
    }
}
