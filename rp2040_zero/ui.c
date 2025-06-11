#include "ui.h"
#include "pico/stdlib.h"

#define STATUS_LED_PIN 8
#define BUTTON_PIN 9

static ui_status_t current_status = UI_STATUS_IDLE;

void ui_init(void) {
    gpio_init(STATUS_LED_PIN);
    gpio_set_dir(STATUS_LED_PIN, GPIO_OUT);
    gpio_put(STATUS_LED_PIN, 0);

    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);
}

int ui_button_pressed(void) {
    // Button is active low
    static int last_state = 1;
    int state = gpio_get(BUTTON_PIN);
    int pressed = (last_state == 1 && state == 0);
    last_state = state;
    return pressed;
}

void ui_set_status(ui_status_t status) {
    current_status = status;
    switch (status) {
        case UI_STATUS_IDLE:
            gpio_put(STATUS_LED_PIN, 0);
            break;
        case UI_STATUS_WORKING:
            gpio_put(STATUS_LED_PIN, 1);
            break;
        case UI_STATUS_ERROR:
            // Blink LED for error
            for (int i = 0; i < 3; ++i) {
                gpio_put(STATUS_LED_PIN, 1);
                sleep_ms(100);
                gpio_put(STATUS_LED_PIN, 0);
                sleep_ms(100);
            }
            break;
    }
} 