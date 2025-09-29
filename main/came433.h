#ifndef CAME433_H
#define CAME433_H

#include <stdint.h>
#include <stdbool.h>

// ====== Hardware Configuration ======
// High-side driver NPN+PNP requires GPIO idle = LOW (0)
#define CAME_GPIO 4                    // GPIO for CAME 433MHz TX (ESP32-C6 DevKit)
#define CAME_CARRIER_FREQ 433920000    // 433.92 MHz
#define CAME_REPEATS 5                 // Number of repetitions per transmission

// ====== CAME Protocol Keys ======
#define KEY_A  0x0003B29B  // Portail principal (24 bits)
#define KEY_B  0x0003B29A  // Portail parking (24 bits)

// ====== CAME Protocol Parameters (Working) ======
// These parameters have been tested and work perfectly with Flipper Zero
#define CAME_SHORT_PULSE 320           // Short pulse duration (µs)
#define CAME_LONG_PULSE 640            // Long pulse duration (µs)
#define CAME_SHORT_GAP 320             // Short gap duration (µs)
#define CAME_LONG_GAP 640              // Long gap duration (µs)
#define CAME_HEADER_DURATION 24320     // CAME header duration (µs)
#define CAME_START_BIT_DURATION 320    // CAME start bit duration (µs)

// ====== Public API ======
void came433_init(void);
void came433_send_portail1(void);
void came433_send_portail2(void);

#endif // CAME433_H