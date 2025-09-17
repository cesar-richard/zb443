#include "came433.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "driver/gpio.h"
#include "soc/rmt_struct.h"
#include <inttypes.h>
#include <string.h>

static const char *TAG = "CAME433";

static rmt_channel_handle_t came_tx_channel = NULL;
static rmt_encoder_handle_t came_encoder = NULL;

// Applique l'inversion de niveau si nécessaire
#if CAME_INVERT_OUTPUT
#define OUT_LEVEL(v) ((v) ? 0 : 1)
#else
#define OUT_LEVEL(v) (v)
#endif

// ====== CAME 433MHz Initialization ======
void came433_init(void)
{
    ESP_LOGI(TAG, "Initializing CAME 433MHz transmitter on GPIO%d (invert=%d)", CAME_GPIO, (int)CAME_INVERT_OUTPUT);
    
    // Ensure idle level keeps transmitter OFF: set GPIO high so NPN is ON → DATA low
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << CAME_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 1,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_ERROR_CHECK(gpio_set_level(CAME_GPIO, 1));
    
    rmt_tx_channel_config_t tx_chan_config = {
        .gpio_num = CAME_GPIO,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 1000000, // 1MHz = 1µs resolution
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
    };
    
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &came_tx_channel));
    ESP_ERROR_CHECK(rmt_enable(came_tx_channel));
    
    // Create copy encoder
    rmt_copy_encoder_config_t copy_encoder_config = {};
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&copy_encoder_config, &came_encoder));
    
    ESP_LOGI(TAG, "CAME 433MHz transmitter initialized successfully");
}

// ====== CAME Code Generation ======
static void came_encode_bit_with_exact_timings(rmt_symbol_word_t *symbol, bool bit, uint16_t short_pulse, uint16_t long_pulse,
                                               uint16_t short_gap, uint16_t long_gap, bool invert_logic)
{
    if (invert_logic) {
        // Logique inversée: Bit 1 = Short gap + Long pulse
    if (bit) {
            symbol->level0 = OUT_LEVEL(0);
            symbol->duration0 = short_gap;
            symbol->level1 = OUT_LEVEL(1);
            symbol->duration1 = long_pulse;
        } else {
            symbol->level0 = OUT_LEVEL(0);
            symbol->duration0 = long_gap;
            symbol->level1 = OUT_LEVEL(1);
            symbol->duration1 = short_pulse;
        }
    } else {
        // Logique CAME standard: Bit 1 = Long gap + Short pulse, Bit 0 = Short gap + Long pulse
        if (bit) {
            symbol->level0 = OUT_LEVEL(0);
            symbol->duration0 = long_gap;
            symbol->level1 = OUT_LEVEL(1);
            symbol->duration1 = short_pulse;
        } else {
            symbol->level0 = OUT_LEVEL(0);
            symbol->duration0 = short_gap;
            symbol->level1 = OUT_LEVEL(1);
            symbol->duration1 = long_pulse;
        }
    }
}

static void came_encode_bit_with_logic(rmt_symbol_word_t *symbol, bool bit, uint16_t short_pulse, uint16_t long_pulse, bool invert_logic)
{
    came_encode_bit_with_exact_timings(symbol, bit, short_pulse, long_pulse, short_pulse, long_pulse, invert_logic);
}

static void came_encode_bit(rmt_symbol_word_t *symbol, bool bit, uint16_t short_pulse, uint16_t long_pulse)
{
    came_encode_bit_with_logic(symbol, bit, short_pulse, long_pulse, false);
}

static void came_encode_preamble(rmt_symbol_word_t *symbol, uint16_t short_pulse, uint16_t long_pulse)
{
    // Préambule CAME spécifique: 4 bits hauts + 1 bit bas (basé sur doc CAME)
    symbol->level0 = OUT_LEVEL(1);
    symbol->duration0 = long_pulse * 4;  // 4 bits hauts
    symbol->level1 = OUT_LEVEL(0);
    symbol->duration1 = short_pulse;     // 1 bit bas
}

static void came_encode_sync(rmt_symbol_word_t *symbol)
{
    // Sync CAME spécifique : header 24320µs LOW + start bit 320µs HIGH (basé sur code Flipper Zero)
    symbol->level0 = OUT_LEVEL(0);
    symbol->duration0 = 24320; // 24.32ms header LOW (vrai CAME)
    symbol->level1 = OUT_LEVEL(1);
    symbol->duration1 = 320; // 320µs start bit HIGH (vrai CAME)
}

// ====== CAME Transmission ======
void came433_send_code(uint32_t code, uint8_t repeats)
{
    came433_send_code_with_timings(code, repeats, CAME_SHORT_PULSE, CAME_LONG_PULSE);
}

void came433_send_code_with_timings(uint32_t code, uint8_t repeats, uint16_t short_pulse, uint16_t long_pulse)
{
    came433_send_code_with_timings_and_logic(code, repeats, short_pulse, long_pulse, false);
}

void came433_send_code_with_timings_and_logic(uint32_t code, uint8_t repeats, uint16_t short_pulse, uint16_t long_pulse, bool invert_logic)
{
    came433_send_code_with_structure(code, repeats, short_pulse, long_pulse, invert_logic, false, true);
}

void came433_send_code_with_structure(uint32_t code, uint8_t repeats, uint16_t short_pulse, uint16_t long_pulse, 
                                     bool invert_logic, bool use_preamble, bool use_sync)
{
    came433_send_code_with_structure_and_name(code, repeats, short_pulse, long_pulse, invert_logic, use_preamble, use_sync, "UNKNOWN");
}

void came433_send_code_with_structure_and_name(uint32_t code, uint8_t repeats, uint16_t short_pulse, uint16_t long_pulse, 
                                             bool invert_logic, bool use_preamble, bool use_sync, const char* name)
{
    // Utilise les gaps par défaut (même que pulse)
    came433_send_code_with_exact_timings(code, repeats, short_pulse, long_pulse, short_pulse, long_pulse, 
                                       invert_logic, use_preamble, use_sync, name);
}

void came433_send_code_with_exact_timings(uint32_t code, uint8_t repeats, uint16_t short_pulse, uint16_t long_pulse, 
                                        uint16_t short_gap, uint16_t long_gap, bool invert_logic, 
                                        bool use_preamble, bool use_sync, const char* name)
{
    ESP_LOGI(TAG, "Sending CAME code: 0x%06" PRIX32 " (%d repeats) [%dµs/%dµs] invert=%d preamble=%d sync=%d", 
             code, repeats, short_pulse, long_pulse, invert_logic, use_preamble, use_sync);
    // Ensure idle keeps TX OFF
    (void)gpio_set_level(CAME_GPIO, 1);
    ESP_ERROR_CHECK(rmt_enable(came_tx_channel));

    // Calculate symbol count based on structure
    size_t symbols_per_repeat = 24; // 24 bits minimum
    if (use_preamble) symbols_per_repeat += 1; // +1 for preamble
    if (use_sync) symbols_per_repeat += 1;     // +1 for sync
    
    size_t symbol_count = repeats * symbols_per_repeat;
    rmt_symbol_word_t *symbols = malloc(symbol_count * sizeof(rmt_symbol_word_t));
    
    if (symbols == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for CAME symbols");
        return;
    }
    
    size_t symbol_idx = 0;
    
    // Encode each repeat
    for (int repeat = 0; repeat < repeats; repeat++) {
        // Add preamble if requested
        if (use_preamble) {
            came_encode_preamble(&symbols[symbol_idx++], short_pulse, long_pulse);
        }
        
        // Add sync if requested
        if (use_sync) {
            came_encode_sync(&symbols[symbol_idx++]);
        }
        
        // Add 24 bits (MSB first ou LSB first selon le nom)
        bool use_lsb = (strstr(name, "LSB") != NULL);
        if (use_lsb) {
            // LSB first (bit 0 à 23)
            for (int bit = 0; bit < 24; bit++) {
                bool bit_value = (code >> bit) & 1;
                came_encode_bit_with_exact_timings(&symbols[symbol_idx++], bit_value, short_pulse, long_pulse, 
                                                 short_gap, long_gap, invert_logic);
            }
        } else {
            // MSB first (bit 23 à 0)
        for (int bit = 23; bit >= 0; bit--) {
            bool bit_value = (code >> bit) & 1;
                came_encode_bit_with_exact_timings(&symbols[symbol_idx++], bit_value, short_pulse, long_pulse, 
                                                 short_gap, long_gap, invert_logic);
            }
        }
    }
    
    // Transmit
    rmt_transmit_config_t tx_config = {
        .loop_count = 0, // No loop
    };
    
    ESP_ERROR_CHECK(rmt_transmit(came_tx_channel, came_encoder, symbols, symbol_count * sizeof(rmt_symbol_word_t), &tx_config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(came_tx_channel, 1000));
    
    free(symbols);
    
    // Return to idle and disable channel to avoid pin takeover
    (void)gpio_set_level(CAME_GPIO, 1);
    ESP_ERROR_CHECK(rmt_disable(came_tx_channel));
    ESP_LOGI(TAG, "CAME code transmitted successfully");
}

// ====== Predefined CAME Codes ======
void came433_send_portail1(void)
{
    // Portail principal - paramètres qui marchent parfaitement
    uint32_t portail1_code = KEY_A; // 0x03B29B
    ESP_LOGI(TAG, "Sending Portail Principal (0x%06" PRIX32 ")", portail1_code);
    came433_send_code_with_exact_timings(portail1_code, CAME_REPEATS, 
                                        CAME_WORKING_SHORT_PULSE, CAME_WORKING_LONG_PULSE,
                                        CAME_WORKING_SHORT_GAP, CAME_WORKING_LONG_GAP,
                                        CAME_WORKING_INVERT_LOGIC, CAME_WORKING_USE_PREAMBLE, 
                                        CAME_WORKING_USE_SYNC, "PORTAL-1");
}

void came433_send_portail2(void)
{
    // Portail parking - paramètres qui marchent parfaitement
    uint32_t portail2_code = KEY_B; // 0x03B29A
    ESP_LOGI(TAG, "Sending Portail Parking (0x%06" PRIX32 ")", portail2_code);
    came433_send_code_with_exact_timings(portail2_code, CAME_REPEATS, 
                                        CAME_WORKING_SHORT_PULSE, CAME_WORKING_LONG_PULSE,
                                        CAME_WORKING_SHORT_GAP, CAME_WORKING_LONG_GAP,
                                        CAME_WORKING_INVERT_LOGIC, CAME_WORKING_USE_PREAMBLE, 
                                        CAME_WORKING_USE_SYNC, "PORTAL-2");
}

// ====== Test Functions (removed - using working parameters now) ======
