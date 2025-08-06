// Simple hardware test program
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"

#define LED_PIN 25
#define BUTTON_PIN 9

int main() {
    stdio_init_all();
    
    // Initialize LED
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    
    // Initialize button
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);
    
    printf("Hardware test starting...\n");
    printf("LED should blink every second\n");
    printf("Button presses should be logged\n");
    
    bool led_state = false;
    bool last_button = true; // Pull-up means unpressed = true
    
    while (true) {
        // Toggle LED every second
        led_state = !led_state;
        gpio_put(LED_PIN, led_state);
        printf("LED: %s\n", led_state ? "ON" : "OFF");
        
        // Check button
        bool current_button = gpio_get(BUTTON_PIN);
        if (current_button != last_button) {
            printf("Button: %s (GPIO reads %d)\n", 
                   current_button ? "RELEASED" : "PRESSED", 
                   current_button ? 1 : 0);
            last_button = current_button;
        }
        
        sleep_ms(1000);
    }
    
    return 0;
}