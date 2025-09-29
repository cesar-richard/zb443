#include "led.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "LED";

static led_strip_handle_t led_strip;

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
        led_set_color(128, 128, 255);
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
