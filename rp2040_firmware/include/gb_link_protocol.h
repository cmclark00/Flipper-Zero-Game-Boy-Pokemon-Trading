#ifndef GB_LINK_PROTOCOL_H
#define GB_LINK_PROTOCOL_H

#include "pico/stdlib.h"

// Define GPIO pins for Game Boy link cable communication
// IMPORTANT: For the current gb_link_protocol.pio.h, GB_CLK_PIN MUST be 0.
#define GB_CLK_PIN 0  // Game Boy Clock (Input from GB) - MUST BE GPIO0 for current PIO
#define GB_SI_PIN 1   // Serial Input (Data from GB to RP2040, connected to GB's SO) - Example GP1
#define GB_SO_PIN 2   // Serial Output (Data from RP2040 to GB, connected to GB's SI) - Example GP2

// Link State Management
typedef enum {
    GBLINK_DISCONNECTED,
    GBLINK_INIT_FAILED,
    GBLINK_PIO_LOAD_FAILED,
    GBLINK_CONNECTED_IDLE,  // PIO initialized, awaiting interaction
    GBLINK_READY_TO_TRADE,  // (Future use) Initial handshake complete
    GBLINK_TRADING,         // (Future use) Pokemon data exchange in progress
    GBLINK_TRADE_COMPLETE,  // (Future use)
    GBLINK_ERROR
} GblinkState;

// Function declarations
void gb_link_init(void);
uint8_t gb_link_exchange_byte(uint8_t byte_to_send, bool* success); // Added success flag

GblinkState gb_link_get_status(void);
// void gb_link_start_trade_sequence(void); // Optional for now
// void gb_link_process(void);              // Optional for now

#endif // GB_LINK_PROTOCOL_H
