#include "endpoints.h"
#include "led.h"
#include "came433.h"
#include "zigbee.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "zcl/esp_zigbee_zcl_basic.h"
#include "zcl/esp_zigbee_zcl_on_off.h"

static const char *TAG = "BUTTONS";

// Manufacturer info (ZCL char strings: length byte + chars)
static uint8_t MANUFACTURER_NAME[] = {16, 'C','e','s','a','r',' ','R','I','C','H','A','R','D',' ','E','I'};
static uint8_t MODEL_IDENTIFIER[] = {12, 'Z','B','4','3','3','-','R','o','u','t','e','r'};


// ====== Endpoint Creation ======
void create_endpoints(void)
{
    // Create endpoint list
    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();

    // Create EP1 (Portail Principal) - manual creation with server role
    esp_zb_cluster_list_t *ep1_clusters = esp_zb_zcl_cluster_list_create();

    // Basic cluster for EP1
    esp_zb_basic_cluster_cfg_t basic_cfg_ep1 = {
        .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
        .power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_DC_SOURCE
    };
    esp_zb_attribute_list_t *basic_attr_list_ep1 = esp_zb_basic_cluster_create(&basic_cfg_ep1);
    esp_zb_basic_cluster_add_attr(basic_attr_list_ep1, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, MANUFACTURER_NAME);
    esp_zb_basic_cluster_add_attr(basic_attr_list_ep1, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, MODEL_IDENTIFIER);
    esp_zb_cluster_list_add_basic_cluster(ep1_clusters, basic_attr_list_ep1, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    // On/Off cluster for EP1 (SERVER role to receive commands)
    esp_zb_on_off_cluster_cfg_t on_off_cfg_ep1 = {.on_off = false};
    esp_zb_attribute_list_t *on_off_attr_list_ep1 = esp_zb_on_off_cluster_create(&on_off_cfg_ep1);
    esp_zb_cluster_list_add_on_off_cluster(ep1_clusters, on_off_attr_list_ep1, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    // Identify cluster for EP1
    esp_zb_identify_cluster_cfg_t identify_cfg_ep1 = {.identify_time = 0};
    esp_zb_attribute_list_t *identify_attr_list_ep1 = esp_zb_identify_cluster_create(&identify_cfg_ep1);
    esp_zb_cluster_list_add_identify_cluster(ep1_clusters, identify_attr_list_ep1, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    // Add EP1 to endpoint list
    esp_zb_endpoint_config_t ep1_config = {
        .endpoint = BUTTON_1_ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_ON_OFF_SWITCH_DEVICE_ID,
        .app_device_version = 0
    };
    esp_zb_ep_list_add_ep(ep_list, ep1_clusters, ep1_config);

    // Create EP2 (Portail Parking) - create cluster list manually and add to ep_list
    esp_zb_cluster_list_t *ep2_clusters = esp_zb_zcl_cluster_list_create();

    // Basic cluster
    esp_zb_basic_cluster_cfg_t basic_cfg = {
        .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
        .power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_DC_SOURCE
    };
    esp_zb_attribute_list_t *basic_attr_list = esp_zb_basic_cluster_create(&basic_cfg);
    esp_zb_basic_cluster_add_attr(basic_attr_list, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, MANUFACTURER_NAME);
    esp_zb_basic_cluster_add_attr(basic_attr_list, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, MODEL_IDENTIFIER);
    esp_zb_cluster_list_add_basic_cluster(ep2_clusters, basic_attr_list, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    // On/Off cluster
    esp_zb_on_off_cluster_cfg_t on_off_cfg = {.on_off = false};
    esp_zb_attribute_list_t *on_off_attr_list = esp_zb_on_off_cluster_create(&on_off_cfg);
    esp_zb_cluster_list_add_on_off_cluster(ep2_clusters, on_off_attr_list, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    // Identify cluster
    esp_zb_identify_cluster_cfg_t identify_cfg = {.identify_time = 0};
    esp_zb_attribute_list_t *identify_attr_list = esp_zb_identify_cluster_create(&identify_cfg);
    esp_zb_cluster_list_add_identify_cluster(ep2_clusters, identify_attr_list, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    // Add EP2 to endpoint list
    esp_zb_endpoint_config_t ep2_config = {
        .endpoint = BUTTON_2_ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_ON_OFF_SWITCH_DEVICE_ID,
        .app_device_version = 0
    };
    esp_zb_ep_list_add_ep(ep_list, ep2_clusters, ep2_config);

    // Register both endpoints
    ESP_LOGI(TAG, "Registering endpoints with Zigbee stack...");
    esp_zb_device_register(ep_list);

    ESP_LOGI(TAG, "Endpoints EP%d and EP%d registered successfully", BUTTON_1_ENDPOINT, BUTTON_2_ENDPOINT);
}

// ====== Button Click Detection ======
void handle_button_click(uint8_t endpoint)
{
    switch (endpoint) {
    case BUTTON_1_ENDPOINT:
        ESP_LOGI(TAG, "Button 1 clicked - Portail Principal");
        led_set_color(0, 128, 255);
        came433_send_portail1();
        break;
    case BUTTON_2_ENDPOINT:
        ESP_LOGI(TAG, "Button 2 clicked - Portail Parking");
        led_set_color(255, 0, 255);
        came433_send_portail2();
        break;
    default:
        ESP_LOGW(TAG, "Unknown button endpoint: %d", endpoint);
        break;
    }
    led_off();
}
