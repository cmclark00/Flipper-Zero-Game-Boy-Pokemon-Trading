#ifndef GB_LINK_PROTOCOL_H
#define GB_LINK_PROTOCOL_H

#include "pico/stdlib.h"

// Define placeholder GPIO pins for Game Boy link cable communication
// These are arbitrary and should be configured based on the actual hardware setup.
#define GB_CLK_PIN 0  // Example GPIO for Clock (Output from RP2040)
#define GB_SI_PIN 1   // Example GPIO for Serial Input (Data from Game Boy to RP2040, connected to Game Boy's SO)
#define GB_SO_PIN 2   // Example GPIO for Serial Output (Data from RP2040 to Game Boy, connected to Game Boy's SI)

// Function declarations
void gb_link_init();
uint8_t gb_link_exchange_byte(uint8_t byte_to_send);
void gb_link_set_clock_state(bool high); // For potential manual clock control

#endif // GB_LINK_PROTOCOL_H
