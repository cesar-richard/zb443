#include "endpoints.h"
#include "led.h"
#include "came433.h"
#include "zigbee.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "zcl/esp_zigbee_zcl_basic.h"

static const char *TAG = "BUTTONS";

// Basic cluster strings (ZCL char strings: length + bytes)
static uint8_t BASIC_MANUFACTURER[] = {16, 'C','e','s','a','r', ' ', 'R', 'I', 'C', 'H', 'A', 'R', 'D', ' ', 'E', 'I'};
static uint8_t BASIC_MODEL[] = {12, 'Z','B','4','3','3','-','R','o','u','t','e','r'};
static uint8_t BASIC_EP1_LABEL[] = {17, 'P','o','r','t','a','i','l',' ','P','r','i','n','c','i','p','a','l'};
static uint8_t BASIC_EP2_LABEL[] = {15, 'P','o','r','t','a','i','l',' ','P','a','r','k','i','n','g'};
static uint8_t BASIC_SW_BUILD[] = {5, '1','.','0','.','0'}; // softwareBuildId = "1.0.0"
static uint8_t BASIC_EMPTY_LABEL[] = {0};

// ====== Button Clusters ======
esp_zb_cluster_list_t *create_button_clusters(void)
{
    esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
    
    // Basic cluster for button
    esp_zb_basic_cluster_cfg_t basic_cfg = {
        .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
        .power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_DC_SOURCE
    };
    
    esp_zb_attribute_list_t *basic_attr_list = esp_zb_basic_cluster_create(&basic_cfg);
    // Add optional Basic attributes so Z2M can read them
    esp_zb_basic_cluster_add_attr(basic_attr_list, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, BASIC_MANUFACTURER);
    esp_zb_basic_cluster_add_attr(basic_attr_list, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, BASIC_MODEL);
    esp_zb_basic_cluster_add_attr(basic_attr_list, ESP_ZB_ZCL_ATTR_BASIC_SW_BUILD_ID, BASIC_SW_BUILD);
    esp_zb_basic_cluster_add_attr(basic_attr_list, ESP_ZB_ZCL_ATTR_BASIC_PRODUCT_LABEL_ID, BASIC_EMPTY_LABEL);
    esp_zb_basic_cluster_add_attr(basic_attr_list, ESP_ZB_ZCL_ATTR_BASIC_LOCATION_DESCRIPTION_ID, BASIC_EMPTY_LABEL);
    esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_attr_list, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    
    // On/Off cluster for button (to accept ON commands from Z2M)
    esp_zb_on_off_cluster_cfg_t on_off_cfg = {
        .on_off = false
    };

    esp_zb_attribute_list_t *on_off_attr_list = esp_zb_on_off_cluster_create(&on_off_cfg);
    esp_zb_cluster_list_add_on_off_cluster(cluster_list, on_off_attr_list, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    
    // Identify cluster for EP1
    esp_zb_identify_cluster_cfg_t identify_cfg = {
        .identify_time = 0
    };
    esp_zb_attribute_list_t *identify_attr_list = esp_zb_identify_cluster_create(&identify_cfg);
    esp_zb_cluster_list_add_identify_cluster(cluster_list, identify_attr_list, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    
    return cluster_list;
}

// Helper: cluster list without Identify (for EP2)
static esp_zb_cluster_list_t *create_button_clusters_no_identify(void)
{
    esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();

    // Basic cluster
    esp_zb_basic_cluster_cfg_t basic_cfg = {
        .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
        .power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_DC_SOURCE
    };
    esp_zb_attribute_list_t *basic_attr_list = esp_zb_basic_cluster_create(&basic_cfg);
    esp_zb_basic_cluster_add_attr(basic_attr_list, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, BASIC_MANUFACTURER);
    esp_zb_basic_cluster_add_attr(basic_attr_list, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, BASIC_MODEL);
    esp_zb_basic_cluster_add_attr(basic_attr_list, ESP_ZB_ZCL_ATTR_BASIC_SW_BUILD_ID, BASIC_SW_BUILD);
    esp_zb_basic_cluster_add_attr(basic_attr_list, ESP_ZB_ZCL_ATTR_BASIC_PRODUCT_LABEL_ID, BASIC_EMPTY_LABEL);
    esp_zb_basic_cluster_add_attr(basic_attr_list, ESP_ZB_ZCL_ATTR_BASIC_LOCATION_DESCRIPTION_ID, BASIC_EMPTY_LABEL);
    esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_attr_list, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    // On/Off cluster
    esp_zb_on_off_cluster_cfg_t on_off_cfg = { .on_off = false };
    esp_zb_attribute_list_t *on_off_attr_list = esp_zb_on_off_cluster_create(&on_off_cfg);
    esp_zb_cluster_list_add_on_off_cluster(cluster_list, on_off_attr_list, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    return cluster_list;
}

// ====== Endpoint Creation ======
void create_endpoints(void)
{
    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();
    
    // Button 1 endpoint (On/Off Switch - Portail Principal) with Identify
    esp_zb_cluster_list_t *button1_clusters = create_button_clusters();
    esp_zb_endpoint_config_t button1_config = {
        .endpoint = BUTTON_1_ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_ON_OFF_SWITCH_DEVICE_ID,
        .app_device_version = 1
    };
    esp_zb_ep_list_add_ep(ep_list, button1_clusters, button1_config);
    
    // Button 2 endpoint (On/Off Switch - Portail Parking) without Identify
    esp_zb_cluster_list_t *button2_clusters = create_button_clusters_no_identify();
    esp_zb_endpoint_config_t button2_config = {
        .endpoint = BUTTON_2_ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_ON_OFF_SWITCH_DEVICE_ID,
        .app_device_version = 1
    };
    esp_zb_ep_list_add_ep(ep_list, button2_clusters, button2_config);
    
    // Register all endpoints
    ESP_LOGI(TAG, "Registering endpoints with Zigbee stack...");
    esp_zb_device_register(ep_list);

    // Set Basic cluster strings (manufacturer/model common, labels per endpoint)
    (void)esp_zb_zcl_set_attribute_val(BUTTON_1_ENDPOINT,
                                       ESP_ZB_ZCL_CLUSTER_ID_BASIC,
                                       ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                                       ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID,
                                       BASIC_MANUFACTURER,
                                       true);
    (void)esp_zb_zcl_set_attribute_val(BUTTON_1_ENDPOINT,
                                       ESP_ZB_ZCL_CLUSTER_ID_BASIC,
                                       ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                                       ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID,
                                       BASIC_MODEL,
                                       true);
    (void)esp_zb_zcl_set_attribute_val(BUTTON_1_ENDPOINT,
                                       ESP_ZB_ZCL_CLUSTER_ID_BASIC,
                                       ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                                       ESP_ZB_ZCL_ATTR_BASIC_SW_BUILD_ID,
                                       BASIC_SW_BUILD,
                                       true);
    (void)esp_zb_zcl_set_attribute_val(BUTTON_1_ENDPOINT,
                                       ESP_ZB_ZCL_CLUSTER_ID_BASIC,
                                       ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                                       ESP_ZB_ZCL_ATTR_BASIC_PRODUCT_LABEL_ID,
                                       BASIC_EP1_LABEL,
                                       true);
    (void)esp_zb_zcl_set_attribute_val(BUTTON_1_ENDPOINT,
                                       ESP_ZB_ZCL_CLUSTER_ID_BASIC,
                                       ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                                       ESP_ZB_ZCL_ATTR_BASIC_LOCATION_DESCRIPTION_ID,
                                       BASIC_EP1_LABEL,
                                       true);

    (void)esp_zb_zcl_set_attribute_val(BUTTON_2_ENDPOINT,
                                       ESP_ZB_ZCL_CLUSTER_ID_BASIC,
                                       ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                                       ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID,
                                       BASIC_MANUFACTURER,
                                       true);
    (void)esp_zb_zcl_set_attribute_val(BUTTON_2_ENDPOINT,
                                       ESP_ZB_ZCL_CLUSTER_ID_BASIC,
                                       ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                                       ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID,
                                       BASIC_MODEL,
                                       true);
    (void)esp_zb_zcl_set_attribute_val(BUTTON_2_ENDPOINT,
                                       ESP_ZB_ZCL_CLUSTER_ID_BASIC,
                                       ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                                       ESP_ZB_ZCL_ATTR_BASIC_SW_BUILD_ID,
                                       BASIC_SW_BUILD,
                                       true);
    (void)esp_zb_zcl_set_attribute_val(BUTTON_2_ENDPOINT,
                                       ESP_ZB_ZCL_CLUSTER_ID_BASIC,
                                       ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                                       ESP_ZB_ZCL_ATTR_BASIC_PRODUCT_LABEL_ID,
                                       BASIC_EP2_LABEL,
                                       true);
    (void)esp_zb_zcl_set_attribute_val(BUTTON_2_ENDPOINT,
                                       ESP_ZB_ZCL_CLUSTER_ID_BASIC,
                                       ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                                       ESP_ZB_ZCL_ATTR_BASIC_LOCATION_DESCRIPTION_ID,
                                       BASIC_EP2_LABEL,
                                       true);
    
    ESP_LOGI(TAG, "Created Zigbee endpoints:");
    ESP_LOGI(TAG, "  - Button 1: EP%d (Portail Principal)", BUTTON_1_ENDPOINT);
    ESP_LOGI(TAG, "  - Button 2: EP%d (Portail Parking)", BUTTON_2_ENDPOINT);
    ESP_LOGI(TAG, "Endpoints registered successfully");
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
