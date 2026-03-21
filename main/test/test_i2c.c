/**
 * @file test_i2c.c
 * @brief 工厂测试：I2C 总线扫描
 *
 * 扫描 I2C 总线并报告发现的设备。
 * 注意：编码器和 I2C 共用部分引脚（GPIO2/GPIO3）。
 * 本测试独立于编码器初始化 I2C。
 * 如果没有连接外部 I2C 设备，找到 0 个设备是正常的。
 */

#include "factory_test.h"
#include "../bsp/i2c/i2c.h"
#include "esp_log.h"

static const char* TAG = "test_i2c";

#define MAX_I2C_DEVICES 16

test_result_t test_i2c_run(void)
{
    esp_err_t ret = hal_i2c_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FAIL: I2C init failed: %s", esp_err_to_name(ret));
        return TEST_FAIL;
    }

    uint8_t addrs[MAX_I2C_DEVICES];
    uint8_t num_found = 0;

    ret = hal_i2c_scan(addrs, MAX_I2C_DEVICES, &num_found);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FAIL: I2C scan failed: %s", esp_err_to_name(ret));
        hal_i2c_deinit();
        return TEST_FAIL;
    }

    if (num_found > 0) {
        ESP_LOGI(TAG, "Found %d I2C device(s):", num_found);
        for (int i = 0; i < num_found && i < MAX_I2C_DEVICES; i++) {
            ESP_LOGI(TAG, "  Address: 0x%02X", addrs[i]);
        }
    } else {
        ESP_LOGI(TAG, "No I2C devices found (this is OK if none are connected)");
    }

    hal_i2c_deinit();

    /* I2C 总线初始化成功即为通过标准。
     * 总线上的设备是可选的。 */
    ESP_LOGI(TAG, "PASS: I2C bus scan completed");
    return TEST_PASS;
}
