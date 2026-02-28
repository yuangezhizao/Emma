/**
 * @file chip.c
 * @brief ESP32-S3 芯片诊断实现
 *
 * 使用内部温度传感器读取芯片温度，
 * 通过 heap_caps API 获取 SRAM 和 PSRAM 内存信息。
 *
 * 温度传感器量程：-10°C ~ 80°C（典型精度 ±1°C）
 * ESP32-S3 支持片上温度传感器，无需外部硬件。
 */

#include "chip.h"
#include "driver/temperature_sensor.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char* TAG = "hal_chip";

esp_err_t hal_chip_read_temperature(float* temperature_c)
{
    if (temperature_c == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* ── 配置温度传感器 ───────────────────────────────────────── */
    temperature_sensor_handle_t temp_sensor = NULL;
    temperature_sensor_config_t temp_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);

    esp_err_t ret = temperature_sensor_install(&temp_config, &temp_sensor);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install temperature sensor: %s", esp_err_to_name(ret));
        return ret;
    }

    /* ── 使能传感器并读取 ─────────────────────────────────────── */
    ret = temperature_sensor_enable(temp_sensor);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable temperature sensor: %s", esp_err_to_name(ret));
        temperature_sensor_uninstall(temp_sensor);
        return ret;
    }

    ret = temperature_sensor_get_celsius(temp_sensor, temperature_c);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read temperature: %s", esp_err_to_name(ret));
    }

    /* ── 清理：禁用并卸载传感器 ──────────────────────────────── */
    temperature_sensor_disable(temp_sensor);
    temperature_sensor_uninstall(temp_sensor);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Chip temperature: %.1f C", *temperature_c);
    }

    return ret;
}

esp_err_t hal_chip_get_info(hal_chip_info_t* info)
{
    if (info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* ── 读取温度 ─────────────────────────────────────────────── */
    esp_err_t ret = hal_chip_read_temperature(&info->temperature_celsius);
    if (ret != ESP_OK) {
        return ret;
    }

    /* ── 内部 SRAM 堆信息 ─────────────────────────────────────── */
    info->internal_heap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

    /* ── PSRAM (SPI RAM) 信息 ─────────────────────────────────── */
    info->psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    info->psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

    ESP_LOGI(TAG, "Internal heap free: %lu bytes", (unsigned long)info->internal_heap);
    ESP_LOGI(TAG, "PSRAM total: %lu bytes, free: %lu bytes", (unsigned long)info->psram_total,
             (unsigned long)info->psram_free);

    return ESP_OK;
}
