/**
 * @file main_factory_test.c
 * @brief Emma 工厂测试固件 - 入口点
 *
 * 本固件测试迷你 HMI 核心板 (ESP32-S3) V1.0 上的所有硬件组件。
 * 依次运行各外设测试，并通过串口控制台打印总结报告。
 *
 * 测试组件：
 * - SPI Flash (W25Q128/W25Q256) - 芯片 ID 和容量验证
 * - RGB LED (WS2812) - 颜色循环（需目视检查）
 * - 蜂鸣器 - 音调序列（需听觉检查）
 * - 旋转编码器 - 旋转和按钮检测
 * - BOOT 按钮 (GPIO0) - 按压检测
 * - I2C 总线 - 设备扫描
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "test/factory_test.h"

void app_main(void)
{
    printf("\n\n");
    printf("**** EMMA Factory Test Firmware ****\n");
    printf("Starting in 2 seconds...\n\n");
    vTaskDelay(pdMS_TO_TICKS(2000));

    factory_test_run_all();

    printf("\n\nFactory test completed, system idle.\n");

    /* 保持运行 */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
