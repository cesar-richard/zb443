idf_component_register(
  SRCS "main.c" "zigbee.c" "led.c" "endpoints.c" "came433.c"
  REQUIRES esp-zigbee-lib
  PRIV_REQUIRES nvs_flash driver esp_netif esp_event esp_coex led_strip
)

