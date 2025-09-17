#include "led.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "LED";

static led_strip_handle_t led_strip;
static TaskHandle_t led_blink_task_handle = NULL;

// ====== LED Initialization ======
void led_init(void)
{
    ESP_LOGI(TAG, "Initializing LED RGB on GPIO%d", LED_GPIO);
    
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = LED_NUMBERS,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };
    
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };
    
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    ESP_LOGI(TAG, "LED RGB initialized successfully");
}

// ====== LED Control ======
void led_set_color(uint32_t red, uint32_t green, uint32_t blue)
{
    ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 0, red, green, blue));
    ESP_ERROR_CHECK(led_strip_refresh(led_strip));
}

void led_off(void)
{
    ESP_ERROR_CHECK(led_strip_clear(led_strip));
}

void led_blink(uint32_t red, uint32_t green, uint32_t blue, uint32_t duration_ms)
{
    led_set_color(red, green, blue);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    led_off();
}

// ====== Button LED Effects ======
void led_button1_effect(void)
{
    ESP_LOGI(TAG, "Button 1 LED effect - Blue fixed");
    
    // Arrêter le clignotant si il tourne
    if (led_blink_task_handle != NULL) {
        vTaskDelete(led_blink_task_handle);
        led_blink_task_handle = NULL;
    }
    
    led_set_color(0, 0, 255); // Bleu fixe pour portail 1
}

void led_button2_effect(void)
{
    ESP_LOGI(TAG, "Button 2 LED effect - Blue fixed");
    
    // Arrêter le clignotant si il tourne
    if (led_blink_task_handle != NULL) {
        vTaskDelete(led_blink_task_handle);
        led_blink_task_handle = NULL;
    }
    
    led_set_color(0, 0, 255); // Bleu fixe pour portail 2
}

// ====== LED Blink Task ======
static void led_blink_task(void *pvParameters)
{
    ESP_LOGI(TAG, "LED blink task started - Orange blinking");
    
    while (1) {
        led_set_color(255, 165, 0); // Orange
        vTaskDelay(pdMS_TO_TICKS(100));
        led_off();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void led_zigbee_connecting(void)
{
    ESP_LOGI(TAG, "Zigbee connecting LED effect - Orange blinking");
    
    // Créer la tâche de clignotant si elle n'existe pas
    if (led_blink_task_handle == NULL) {
        xTaskCreate(led_blink_task, "led_blink", 2048, NULL, 5, &led_blink_task_handle);
    }
}

void led_zigbee_connected(void)
{
    ESP_LOGI(TAG, "Zigbee connected LED effect - Green fixed");
    
    // Arrêter la tâche de clignotant
    if (led_blink_task_handle != NULL) {
        vTaskDelete(led_blink_task_handle);
        led_blink_task_handle = NULL;
    }
    
    led_set_color(0, 255, 0); // Vert fixe
}

// ====== Identify LED Effects ======
void led_identify_blink(void)
{
    ESP_LOGI(TAG, "Identify effect - Blink");
    led_set_color(255, 255, 255); // Blanc
    vTaskDelay(pdMS_TO_TICKS(100));
    led_off();
    vTaskDelay(pdMS_TO_TICKS(100));
    led_set_color(255, 255, 255); // Blanc
    vTaskDelay(pdMS_TO_TICKS(100));
    led_off();
}

void led_identify_breathe(void)
{
    ESP_LOGI(TAG, "Identify effect - Breathe");
    for (int i = 0; i < 15; i++) {
        led_set_color(255, 255, 255); // Blanc
        vTaskDelay(pdMS_TO_TICKS(100));
        led_off();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void led_identify_okay(void)
{
    ESP_LOGI(TAG, "Identify effect - Okay");
    led_set_color(0, 255, 0); // Vert
    vTaskDelay(pdMS_TO_TICKS(1000));
    led_off();
}
