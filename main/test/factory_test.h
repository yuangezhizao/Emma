/**
 * @file factory_test.h
 * @brief 工厂测试框架 - 类型定义、测试声明和运行接口
 *
 * 定义测试结果枚举、测试用例结构体，
 * 以及所有 9 个测试模块的入口函数声明。
 */

#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 测试结果状态
 */
typedef enum {
    TEST_PASS, /**< 测试通过 */
    TEST_FAIL, /**< 测试失败 */
    TEST_SKIP, /**< 测试跳过 */
} test_result_t;

/**
 * @brief 测试用例描述符
 */
typedef struct {
    const char* name;           /**< 可读的测试名称（英文） */
    test_result_t (*run)(void); /**< 测试执行函数指针 */
} test_case_t;

/* ── 各测试模块入口函数声明（按执行顺序） ────────────────────────── */
test_result_t test_flash_run(void);   /**< Flash 芯片验证 */
test_result_t test_chip_run(void);    /**< 芯片诊断（温度 + 内存） */
test_result_t test_wifi_run(void);    /**< WiFi AP 扫描 */
test_result_t test_ble_run(void);     /**< BLE 设备扫描 */
test_result_t test_rgb_led_run(void); /**< RGB LED 颜色循环 */
test_result_t test_buzzer_run(void);  /**< 蜂鸣器音调测试 */
test_result_t test_encoder_run(void); /**< 旋转编码器检测 */
test_result_t test_button_run(void);  /**< BOOT 按钮检测 */
test_result_t test_i2c_run(void);     /**< I2C 总线扫描 */

/**
 * @brief 运行所有工厂测试
 *
 * 初始化 NVS，打印系统信息横幅，
 * 依次执行 9 个测试，最后打印总结报告。
 *
 * 测试顺序：Flash → 芯片诊断 → WiFi → BLE → RGB LED →
 *           蜂鸣器 → 编码器 → 按钮 → I2C
 */
void factory_test_run_all(void);

#ifdef __cplusplus
}
#endif
