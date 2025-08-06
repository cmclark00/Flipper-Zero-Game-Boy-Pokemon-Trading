#ifndef UI_H
#define UI_H

#include <stdint.h>
#include <stdbool.h>
#include "gb_link.h"

// Pin definitions for RP2040 Zero UI
#define LED_PIN         8   // LED (GP8)
#define BUTTON_PIN      9   // User button (GP9)

// LED patterns
typedef enum {
    LED_OFF = 0,
    LED_ON,
    LED_SLOW_BLINK,
    LED_FAST_BLINK,
    LED_HEARTBEAT
} led_pattern_t;

// Function prototypes
bool ui_init(void);
void ui_deinit(void);

void ui_update(void);
bool ui_button_pressed(void);

void ui_set_led_pattern(led_pattern_t pattern);
void ui_show_status(gb_trade_state_t state);

void ui_show_error(const char* message);
void ui_show_success(const char* message);

#endif // UI_H