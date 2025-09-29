#include "came433.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "driver/gpio.h"
#include "soc/rmt_struct.h"
#include <inttypes.h>
#include <stdlib.h>

static const char *TAG = "CAME433";

/*
Hardware: High-side driver NPN+PNP (2N2222A + 2N2907A)
- Idle (no TX): GPIO=0 → NPN OFF → PNP base pulled up by RB_PNP → PNP OFF → DATA pulled down by RPD
- Active (TX): GPIO=1 → NPN ON → PNP base driven low → PNP ON → DATA = +5V (OOK active-high)
Firmware requirement: keep GPIO low at startup and after transmission.
*/
// ====== RMT Configuration ======
static rmt_channel_handle_t came_tx_channel = NULL;
static rmt_encoder_handle_t came_encoder = NULL;

// ====== Helper Macros ======
#define OUT_LEVEL(level) ((level) ? 1 : 0)

// ====== Private Functions ======

/**
 * @brief Encode a single CAME bit with exact timings
 * 
 * CAME bit encoding (based on Flipper Zero implementation):
 * - Bit 0: 320µs LOW + 640µs HIGH
 * - Bit 1: 640µs LOW + 320µs HIGH
 */
static void came_encode_bit(rmt_symbol_word_t *symbol, bool bit)
{
    if (bit) {
        // Bit 1: 640µs LOW + 320µs HIGH
        symbol->level0 = OUT_LEVEL(0);
        symbol->duration0 = CAME_LONG_GAP;
        symbol->level1 = OUT_LEVEL(1);
        symbol->duration1 = CAME_SHORT_PULSE;
    } else {
        // Bit 0: 320µs LOW + 640µs HIGH
        symbol->level0 = OUT_LEVEL(0);
        symbol->duration0 = CAME_SHORT_GAP;
        symbol->level1 = OUT_LEVEL(1);
        symbol->duration1 = CAME_LONG_PULSE;
    }
}

/**
 * @brief Encode CAME sync pulse (header + start bit)
 * 
 * CAME sync structure (based on Flipper Zero):
 * - Header: 24320µs LOW
 * - Start bit: 320µs HIGH
 */
static void came_encode_sync(rmt_symbol_word_t *symbol)
{
    symbol->level0 = OUT_LEVEL(0);
    symbol->duration0 = CAME_HEADER_DURATION;  // 24320µs header LOW
    symbol->level1 = OUT_LEVEL(1);
    symbol->duration1 = CAME_START_BIT_DURATION; // 320µs start bit HIGH
}

/**
 * @brief Send a CAME 24-bit code with proper protocol structure
 */
static void came_send_came_code(uint32_t code, uint8_t repeats)
{
    ESP_LOGI(TAG, "Sending CAME code: 0x%06X (%d repeats)", (unsigned int)code, repeats);
    
    // Ensure transmitter is OFF in idle state (idle LOW with high-side driver)
    (void)gpio_set_level(CAME_GPIO, 0);
    ESP_ERROR_CHECK(rmt_enable(came_tx_channel));

    // Calculate symbol count: sync + 24 bits per repeat
    size_t symbols_per_repeat = 1 + 24; // 1 sync + 24 bits
    size_t symbol_count = repeats * symbols_per_repeat;
    rmt_symbol_word_t *symbols = malloc(symbol_count * sizeof(rmt_symbol_word_t));

    if (symbols == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for CAME symbols");
        return;
    }

    size_t symbol_idx = 0;

    // Encode each repeat
    for (int repeat = 0; repeat < repeats; repeat++) {
        // Add sync pulse (header + start bit)
        came_encode_sync(&symbols[symbol_idx++]);

        // Add 24 bits (MSB first)
        for (int bit = 23; bit >= 0; bit--) {
            bool bit_value = (code >> bit) & 1;
            came_encode_bit(&symbols[symbol_idx++], bit_value);
        }
    }

    // Transmit
    rmt_transmit_config_t tx_config = {
        .loop_count = 0, // No loop
    };

    ESP_ERROR_CHECK(rmt_transmit(came_tx_channel, came_encoder, symbols, 
                                symbol_count * sizeof(rmt_symbol_word_t), &tx_config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(came_tx_channel, 1000));

    free(symbols);

    // Return to idle (LOW) and disable channel
    (void)gpio_set_level(CAME_GPIO, 0);
    ESP_ERROR_CHECK(rmt_disable(came_tx_channel));
    ESP_LOGI(TAG, "CAME code transmitted successfully");
}

// ====== Public API ======

void came433_init(void)
{
    ESP_LOGI(TAG, "Initializing CAME 433MHz transmitter...");

    // Configure GPIO
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << CAME_GPIO),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    
    // Ensure transmitter is OFF at startup (idle LOW)
    ESP_ERROR_CHECK(gpio_set_level(CAME_GPIO, 0));

    // Configure RMT TX channel
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = CAME_GPIO,
        .mem_block_symbols = 64,
        .resolution_hz = 1000000, // 1µs resolution
        .trans_queue_depth = 4,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &came_tx_channel));

    // Configure RMT encoder
    rmt_copy_encoder_config_t copy_encoder_config = {};
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&copy_encoder_config, &came_encoder));

    ESP_LOGI(TAG, "CAME 433MHz transmitter initialized successfully");
}

void came433_send_portail1(void)
{
    ESP_LOGI(TAG, "Sending Portail Principal (0x%06X)", (unsigned int)KEY_A);
    came_send_came_code(KEY_A, CAME_REPEATS);
}

void came433_send_portail2(void)
{
    ESP_LOGI(TAG, "Sending Portail Parking (0x%06X)", (unsigned int)KEY_B);
    came_send_came_code(KEY_B, CAME_REPEATS);
}