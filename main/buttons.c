#include "buttons.h"
#include "led.h"
#include "came433.h"
#include "zigbee.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "zcl/esp_zigbee_zcl_binary_input.h"
#include "zcl/esp_zigbee_zcl_basic.h"

static const char *TAG = "BUTTONS";

static uint8_t BI_ACTIVE_TEXT[] = {12, 'T','r','a','n','s','m','i','s','s','i','o','n'};
static uint8_t BI_INACTIVE_TEXT[] = {4, 'I','d','l','e'};
static uint8_t BI_DESCRIPTION[] = {6, '4','3','3',' ','T','X'};
static uint32_t BI_APP_TYPE = ESP_ZB_ZCL_BI_SET_APP_TYPE_WITH_ID(ESP_ZB_ZCL_BI_APP_TYPE_OTHER, 0x0001);

// Basic cluster strings (ZCL char strings: length + bytes)
static uint8_t BASIC_MANUFACTURER[] = {5, 'c','e','s','a','r'};
static uint8_t BASIC_MODEL[] = {13, 'z','b','4','3','3','-','e','s','p','3','2','c','6'};
static uint8_t BASIC_EP1_LABEL[] = {7, 'P','o','r','t','e',' ','A'};
static uint8_t BASIC_EP2_LABEL[] = {7, 'P','o','r','t','e',' ','B'};

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
    esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_attr_list, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    
    // On/Off cluster for button (to detect clicks)
    esp_zb_on_off_cluster_cfg_t on_off_cfg = {
        .on_off = false
    };

    esp_zb_attribute_list_t *on_off_attr_list = esp_zb_on_off_cluster_create(&on_off_cfg);
    esp_zb_cluster_list_add_on_off_cluster(cluster_list, on_off_attr_list, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    
    // Identify cluster for better Z2M compatibility
    esp_zb_identify_cluster_cfg_t identify_cfg = {
        .identify_time = 0  // No identify time by default
    };
    
    esp_zb_attribute_list_t *identify_attr_list = esp_zb_identify_cluster_create(&identify_cfg);
    esp_zb_cluster_list_add_identify_cluster(cluster_list, identify_attr_list, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    
    // Binary Input cluster for human-readable state labels (Transmission/Idle)
    esp_zb_binary_input_cluster_cfg_t bi_cfg = {
        .out_of_service = ESP_ZB_ZCL_BINARY_INPUT_OUT_OF_SERVICE_DEFAULT_VALUE,
        .status_flags = ESP_ZB_ZCL_BINARY_INPUT_STATUS_FLAGS_DEFAULT_VALUE,
        .present_value = false
    };
    esp_zb_attribute_list_t *bi_attr_list = esp_zb_binary_input_cluster_create(&bi_cfg);
    // Optional attributes
    esp_zb_binary_input_cluster_add_attr(bi_attr_list, ESP_ZB_ZCL_ATTR_BINARY_INPUT_ACTIVE_TEXT_ID, BI_ACTIVE_TEXT);
    esp_zb_binary_input_cluster_add_attr(bi_attr_list, ESP_ZB_ZCL_ATTR_BINARY_INPUT_INACTIVE_TEXT_ID, BI_INACTIVE_TEXT);
    esp_zb_binary_input_cluster_add_attr(bi_attr_list, ESP_ZB_ZCL_ATTR_BINARY_INPUT_DESCRIPTION_ID, BI_DESCRIPTION);
    esp_zb_binary_input_cluster_add_attr(bi_attr_list, ESP_ZB_ZCL_ATTR_BINARY_INPUT_APPLICATION_TYPE_ID, &BI_APP_TYPE);
    esp_zb_cluster_list_add_binary_input_cluster(cluster_list, bi_attr_list, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    
    return cluster_list;
}

// ====== Endpoint Creation ======
void create_endpoints(void)
{
    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();
    
    // Router endpoint (internal only - not visible in Z2M)
    // Note: Router functionality is handled by the Zigbee stack itself
    
    // Button 1 endpoint (On/Off Switch - Portail 1)
    esp_zb_cluster_list_t *button1_clusters = create_button_clusters();
    esp_zb_endpoint_config_t button1_config = {
        .endpoint = BUTTON_1_ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_ON_OFF_SWITCH_DEVICE_ID,
        .app_device_version = 1
    };
    esp_zb_ep_list_add_ep(ep_list, button1_clusters, button1_config);
    
    // Button 2 endpoint (On/Off Switch - Portail 2)
    esp_zb_cluster_list_t *button2_clusters = create_button_clusters();
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
    ESP_LOGI(TAG, "  - Button 1: EP%d (Portail 1)", BUTTON_1_ENDPOINT);
    ESP_LOGI(TAG, "  - Button 2: EP%d (Portail 2)", BUTTON_2_ENDPOINT);
    ESP_LOGI(TAG, "Endpoints registered successfully");
}

// ====== Button Click Detection ======
void handle_button_click(uint8_t endpoint)
{
    switch (endpoint) {
    case BUTTON_1_ENDPOINT:
        ESP_LOGI(TAG, "Button 1 clicked - Portail 1");
        // Pulse Binary Input PresentValue to TRUE while transmitting ("Transmission")
        {
            bool bi_true = true;
            esp_zb_zcl_set_attribute_val(BUTTON_1_ENDPOINT,
                                         ESP_ZB_ZCL_CLUSTER_ID_BINARY_INPUT,
                                         ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                                         ESP_ZB_ZCL_ATTR_BINARY_INPUT_PRESENT_VALUE_ID,
                                         &bi_true,
                                         sizeof(bi_true));
        }
        led_button1_effect();
        came433_send_portail1(); // Envoyer code CAME pour portail 1
        
        // Retour à OFF après 1 seconde (pour simuler un bouton)
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG, "Button 1 returning to OFF state");
        
        // Mettre à jour l'attribut On/Off à false (stateless)
        {
            bool off_state_1 = false;
            esp_zb_zcl_set_attribute_val(BUTTON_1_ENDPOINT,
                                         ESP_ZB_ZCL_CLUSTER_ID_ON_OFF,
                                         ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                                         ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID,
                                         &off_state_1,
                                         sizeof(off_state_1));
            zb_report_onoff(BUTTON_1_ENDPOINT, false);
        }
        // Ramener Binary Input PresentValue à FALSE ("Idle")
        {
            bool bi_false = false;
            esp_zb_zcl_set_attribute_val(BUTTON_1_ENDPOINT,
                                         ESP_ZB_ZCL_CLUSTER_ID_BINARY_INPUT,
                                         ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                                         ESP_ZB_ZCL_ATTR_BINARY_INPUT_PRESENT_VALUE_ID,
                                         &bi_false,
                                         sizeof(bi_false));
        }
        break;
    case BUTTON_2_ENDPOINT:
        ESP_LOGI(TAG, "Button 2 clicked - Portail 2");
        // Pulse Binary Input PresentValue to TRUE while transmitting ("Transmission")
        {
            bool bi_true = true;
            esp_zb_zcl_set_attribute_val(BUTTON_2_ENDPOINT,
                                         ESP_ZB_ZCL_CLUSTER_ID_BINARY_INPUT,
                                         ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                                         ESP_ZB_ZCL_ATTR_BINARY_INPUT_PRESENT_VALUE_ID,
                                         &bi_true,
                                         sizeof(bi_true));
        }
        led_button2_effect();
        came433_send_portail2(); // Envoyer code CAME pour portail 2
        
        // Retour à OFF après 1 seconde (pour simuler un bouton)
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG, "Button 2 returning to OFF state");
        
        // Mettre à jour l'attribut On/Off à false (stateless)
        {
            bool off_state_2 = false;
            esp_zb_zcl_set_attribute_val(BUTTON_2_ENDPOINT,
                                         ESP_ZB_ZCL_CLUSTER_ID_ON_OFF,
                                         ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                                         ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID,
                                         &off_state_2,
                                         sizeof(off_state_2));
            zb_report_onoff(BUTTON_2_ENDPOINT, false);
        }
        // Ramener Binary Input PresentValue à FALSE ("Idle")
        {
            bool bi_false = false;
            esp_zb_zcl_set_attribute_val(BUTTON_2_ENDPOINT,
                                         ESP_ZB_ZCL_CLUSTER_ID_BINARY_INPUT,
                                         ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                                         ESP_ZB_ZCL_ATTR_BINARY_INPUT_PRESENT_VALUE_ID,
                                         &bi_false,
                                         sizeof(bi_false));
        }
        break;
    default:
        ESP_LOGW(TAG, "Unknown button endpoint: %d", endpoint);
        break;
    }
}
