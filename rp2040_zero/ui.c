#include "ui.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include <stdio.h>

static led_pattern_t current_led_pattern = LED_OFF;
static uint64_t last_led_update = 0;
static bool led_state = false;
static bool button_state = false;
static bool last_button_state = false;
static uint64_t button_press_time = 0;

// LED timing constants (in microseconds)
#define LED_SLOW_BLINK_PERIOD   1000000  // 1 second
#define LED_FAST_BLINK_PERIOD   200000   // 200ms
#define LED_HEARTBEAT_PERIOD    2000000  // 2 seconds
#define BUTTON_DEBOUNCE_TIME    50000    // 50ms

bool ui_init(void) {
    printf("Initializing UI with LED_PIN=%d, BUTTON_PIN=%d\n", LED_PIN, BUTTON_PIN);
    
    // Initialize LED pin
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);
    
    // Test LED immediately
    printf("Testing LED on GPIO %d...\n", LED_PIN);
    for (int i = 0; i < 3; i++) {
        printf("LED ON\n");
        gpio_put(LED_PIN, 1);
        sleep_ms(300);
        printf("LED OFF\n");
        gpio_put(LED_PIN, 0);
        sleep_ms(300);
    }
    printf("LED test complete\n");
    
    // Initialize button pin
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);
    
    current_led_pattern = LED_OFF;
    last_led_update = time_us_64();
    
    printf("UI initialized successfully\n");
    return true;
}

void ui_deinit(void) {
    gpio_put(LED_PIN, 0);
    gpio_deinit(LED_PIN);
    gpio_deinit(BUTTON_PIN);
}

static void update_led(void) {
    uint64_t current_time = time_us_64();
    uint64_t elapsed = current_time - last_led_update;
    
    switch (current_led_pattern) {
        case LED_OFF:
            gpio_put(LED_PIN, 0);
            break;
            
        case LED_ON:
            gpio_put(LED_PIN, 1);
            break;
            
        case LED_SLOW_BLINK:
            if (elapsed >= LED_SLOW_BLINK_PERIOD) {
                led_state = !led_state;
                gpio_put(LED_PIN, led_state);
                last_led_update = current_time;
                printf("LED slow blink: %d (elapsed=%llu us)\n", led_state ? 1 : 0, elapsed);
            }
            break;
            
        case LED_FAST_BLINK:
            if (elapsed >= LED_FAST_BLINK_PERIOD) {
                led_state = !led_state;
                gpio_put(LED_PIN, led_state);
                last_led_update = current_time;
            }
            break;
            
        case LED_HEARTBEAT:
            // Double blink pattern
            if (elapsed < 100000) {
                gpio_put(LED_PIN, 1);
            } else if (elapsed < 200000) {
                gpio_put(LED_PIN, 0);
            } else if (elapsed < 300000) {
                gpio_put(LED_PIN, 1);
            } else if (elapsed < LED_HEARTBEAT_PERIOD) {
                gpio_put(LED_PIN, 0);
            } else {
                last_led_update = current_time;
            }
            break;
    }
}

static void update_button(void) {
    bool current_button = !gpio_get(BUTTON_PIN); // Inverted because pull-up
    uint64_t current_time = time_us_64();
    
    // Debounce logic
    if (current_button != last_button_state) {
        button_press_time = current_time;
        printf("Button state change: GPIO=%d, button=%d\n", gpio_get(BUTTON_PIN), current_button ? 1 : 0);
    }
    
    if ((current_time - button_press_time) > BUTTON_DEBOUNCE_TIME) {
        if (button_state != current_button) {
            button_state = current_button;
            printf("Button debounced: %d\n", button_state ? 1 : 0);
        }
    }
    
    last_button_state = current_button;
}

void ui_update(void) {
    update_led();
    update_button();
}

bool ui_button_pressed(void) {
    return button_state;
}

void ui_set_led_pattern(led_pattern_t pattern) {
    current_led_pattern = pattern;
    last_led_update = time_us_64();
    led_state = false;
}

void ui_show_status(gb_trade_state_t state) {
    switch (state) {
        case TRADE_STATE_NOT_CONNECTED:
            ui_set_led_pattern(LED_SLOW_BLINK);
            printf("Status: Not connected\n");
            break;
            
        case TRADE_STATE_CONNECTED:
            ui_set_led_pattern(LED_FAST_BLINK);
            printf("Status: Connected - Ready for trade\n");
            break;
            
        case TRADE_STATE_READY:
            ui_set_led_pattern(LED_ON);
            printf("Status: Ready - Trade Center selected\n");
            break;
            
        case TRADE_STATE_WAITING:
            ui_set_led_pattern(LED_HEARTBEAT);
            printf("Status: Waiting for trade data\n");
            break;
            
        case TRADE_STATE_DEALING:
            ui_set_led_pattern(LED_FAST_BLINK);
            printf("Status: Dealing - Pokemon selection\n");
            break;
            
        case TRADE_STATE_TRADING:
            ui_set_led_pattern(LED_HEARTBEAT);
            printf("Status: Trading in progress\n");
            break;
    }
}

void ui_show_error(const char* message) {
    printf("ERROR: %s\n", message);
    
    // Flash LED rapidly for error indication
    for (int i = 0; i < 10; i++) {
        gpio_put(LED_PIN, 1);
        sleep_ms(50);
        gpio_put(LED_PIN, 0);
        sleep_ms(50);
    }
}

void ui_show_success(const char* message) {
    printf("SUCCESS: %s\n", message);
    
    // Solid LED for 2 seconds for success indication
    gpio_put(LED_PIN, 1);
    sleep_ms(2000);
    gpio_put(LED_PIN, 0);
}