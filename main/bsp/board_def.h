/**
 * @file board_def.h
 * @brief Emma 迷你 HMI 核心板 (ESP32-S3) 引脚定义
 *
 * 引脚映射来自原理图和参考 BSP 项目验证
 */

#pragma once

#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── USB 接口 ────────────────────────────────────────────────────────── */
#define PIN_USB_DN GPIO_NUM_19
#define PIN_USB_DP GPIO_NUM_20

/* ── BOOT / 下载按钮（低电平有效，直接触发 BOOT 模式） ─────────────── */
#define PIN_BOOT_KEY GPIO_NUM_0

/* ── RGB LED（WS2812，数据线） ───────────────────────────────────────── */
#define PIN_RGB_DATA GPIO_NUM_1

/* ── 旋转编码器（低电平有效，内部上拉） ──────────────────────────────── */
/* 引脚映射已通过参考 BSP 项目验证（Emma_test 项目）                      */
#define PIN_ENCODER_A   GPIO_NUM_18
#define PIN_ENCODER_B   GPIO_NUM_3
#define PIN_ENCODER_BTN GPIO_NUM_2

/* ── I2C 总线（通过排针 J1/J2 引出，与编码器引脚共用） ───────────────── */
#define PIN_I2C_SDA  GPIO_NUM_2
#define PIN_I2C_SCL  GPIO_NUM_3
#define I2C_PORT_NUM I2C_NUM_0
#define I2C_FREQ_HZ  100000

/* ── LCD（SPI 接口 - 未焊接，引脚已验证） ────────────────────────────── */
/* 引脚映射已通过参考 BSP 项目验证（Emma_test 项目）                      */
#define PIN_LCD_RST GPIO_NUM_17
#define PIN_LCD_SDA GPIO_NUM_15 /* MOSI */
#define PIN_LCD_SCL GPIO_NUM_13 /* SCLK */
#define PIN_LCD_CS  GPIO_NUM_21
#define PIN_LCD_DC  GPIO_NUM_16
#define PIN_LCD_BL  GPIO_NUM_14

/* ── 蜂鸣器（通过 AO3400A N-MOSFET 驱动，栅极高电平有效） ───────────── */
/* 引脚映射已通过参考 BSP 项目验证（Emma_test 项目）                      */
#define PIN_BUZZER GPIO_NUM_46

/* ── 外部 SPI Flash W25Q128JVPIQ（由 ESP32-S3 SPI 控制器直接管理，     */
/*    用于 PSRAM/Flash 扩展）                                            */
/* 这些引脚由 SPI Flash 子系统管理，此处仅作文档记录用途。                */
#define PIN_SPI_CS0  GPIO_NUM_NC /* SPICS0 - 由硬件管理 */
#define PIN_SPI_CLK  GPIO_NUM_NC /* SPICLK */
#define PIN_SPI_MOSI GPIO_NUM_NC /* SPID   */
#define PIN_SPI_MISO GPIO_NUM_NC /* SPIQ   */

/* ── 状态 LED（LED2，电源指示灯，无需 GPIO 控制） ────────────────────── */
/* LED2 通过限流电阻 R7 连接在 VDD3V3 和 GPIO 之间，位于原理图 GPIO      */
/* 区域附近。从布局来看，它是一个电源指示 LED，上电即亮，无需 GPIO 控制。 */

/* ── 晶振 ────────────────────────────────────────────────────────────── */
/* 40 MHz 外部晶振连接在 XTAL_P / XTAL_N（GPIO48/GPIO_XTAL 引脚）      */

#ifdef __cplusplus
}
#endif
