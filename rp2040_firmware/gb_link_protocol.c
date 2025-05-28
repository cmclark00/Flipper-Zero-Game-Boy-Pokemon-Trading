#include "include/gb_link_protocol.h"
#include "hardware/pio.h"
#include "hardware/gpio.h" // For direct GPIO functions if needed, and for gpio_set_function
#include "gb_link_protocol.pio.h" // Generated PIO header
#include <stdio.h> // For printf in debug

// Global state for the link
static GblinkState gblink_current_state = GBLINK_DISCONNECTED;
static PIO pio_instance = pio0; // Choose PIO0 or PIO1
static uint sm_instance = 0;   // State machine number (0-3 for each PIO)
static uint pio_program_offset = 0;

void gb_link_init(void) {
    printf("gb_link_init: Initializing Game Boy Link Protocol...\n");

    // CRITICAL ASSUMPTION: The gb_link_protocol.pio.h program currently uses `wait 0 pin 0`.
    // This hardcodes the expectation that the Game Boy's CLK signal is connected to absolute GPIO0.
    // If GB_CLK_PIN (defined in gb_link_protocol.h) is not 0, this PIO program will not function correctly.
    // Future improvements could make the PIO program more flexible or dynamically patch the instruction.
    if (GB_CLK_PIN != 0) {
        printf("ERROR: GB_CLK_PIN is defined as %d but current PIO program requires GPIO0 for CLK.\n", GB_CLK_PIN);
        printf("Please redefine GB_CLK_PIN to 0 or modify the PIO program's 'wait' instruction.\n");
        gblink_current_state = GBLINK_INIT_FAILED;
        return;
    }

    // Get a free state machine on our chosen PIO instance
    // sm_instance = pio_claim_unused_sm(pio_instance, true); // Or use a fixed one like 0
    // For simplicity, we'll use a fixed SM, assuming it's available.
    // If pio_claim_unused_sm fails, it asserts, so no return check needed if `true` is passed.

    // Load the PIO program into the PIO instruction memory
    if (pio_can_add_program(pio_instance, &gb_link_slave_program)) {
        pio_program_offset = pio_add_program(pio_instance, &gb_link_slave_program);
    } else {
        printf("ERROR: Cannot add PIO program to %s.\n", pio_instance == pio0 ? "pio0" : "pio1");
        gblink_current_state = GBLINK_PIO_LOAD_FAILED;
        return;
    }

    printf("PIO program loaded at offset %d on %s.\n", pio_program_offset, pio_instance == pio0 ? "pio0" : "pio1");

    // Initialize the PIO state machine using the C SDK helper from the .pio.h file
    // This helper function will also configure GPIOs for PIO control.
    // Parameters: PIO instance, SM number, program offset, CLK pin, SI pin, SO pin.
    gb_link_pio_program_init(pio_instance, sm_instance, pio_program_offset,
                             GB_CLK_PIN, GB_SI_PIN, GB_SO_PIN);

    printf("PIO State Machine %d initialized and enabled.\n", sm_instance);
    
    // Set pull-ups/downs if necessary for your hardware setup.
    // For Game Boy link, CLK is driven by GB. SI is input, SO is output.
    // Usually, external pull-ups might be on the Game Boy side or cable.
    // If RP2040 needs to ensure defined states when GB is not driving:
    // gpio_pull_up(GB_CLK_PIN); // Or down, depending on idle state if not driven
    // gpio_pull_up(GB_SI_PIN);  // If expecting high idle from GB's SO

    gblink_current_state = GBLINK_CONNECTED_IDLE; // PIO is ready
    printf("gb_link_init: Initialization complete. State: IDLE\n");
}

uint8_t gb_link_exchange_byte(uint8_t byte_to_send, bool* success) {
    if (gblink_current_state != GBLINK_CONNECTED_IDLE && gblink_current_state != GBLINK_TRADING) {
        // Allow trading state for subsequent bytes in a multi-byte transfer
        if (success) *success = false;
        printf("gb_link_exchange_byte: Not in a valid state for exchange (%d).\n", gblink_current_state);
        return 0xFF; // Indicate error or invalid state
    }

    // Ensure FIFOs are clear before starting (optional, but good practice if previous transaction might have failed)
    // pio_sm_clear_fifos(pio_instance, sm_instance); // Might be too aggressive if used mid-multibyte-transfer

    // Write data to TX FIFO. This will block until the PIO program pulls it.
    // The PIO program's `pull block` will wait for this.
    // printf("PIO TX: 0x%02X\n", byte_to_send);
    pio_sm_put_blocking(pio_instance, sm_instance, (uint32_t)byte_to_send);

    // Read data from RX FIFO. This will block until the PIO program pushes it.
    // The PIO program's `push block` will provide this.
    // Set a timeout for receiving data to prevent indefinite blocking if GB doesn't clock.
    // Timeout is in microseconds. 8 bits at 8kHz is 1ms. Add some margin.
    uint32_t timeout_us = 5000; // 5ms timeout
    uint32_t received_data = 0;
    bool received_in_time = false;
    
    absolute_time_t timeout_time = make_timeout_time_us(timeout_us);
    while (!pio_sm_is_rx_fifo_empty(pio_instance, sm_instance) || !time_reached(timeout_time)) {
        if (!pio_sm_is_rx_fifo_empty(pio_instance, sm_instance)) {
            received_data = pio_sm_get(pio_instance, sm_instance);
            received_in_time = true;
            break;
        }
    }

    if (!received_in_time) {
        if (success) *success = false;
        printf("gb_link_exchange_byte: Timeout waiting for RX data.\n");
        // Consider resetting PIO SM or clearing FIFOs here if a timeout indicates a desync.
        // pio_sm_clear_fifos(pio_instance, sm_instance);
        // pio_sm_exec(pio_instance, sm_instance, pio_encode_jmp(pio_program_offset)); // Restart SM
        return 0xFE; // Indicate timeout error
    }
    
    // printf("PIO RX: 0x%02X\n", (uint8_t)received_data);
    if (success) *success = true;
    return (uint8_t)received_data;
}

GblinkState gb_link_get_status(void) {
    // Future: This could check for clock activity or other signals
    // to determine if a Game Boy is physically connected and active.
    // For now, it just returns the software state.
    return gblink_current_state;
}
