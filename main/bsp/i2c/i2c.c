/**
 * @file i2c.c
 * @brief I2C 主机总线驱动实现（新驱动 API）
 *
 * 使用 ESP-IDF v5.x i2c_master 驱动，通过 i2c_master_probe() 进行总线扫描。
 * 扫描完整的 7 位地址空间 (0x08 - 0x77)。
 */

#include "i2c.h"
#include "bsp/board_def.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

static const char* TAG = "hal_i2c";

static i2c_master_bus_handle_t s_bus_handle = NULL;

esp_err_t hal_i2c_init(void)
{
    if (s_bus_handle != NULL) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_PORT_NUM,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&bus_cfg, &s_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C master bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "I2C master initialized: SDA=GPIO%d, SCL=GPIO%d, freq=%dHz", PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ_HZ);
    return ESP_OK;
}

esp_err_t hal_i2c_scan(uint8_t* found_addrs, uint8_t max_addrs, uint8_t* num_found)
{
    if (s_bus_handle == NULL || found_addrs == NULL || num_found == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    *num_found = 0;

    ESP_LOGI(TAG, "Scanning I2C bus...");

    for (uint16_t addr = 0x08; addr <= 0x77; addr++) {
        esp_err_t ret = i2c_master_probe(s_bus_handle, addr, 50);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "  Found device at 0x%02X", addr);
            if (*num_found < max_addrs) {
                found_addrs[*num_found] = (uint8_t)addr;
            }
            (*num_found)++;
        }
    }

    ESP_LOGI(TAG, "I2C scan complete: %d device(s) found", *num_found);
    return ESP_OK;
}

esp_err_t hal_i2c_deinit(void)
{
    if (s_bus_handle == NULL) {
        return ESP_OK;
    }

    esp_err_t ret = i2c_del_master_bus(s_bus_handle);
    s_bus_handle = NULL;

    ESP_LOGI(TAG, "I2C master deinitialized");
    return ret;
}
