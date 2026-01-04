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
// Match working example pattern: all initialization inside task + blocking main loop
static void zigbee_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Starting Zigbee task...");

    // Initialize Zigbee stack with standard router config (like working example)
    esp_zb_cfg_t zb_cfg = {
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ROUTER,
        .install_code_policy = false,
        .nwk_cfg.zczr_cfg = {
            .max_children = 10,
        },
    };
    esp_zb_init(&zb_cfg);

    // Create and register endpoints
    create_endpoints();

    // Register Identify notify handler for both endpoints
    esp_zb_identify_notify_handler_register(BUTTON_1_ENDPOINT, identify_notify_cb);
    esp_zb_identify_notify_handler_register(BUTTON_2_ENDPOINT, identify_notify_cb);

    // Register action handler
    esp_zb_core_action_handler_register(zb_action_handler);

    // Set channels to scan for networks (use same mask as working example)
    esp_zb_set_primary_network_channel_set(ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK);

    // Start Zigbee stack with autostart=false (commissioning via signal handler)
    ESP_LOGI(TAG, "Starting Zigbee stack (autostart=false)");
    ESP_ERROR_CHECK(esp_zb_start(false));

    // Run blocking main loop (like working example)
    ESP_LOGI(TAG, "Entering Zigbee main loop (blocking)");
    esp_zb_stack_main_loop();
}

// ====== Zigbee Initialization ======
void zigbee_init(void)
{
    ESP_LOGI(TAG, "Initializing Zigbee router...");

    // Enable verbose logging for debugging
    esp_log_level_set("*", ESP_LOG_DEBUG);
    esp_log_level_set("ZIGBEE", ESP_LOG_DEBUG);

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

    // Create Zigbee task (all stack init happens inside task, like working example)
    xTaskCreate(zigbee_task, "Zigbee_main", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Zigbee task created");
}

// ====== Zigbee Callback ======
static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    if (esp_zb_bdb_start_top_level_commissioning(mode_mask) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start Zigbee commissioning");
    }
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;

    ESP_LOGI(TAG, "=== Zigbee Signal: %s (0x%x), status=%s ===",
             esp_zb_zdo_signal_to_string(sig_type), sig_type, esp_err_to_name(err_status));

    switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Initialize Zigbee stack");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;

    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Device started up in%s factory-reset mode", esp_zb_bdb_is_factory_new() ? "" : " non");
            if (esp_zb_bdb_is_factory_new()) {
                ESP_LOGI(TAG, "Start network steering");
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            } else {
                ESP_LOGI(TAG, "Device rebooted");
            }
        } else {
            ESP_LOGW(TAG, "Device start failed with status: %s, retrying", esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb,
                                   ESP_ZB_BDB_MODE_INITIALIZATION, 1000);
        }
        break;

    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK) {
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            ESP_LOGI(TAG, "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
                     extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                     extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                     esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
        } else {
            ESP_LOGI(TAG, "Network steering was not successful (status: %s), retrying", esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb,
                                   ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        }
        break;

    case ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS:
        if (err_status == ESP_OK) {
            if (*(uint8_t *)esp_zb_app_signal_get_params(p_sg_p)) {
                ESP_LOGI(TAG, "Network(0x%04hx) is open for %d seconds",
                        esp_zb_get_pan_id(), *(uint8_t *)esp_zb_app_signal_get_params(p_sg_p));
            } else {
                ESP_LOGW(TAG, "Network(0x%04hx) closed, devices joining not allowed.", esp_zb_get_pan_id());
            }
        }
        break;

    case ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE:
        ESP_LOGI(TAG, "Device announced to network");
        break;

    case ESP_ZB_COMMON_SIGNAL_CAN_SLEEP:
        ESP_LOGD(TAG, "Can sleep signal");
        break;

    default:
        ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s",
                esp_zb_zdo_signal_to_string(sig_type), sig_type, esp_err_to_name(err_status));
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
