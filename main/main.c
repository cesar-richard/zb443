#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "zigbee.h"
#include "led.h"
#include "endpoints.h"
#include "came433.h"

#define TAG "ZB433"

void app_main(void)
{
    ESP_LOGI(TAG, "ZB433 Router starting...");
    
    // Initialize LED
    led_init();
    led_set_color(255, 0, 0);

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    came433_init();
    zigbee_init();
    
    // Wait for Zigbee stack to be ready
    vTaskDelay(pdMS_TO_TICKS(1000));

    led_set_color(255, 64, 0);
    
    // Force factory reset if not joined (uncomment to force reset)
    // ESP_LOGI(TAG, "Forcing factory reset...");
    // esp_zb_factory_reset();
    // vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Start commissioning if needed
    if (esp_zb_bdb_is_factory_new()) {
        ESP_LOGI(TAG, "Device is factory new - starting commissioning");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
    } else if (esp_zb_bdb_dev_joined()) {
        ESP_LOGI(TAG, "Device already joined to network");
    } else {
        ESP_LOGI(TAG, "Device not joined - starting commissioning");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
    }
    
    // Main loop
    bool zigbee_connected = false;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        
        // Check Zigbee connection status
        if (!zigbee_connected && esp_zb_bdb_dev_joined()) {
            ESP_LOGI(TAG, "Zigbee connected");
            led_off();
            zigbee_connected = true;
        }
    }
}