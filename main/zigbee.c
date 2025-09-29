#include "zigbee.h"
#include "endpoints.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "zcl/esp_zigbee_zcl_command.h"
#include "aps/esp_zigbee_aps.h"

static const char *TAG = "ZIGBEE";

// ====== Zigbee Task ======
static void zigbee_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Starting Zigbee main loop...");
    
    while (1) {
        esp_zb_stack_main_loop_iteration();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ====== Zigbee Initialization ======
void zigbee_init(void)
{
    ESP_LOGI(TAG, "Initializing Zigbee router...");
    
    // Platform configuration for ESP32-C6 (native Zigbee)
    esp_zb_platform_config_t config = {
        .radio_config = {
            .radio_mode = ZB_RADIO_MODE_NATIVE,
        },
        .host_config = {
            .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE,
        }
    };
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));
    
    // Set Zigbee stack parameters BEFORE init (as per doc)
    esp_zb_overall_network_size_set(64);  // Default network size
    esp_zb_io_buffer_size_set(80);        // Default I/O buffer size
    esp_zb_scheduler_queue_size_set(80);  // Default scheduler queue size
    
    // Initialize Zigbee stack with router config
    esp_zb_cfg_t zb_cfg = {
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ROUTER,
        .install_code_policy = false,
        .nwk_cfg = {
            .zczr_cfg = {
                .max_children = 20
            }
        }
    };
    esp_zb_init(&zb_cfg);
    
    // Set specific channel 25 (your Z2M network channel)
    esp_zb_set_primary_network_channel_set(1 << 25); // Channel 25 only
    
    // Create endpoints
    create_endpoints();
    
    // Register action handler
    esp_zb_core_action_handler_register(zb_action_handler);
    
    // Start Zigbee stack
    ESP_ERROR_CHECK(esp_zb_start(false));
    
    // Create Zigbee task
    xTaskCreate(zigbee_task, "Zigbee_main", 4096, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "Zigbee router initialized successfully");
}

// ====== Zigbee Callback ======
void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    esp_zb_app_signal_type_t sig_type = *(esp_zb_app_signal_type_t *)signal_struct->p_app_signal;
    esp_err_t status = signal_struct->esp_err_status;
    
    switch (sig_type) {
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
        if (status == ESP_OK) {
            ESP_LOGI(TAG, "Zigbee: Device first start - starting commissioning");
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
        } else {
            ESP_LOGE(TAG, "Zigbee: Failed to start commissioning: %s", esp_err_to_name(status));
        }
        break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (status == ESP_OK) {
            ESP_LOGI(TAG, "Zigbee: Steering completed - device joined network");
        } else {
            ESP_LOGE(TAG, "Zigbee: Steering failed: %s", esp_err_to_name(status));
        }
        break;
    case ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE:
        ESP_LOGI(TAG, "Zigbee: Device announced to network");
        break;
    case ESP_ZB_ZDO_SIGNAL_LEAVE_INDICATION:
        ESP_LOGI(TAG, "Zigbee: Device left network");
        break;
    default:
        ESP_LOGD(TAG, "Zigbee: Unhandled signal %d, status: %s", sig_type, esp_err_to_name(status));
        break;
    }
}

// ====== Zigbee Handlers ======
esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message)
{
    esp_err_t ret = ESP_OK;
    switch (callback_id) {
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
        // Handle attribute changes (button clicks)
        ESP_LOGI(TAG, "Zigbee attribute changed - button click detected");
        
        // Détecter quel endpoint a été cliqué
        esp_zb_zcl_set_attr_value_message_t *attr_msg = (esp_zb_zcl_set_attr_value_message_t *)message;
        uint8_t endpoint = attr_msg->info.dst_endpoint;
        
        ESP_LOGI(TAG, "Button clicked on endpoint %d", endpoint);
        
        // Gérer le clic selon l'endpoint
        if (endpoint == BUTTON_1_ENDPOINT) {
            ESP_LOGI(TAG, "Button 1 clicked - Portail Principal");
            handle_button_click(BUTTON_1_ENDPOINT);
        } else if (endpoint == BUTTON_2_ENDPOINT) {
            ESP_LOGI(TAG, "Button 2 clicked - Portail Parking");
            handle_button_click(BUTTON_2_ENDPOINT);
        } else {
            ESP_LOGW(TAG, "Unknown endpoint clicked: %d", endpoint);
        }
        break;
    default:
        ESP_LOGD(TAG, "Receive Zigbee action(0x%x) callback", callback_id);
        break;
    }
    return ret;
}
