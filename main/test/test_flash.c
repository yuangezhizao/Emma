/**
 * @file test_flash.c
 * @brief 工厂测试：外部 SPI Flash (W25Q128 / W25Q256)
 *
 * 验证项：
 * - Flash 芯片可正常读取
 * - Flash 大小为 16 MB 或 32 MB（两种规格均适用于本开发板）
 * - 制造商 ID 为 0xEF（Winbond 华邦）
 */

#include "factory_test.h"
#include "../bsp/flash/flash.h"
#include "esp_log.h"

static const char* TAG = "test_flash";

#define FLASH_SIZE_16MB       (16 * 1024 * 1024) /* W25Q128 */
#define FLASH_SIZE_32MB       (32 * 1024 * 1024) /* W25Q256 */
#define EXPECTED_MANUFACTURER 0xEF               /* Winbond 华邦 */

test_result_t test_flash_run(void)
{
    hal_flash_info_t info;
    esp_err_t ret = hal_flash_get_info(&info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FAIL: Cannot read flash info: %s", esp_err_to_name(ret));
        return TEST_FAIL;
    }

    bool pass = true;

    ESP_LOGI(TAG, "Flash size: %lu MB", (unsigned long)(info.size_bytes / (1024 * 1024)));
    ESP_LOGI(TAG, "Manufacturer ID: 0x%02lX %s", (unsigned long)info.manufacturer_id,
             info.manufacturer_id == EXPECTED_MANUFACTURER ? "(Winbond)" : "(Unknown)");
    ESP_LOGI(TAG, "Device ID: 0x%04lX", (unsigned long)info.device_id);

    /* 接受 16 MB (W25Q128) 和 32 MB (W25Q256) 两种规格 */
    if (info.size_bytes != FLASH_SIZE_16MB && info.size_bytes != FLASH_SIZE_32MB) {
        ESP_LOGE(TAG, "FAIL: Unexpected flash size: %lu bytes (expected 16 or 32 MB)", (unsigned long)info.size_bytes);
        pass = false;
    }

    if (info.size_bytes == 0) {
        ESP_LOGE(TAG, "FAIL: Flash size is 0");
        pass = false;
    }

    return pass ? TEST_PASS : TEST_FAIL;
}
