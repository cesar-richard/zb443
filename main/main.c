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
    led_off();

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize CAME 433MHz transmitter
    ESP_LOGI(TAG, "Initializing CAME 433MHz transmitter...");
    came433_init();
    ESP_LOGI(TAG, "CAME 433MHz transmitter ready!");
    
    // Initialize Zigbee
    ESP_LOGI(TAG, "Initializing Zigbee...");
    zigbee_init();
    ESP_LOGI(TAG, "Zigbee initialized");
    
    // Wait for Zigbee stack to be ready
    vTaskDelay(pdMS_TO_TICKS(500));
    
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
    
    ESP_LOGI(TAG, "ZB433 Router ready!");
    ESP_LOGI(TAG, "Button 1: EP%d (Portail Principal)", BUTTON_1_ENDPOINT);
    ESP_LOGI(TAG, "Button 2: EP%d (Portail Parking)", BUTTON_2_ENDPOINT);
    
    // Main loop
    bool zigbee_connected = false;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        
        // Check Zigbee connection status
        if (!zigbee_connected && esp_zb_bdb_dev_joined()) {
            ESP_LOGI(TAG, "Zigbee connected");
            zigbee_connected = true;
        }
    }
}