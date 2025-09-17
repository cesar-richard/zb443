#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "zigbee.h"
#include "led.h"
#include "buttons.h"
#include "came433.h"
#include "cc1101.h"

#define TAG "ZB433"

// ====== Main ======
void app_main(void)
{
    ESP_LOGI(TAG, "ZB433 Router starting...");
    // Initialize LED
    ESP_LOGI(TAG, "Starting LED initialization...");
    led_init();
    led_set_color(255, 0, 0); // Rouge fixe au boot
    // Initialize NVS
    ESP_LOGI(TAG, "Initializing NVS...");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGI(TAG, "NVS needs erase, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS initialized successfully");
    
    ESP_LOGI(TAG, "LED initialization completed");

    // Initialize CC1101 (FSK transceiver)
    cc1101_init();
    
    // Initialize CAME 433MHz (legacy OOK path; safe if hardware absent)
    ESP_LOGI(TAG, "Starting CAME 433MHz initialization...");
    came433_init();
    ESP_LOGI(TAG, "CAME 433MHz initialization completed");
    
    // CAME 433MHz is ready with working parameters
    ESP_LOGI(TAG, "CAME 433MHz ready with working parameters!");
    
    // Initialize Zigbee
    ESP_LOGI(TAG, "Starting Zigbee initialization...");
    zigbee_init();
    ESP_LOGI(TAG, "Zigbee initialization completed");
    
    // Wait a bit for Zigbee stack to be ready
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Force commissioning if device is factory new
    if (esp_zb_bdb_is_factory_new()) {
        ESP_LOGI(TAG, "Device is factory new - starting commissioning on channel 25");
        led_zigbee_connecting();
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
    } else {
        ESP_LOGI(TAG, "Device not factory new - checking if joined");
        if (esp_zb_bdb_dev_joined()) {
            ESP_LOGI(TAG, "Device already joined to network");
            led_zigbee_connected();
        } else {
            ESP_LOGI(TAG, "Device not joined - starting commissioning on channel 25");
            led_zigbee_connecting();
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
        }
    }
    
    ESP_LOGI(TAG, "ZB433 Router ready!");
    ESP_LOGI(TAG, "Button 1: EP%d (Portail 1)", BUTTON_1_ENDPOINT);
    ESP_LOGI(TAG, "Button 2: EP%d (Portail 2)", BUTTON_2_ENDPOINT);
    
    // Main loop
    int counter = 0;
    bool zigbee_connected = false;
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "Main loop running... counter=%d", counter++);
        
        // Vérifier si Zigbee est connecté (sécurité)
        if (!zigbee_connected && esp_zb_bdb_dev_joined()) {
            ESP_LOGI(TAG, "Zigbee connected detected in main loop");
            led_zigbee_connected();
            zigbee_connected = true;
        }
    }
}