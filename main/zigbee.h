#ifndef ZIGBEE_H
#define ZIGBEE_H

#include "esp_err.h"
#include "esp_zigbee_core.h"
#include "esp_zigbee_cluster.h"
#include "esp_zigbee_attribute.h"
#include "esp_zigbee_endpoint.h"
#include "platform/esp_zigbee_platform.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "endpoints.h"

// ====== Zigbee Configuration ======
#define ESP_ZB_AF_HA_PROFILE_ID 0x0104

// ====== Function Prototypes ======
void zigbee_init(void);
void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct);
esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message);

void create_endpoints(void);

#endif // ZIGBEE_H
