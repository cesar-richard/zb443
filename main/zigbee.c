#include "zigbee.h"
#include "endpoints.h"
#include "led.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "zcl/esp_zigbee_zcl_command.h"
#include "aps/esp_zigbee_aps.h"
#include "zcl/esp_zigbee_zcl_identify.h"
#include "zcl/esp_zigbee_zcl_on_off.h"

static const char *TAG = "ZIGBEE";

// Timers pour reset l'état on_off après 5 secondes (debounce)
static TimerHandle_t reset_timer_ep1 = NULL;
static TimerHandle_t reset_timer_ep2 = NULL;

// Identify notify (called by stack on Identify start/stop)
static void identify_notify_cb(uint8_t identify_on)
{
    ESP_LOGI(TAG, "Identify notify: %s", identify_on ? "start" : "stop");
    if (identify_on) {
        led_identify_breathe();
    } else {
        led_identify_okay();
    }
}

// Callback pour reset l'attribut on_off à false après 5 secondes
static void reset_on_off_timer_callback(TimerHandle_t xTimer)
{
    uint8_t endpoint = (uint8_t)pvTimerGetTimerID(xTimer);
    
    ESP_LOGI(TAG, "Resetting on_off attribute to false on endpoint %d", endpoint);
    
    // Remettre l'attribut on_off à false
    uint8_t on_off_value = 0; // false
    esp_err_t ret = esp_zb_zcl_set_attribute_val(
        endpoint,
        ESP_ZB_ZCL_CLUSTER_ID_ON_OFF,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID,
        &on_off_value,
        false  // manufacturer specific = false
    );
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reset on_off attribute on endpoint %d: %s", endpoint, esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Successfully reset on_off attribute on endpoint %d", endpoint);
    }
}

// Fonction pour lancer/relancer le timer de reset (avec debounce)
static void start_reset_timer(uint8_t endpoint)
{
    TimerHandle_t *timer = (endpoint == BUTTON_1_ENDPOINT) ? &reset_timer_ep1 : &reset_timer_ep2;
    
    // Si le timer existe déjà, le relancer (debounce)
    if (*timer != NULL) {
        xTimerReset(*timer, 0);
        ESP_LOGD(TAG, "Reset timer restarted for endpoint %d", endpoint);
    } else {
        // Créer le timer s'il n'existe pas encore
        *timer = xTimerCreate(
            endpoint == BUTTON_1_ENDPOINT ? "ResetEP1" : "ResetEP2",
            pdMS_TO_TICKS(5000),  // 5 secondes
            pdFALSE,              // One-shot timer
            (void *)(uintptr_t)endpoint,  // Timer ID = endpoint
            reset_on_off_timer_callback
        );
        
        if (*timer == NULL) {
            ESP_LOGE(TAG, "Failed to create reset timer for endpoint %d", endpoint);
            return;
        }
        
        xTimerStart(*timer, 0);
        ESP_LOGD(TAG, "Reset timer started for endpoint %d", endpoint);
    }
}

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

    // Register Identify notify handlers (per endpoint)
    esp_zb_identify_notify_handler_register(BUTTON_1_ENDPOINT, identify_notify_cb);
    esp_zb_identify_notify_handler_register(BUTTON_2_ENDPOINT, identify_notify_cb);
    
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
    case ESP_ZB_CORE_CMD_CUSTOM_CLUSTER_REQ_CB_ID:
        {
            // Gérer les commandes On/Off (on, off, toggle)
            esp_zb_zcl_custom_cluster_command_message_t *cmd_msg = (esp_zb_zcl_custom_cluster_command_message_t *)message;
            uint8_t endpoint = cmd_msg->info.dst_endpoint;
            uint16_t cluster = cmd_msg->info.cluster;
            uint8_t command_id = cmd_msg->info.command.id;
            
            ESP_LOGI(TAG, "Command received: cluster=0x%04x, cmd=0x%02x, endpoint=%d", 
                     cluster, command_id, endpoint);
            
            // On/Off cluster: react to on/off/toggle commands
            if (cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
                if (command_id == ESP_ZB_ZCL_CMD_ON_OFF_ON_ID || 
                    command_id == ESP_ZB_ZCL_CMD_ON_OFF_TOGGLE_ID) {
                    ESP_LOGI(TAG, "On/Off command (on/toggle) on endpoint %d", endpoint);
                    
                    // Gérer le clic selon l'endpoint
                    if (endpoint == BUTTON_1_ENDPOINT) {
                        ESP_LOGI(TAG, "Button 1 clicked - Portail Principal");
                        handle_button_click(BUTTON_1_ENDPOINT);
                        // Lancer le timer pour reset on_off après 5 secondes (debounce)
                        start_reset_timer(BUTTON_1_ENDPOINT);
                    } else if (endpoint == BUTTON_2_ENDPOINT) {
                        ESP_LOGI(TAG, "Button 2 clicked - Portail Parking");
                        handle_button_click(BUTTON_2_ENDPOINT);
                        // Lancer le timer pour reset on_off après 5 secondes (debounce)
                        start_reset_timer(BUTTON_2_ENDPOINT);
                    } else {
                        ESP_LOGW(TAG, "Unknown endpoint clicked: %d", endpoint);
                    }
                }
            }
        }
        break;
        
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
        {
            esp_zb_zcl_set_attr_value_message_t *attr_msg = (esp_zb_zcl_set_attr_value_message_t *)message;
            uint8_t endpoint = attr_msg->info.dst_endpoint;
            
            ESP_LOGI(TAG, "Attribute write: cluster=0x%04x, attr=0x%04x, endpoint=%d", 
                     attr_msg->info.cluster, attr_msg->attribute.id, endpoint);
            
            // On/Off cluster: react to on_off attribute writes (fallback)
            if (attr_msg->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF &&
                attr_msg->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID) {
                ESP_LOGI(TAG, "On/Off value changed on endpoint %d", endpoint);
                
                // Gérer le clic selon l'endpoint
                if (endpoint == BUTTON_1_ENDPOINT) {
                    ESP_LOGI(TAG, "Button 1 clicked - Portail Principal");
                    handle_button_click(BUTTON_1_ENDPOINT);
                    // Lancer le timer pour reset on_off après 5 secondes (debounce)
                    start_reset_timer(BUTTON_1_ENDPOINT);
                } else if (endpoint == BUTTON_2_ENDPOINT) {
                    ESP_LOGI(TAG, "Button 2 clicked - Portail Parking");
                    handle_button_click(BUTTON_2_ENDPOINT);
                    // Lancer le timer pour reset on_off après 5 secondes (debounce)
                    start_reset_timer(BUTTON_2_ENDPOINT);
                } else {
                    ESP_LOGW(TAG, "Unknown endpoint clicked: %d", endpoint);
                }
            }
            
            // Identify cluster: react to identify_time writes and play LED effect
            if (attr_msg->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY &&
                attr_msg->attribute.id == ESP_ZB_ZCL_ATTR_IDENTIFY_IDENTIFY_TIME_ID) {
                uint16_t identify_seconds = 0;
                if (attr_msg->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16 &&
                    attr_msg->attribute.data.value != NULL) {
                    identify_seconds = *(uint16_t *)attr_msg->attribute.data.value;
                }
                ESP_LOGI(TAG, "Identify requested on EP%d for %u s (attr write)", endpoint, (unsigned)identify_seconds);
                if (identify_seconds > 0) {
                    led_identify_breathe();
                } else {
                    led_identify_okay();
                }
            }
        }
        break;
    default:
        ESP_LOGD(TAG, "Receive Zigbee action(0x%x) callback", callback_id);
        break;
    }
    return ret;
}
