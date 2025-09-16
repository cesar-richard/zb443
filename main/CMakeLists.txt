idf_component_register(
  SRCS "main.c"
  REQUIRES nvs_flash esp_wifi esp_https_ota esp_http_client driver app_update espressif__esp-zigbee-lib espressif__esp-zboss-lib
)

