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

    // Commissioning is now handled automatically by the signal handler
    ESP_LOGI(TAG, "Zigbee stack started - commissioning handled by signal handler");

    // Main loop - connection status is logged by signal handler
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}