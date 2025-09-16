#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "driver/gpio.h"
#include "esp_zigbee_core.h"
#include "zcl/esp_zigbee_zcl_core.h"
#include "esp_zigbee_cluster.h"
#include "esp_zigbee_attribute.h"
// #include "esp_app_update.h"  // Not needed, functions are in esp_ota_ops.h

#define TAG "ZB433"

// ====== Hardware pin ======
#define TX433_GPIO GPIO_NUM_4   // DATA vers TX433 (adapter si besoin)

// ====== CAME keys ======
#define KEY_A  0x03B29B  // porte A (03 B2 9B)
#define KEY_B  0x03B29A  // porte B (03 B2 9A)

// ====== Timings (µs) - à ajuster si besoin ======
#define T_SHORT 350
#define T_LONG  1050
#define GAP     8000

// ====== OTA ======
#define DEFAULT_OTA_URL "http://homeassistant.local:8123/local/zb433.bin"
#define OTA_URL_MAX   256

static char ota_url_buf[OTA_URL_MAX];

// ====== CAME-24 encoder ======
static void tx_came24_send(uint32_t key, uint8_t repeats)
{
    ESP_LOGI(TAG, "CAME-24: sending key 0x%06" PRIx32 " (%d repeats)", key, repeats);
    
    // Configure GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << TX433_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    
    for (uint8_t r = 0; r < repeats; r++) {
        // Send 24 bits
        for (int8_t bit = 23; bit >= 0; bit--) {
            bool bit_val = (key >> bit) & 1;
            
            if (bit_val) {
                // Bit 1: short high + long low
                gpio_set_level(TX433_GPIO, 1);
                vTaskDelay(pdMS_TO_TICKS(T_SHORT / 1000));
                gpio_set_level(TX433_GPIO, 0);
                vTaskDelay(pdMS_TO_TICKS(T_LONG / 1000));
        } else {
                // Bit 0: long high + short low
                gpio_set_level(TX433_GPIO, 1);
                vTaskDelay(pdMS_TO_TICKS(T_LONG / 1000));
                gpio_set_level(TX433_GPIO, 0);
                vTaskDelay(pdMS_TO_TICKS(T_SHORT / 1000));
            }
        }
        
        // Gap between repeats
        if (r < repeats - 1) {
            gpio_set_level(TX433_GPIO, 0);
            vTaskDelay(pdMS_TO_TICKS(GAP / 1000));
        }
    }
    
    // Ensure GPIO is low
    gpio_set_level(TX433_GPIO, 0);
}

// ====== Actions ======
static void action_open(void)  
{ 
    ESP_LOGI(TAG, "CAME: OPEN A (key 0x%06X)", KEY_A); 
    tx_came24_send(KEY_A, 8); 
}

static void action_open2(void) 
{ 
    ESP_LOGI(TAG, "CAME: OPEN B (key 0x%06X)", KEY_B); 
    tx_came24_send(KEY_B, 8); 
}

static void action_factory_reset(void)
{
    ESP_LOGI(TAG, "Factory Reset: erasing NVS...");
    nvs_flash_erase();
    ESP_LOGI(TAG, "Factory Reset: restarting...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
}

// ====== OTA ======
static esp_err_t nvs_set_ota_url(const char *url)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("zb433", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) return err;
    
    err = nvs_set_str(nvs_handle, "ota_url", url);
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
    }
    nvs_close(nvs_handle);
    return err;
}

static esp_err_t nvs_get_ota_url(char *url, size_t max_len)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("zb433", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) return err;
    
    size_t required_size = max_len;
    err = nvs_get_str(nvs_handle, "ota_url", url, &required_size);
    nvs_close(nvs_handle);
    return err;
}

static void do_ota_sequence(void)
{
    char *ota_url = ota_url_buf;
    
    // Get OTA URL from NVS or use default
    if (nvs_get_ota_url(ota_url, sizeof(ota_url_buf)) != ESP_OK) {
        strlcpy(ota_url, DEFAULT_OTA_URL, sizeof(ota_url_buf));
    }
    
    ESP_LOGI(TAG, "Starting OTA from: %s", ota_url);
    
    esp_http_client_config_t config = {
        .url = ota_url,
        .cert_pem = NULL,
        .timeout_ms = 30000,
    };
    
    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };
    
    esp_err_t ret = esp_https_ota(&ota_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA successful, restarting...");
        esp_restart();
    } else {
        ESP_LOGE(TAG, "OTA failed: %s", esp_err_to_name(ret));
    }
}

// ====== Test functions ======
static void test_came24(void)
{
    ESP_LOGI(TAG, "Testing CAME-24 transmission...");
    action_open();
    vTaskDelay(pdMS_TO_TICKS(2000));
    action_open2();
}

static void test_ota(void)
{
    ESP_LOGI(TAG, "Testing OTA sequence...");
    do_ota_sequence();
}

static void test_factory_reset(void)
{
    ESP_LOGI(TAG, "Testing Factory Reset...");
    action_factory_reset();
}

// ====== Main ======
void app_main(void)
{
    ESP_LOGI(TAG, "ZB433 Router starting...");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize default OTA URL
    strlcpy(ota_url_buf, DEFAULT_OTA_URL, sizeof(ota_url_buf));
    
    ESP_LOGI(TAG, "ZB433 Router ready!");
    ESP_LOGI(TAG, "Available test functions:");
    ESP_LOGI(TAG, "  - test_came24()");
    ESP_LOGI(TAG, "  - test_ota()");
    ESP_LOGI(TAG, "  - test_factory_reset()");
    
    // Test CAME-24 transmission
    vTaskDelay(pdMS_TO_TICKS(2000));
    test_came24();
    
    // Keep running
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "ZB433 Router running...");
    }
}
