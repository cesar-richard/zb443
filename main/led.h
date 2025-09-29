#ifndef LED_H
#define LED_H

#include "esp_err.h"
#include "led_strip.h"

// ====== LED Configuration ======
#define LED_GPIO 8
#define LED_NUMBERS 1

// ====== Function Prototypes ======
void led_init(void);
void led_set_color(uint32_t red, uint32_t green, uint32_t blue);
void led_off(void);

// ====== Identify LED Effects ======
void led_identify_blink(void);
void led_identify_breathe(void);
void led_identify_okay(void);

#endif // LED_H
