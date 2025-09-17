#ifndef CAME433_H
#define CAME433_H

#include "esp_err.h"
#include "driver/rmt_tx.h"

// ====== CAME 433MHz Configuration ======
#define CAME_GPIO 4
#define CAME_CLK_DIV 100
#define CAME_CARRIER_FREQ 433920000  // 433.92 MHz
// Nombre de répétitions par envoi (augmente la longueur audible de la trame)
#define CAME_REPEATS 12
// Inversion de la sortie: 0 = actif haut (liaison directe), 1 = actif bas (montage NPN open-collector + pull-up 5V)
#define CAME_INVERT_OUTPUT 0
// CAME 24 bits timing (retour aux timings qui marchaient)
// Short pulse: ~300µs, Long pulse: ~900µs
// Sync gap: ~3ms
#define CAME_SHORT_PULSE 300
#define CAME_LONG_PULSE 900
#define CAME_SYNC_PULSE 3000
#define CAME_GAP 10000

// ====== CAME keys ======
#define KEY_A  0x0003B29B  // Portail principal (24 bits complets)
#define KEY_B  0x0003B29A  // Portail parking (24 bits complets)

// ====== CAME Working Parameters ======
// Paramètres qui marchent parfaitement (testés avec Flipper Zero)
#define CAME_WORKING_SHORT_PULSE 320
#define CAME_WORKING_LONG_PULSE 640
#define CAME_WORKING_SHORT_GAP 320
#define CAME_WORKING_LONG_GAP 640
#define CAME_WORKING_USE_SYNC true
#define CAME_WORKING_USE_PREAMBLE false
#define CAME_WORKING_INVERT_LOGIC false
#define CAME_WORKING_USE_LSB false

// ====== Function Prototypes ======
void came433_init(void);
void came433_send_code(uint32_t code, uint8_t repeats);
void came433_send_code_with_timings(uint32_t code, uint8_t repeats, uint16_t short_pulse, uint16_t long_pulse);
void came433_send_code_with_timings_and_logic(uint32_t code, uint8_t repeats, uint16_t short_pulse, uint16_t long_pulse, bool invert_logic);
void came433_send_code_with_structure(uint32_t code, uint8_t repeats, uint16_t short_pulse, uint16_t long_pulse, bool invert_logic, bool use_preamble, bool use_sync);
void came433_send_code_with_structure_and_name(uint32_t code, uint8_t repeats, uint16_t short_pulse, uint16_t long_pulse, bool invert_logic, bool use_preamble, bool use_sync, const char* name);
void came433_send_code_with_exact_timings(uint32_t code, uint8_t repeats, uint16_t short_pulse, uint16_t long_pulse, uint16_t short_gap, uint16_t long_gap, bool invert_logic, bool use_preamble, bool use_sync, const char* name);
void came433_send_portail1(void);
void came433_send_portail2(void);

#endif // CAME433_H
