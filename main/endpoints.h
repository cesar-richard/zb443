#ifndef ENDPOINTS_H
#define ENDPOINTS_H

#include "esp_err.h"
#include "esp_zigbee_cluster.h"
#include "esp_zigbee_endpoint.h"
#include "ha/esp_zigbee_ha_standard.h"

// ====== Button/Endpoint Configuration ======
#define ESP_ZB_AF_HA_PROFILE_ID 0x0104
// Use On/Off Switch device id for remote-like stateless buttons
#define ESP_ZB_HA_ON_OFF_SWITCH_DEVICE_ID 0x0003
#define BUTTON_1_ENDPOINT 1
#define BUTTON_2_ENDPOINT 2

// ====== Function Prototypes ======
esp_zb_cluster_list_t *create_button_clusters(void);
void create_endpoints(void);
void handle_button_click(uint8_t endpoint);

#endif // ENDPOINTS_H
