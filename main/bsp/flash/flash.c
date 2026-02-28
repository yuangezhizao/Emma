/**
 * @file flash.c
 * @brief 外部 SPI Flash 信息读取器
 *
 * 使用 ESP-IDF Flash API 读取主 Flash 芯片信息。
 * 板载 Flash 可能为 W25Q128JVPIQ (16 MB) 或 W25Q256JVPIQ (32 MB)，
 * 连接到 SPI 总线，由 ESP32-S3 的 SPI Flash 控制器管理。
 */

#include "flash.h"
#include "esp_flash.h"
#include "esp_log.h"

static const char* TAG = "hal_flash";

esp_err_t hal_flash_get_info(hal_flash_info_t* info)
{
    if (info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret;

    /* 读取 Flash 容量 */
    uint32_t flash_size = 0;
    ret = esp_flash_get_size(NULL, &flash_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get flash size: %s", esp_err_to_name(ret));
        return ret;
    }
    info->size_bytes = flash_size;

    /* 读取芯片 ID（制造商 + 设备） */
    uint32_t chip_id = 0;
    ret = esp_flash_read_id(NULL, &chip_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read flash chip ID: %s", esp_err_to_name(ret));
        return ret;
    }

    /*
     * chip_id 通常是 24 位 JEDEC ID：
     *   [23:16] = 制造商 ID
     *   [15:0]  = 设备 ID
     * W25Q128：制造商 = 0xEF (Winbond)，设备 = 0x4018
     * W25Q256：制造商 = 0xEF (Winbond)，设备 = 0x4019
     */
    info->manufacturer_id = (chip_id >> 16) & 0xFF;
    info->device_id = chip_id & 0xFFFF;

    ESP_LOGI(TAG, "Flash: size=%lu bytes, manufacturer=0x%02lX, device=0x%04lX", (unsigned long)info->size_bytes,
             (unsigned long)info->manufacturer_id, (unsigned long)info->device_id);

    return ESP_OK;
}
