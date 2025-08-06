#ifndef GB_LINK_H
#define GB_LINK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Pin definitions for RP2040 Zero Game Boy PCB
// Based on your actual wiring:
// GPIO 0: Serial Out (from Game Boy) - Input with pullup
// GPIO 2: Serial Clock               - Input with pullup  
// GPIO 3: Serial In (to Game Boy)    - Output, normally high
#define GB_CLK_PIN      2   // GP2 - Game Boy Clock (input with pullup)
#define GB_SO_PIN       0   // GP0 - Game Boy Serial Out (input with pullup)
#define GB_SI_PIN       3   // GP3 - Game Boy Serial In (output, normally high)

// Protocol constants
#define PKMN_BLANK      0x00
#define PKMN_MASTER     0x01
#define PKMN_SLAVE      0x02
#define PKMN_CONNECTED  0x60
#define PKMN_TRADE_CENTRE 0x60
#define PKMN_COLOSSEUM  0x61
#define PKMN_PREAMBLE   0xFD
#define PKMN_NO_DATA    0xFE
#define PKMN_READY      0xFF

// Data size constants
#define PARTY_DATA_SIZE 404      // Game Boy party structure size
#define INDIVIDUAL_POKEMON_SIZE 44   // Individual Pokemon data within party

// Game Boy party structure
typedef struct {
    uint8_t party_count;           // Number of Pokemon (1-6)
    uint8_t species_list[7];       // Species IDs + 0xFF terminator
    uint8_t pokemon_data[6][44];   // Individual Pokemon data (44 bytes each)
    uint8_t ot_names[6][11];       // Original Trainer names
    uint8_t nicknames[6][11];      // Pokemon nicknames
} pokemon_party_t;

// Trade states
typedef enum {
    TRADE_STATE_NOT_CONNECTED = 0,
    TRADE_STATE_CONNECTED,
    TRADE_STATE_READY,
    TRADE_STATE_WAITING,
    TRADE_STATE_DEALING,
    TRADE_STATE_TRADING
} gb_trade_state_t;

// Function prototypes
bool gb_link_init(void);
void gb_link_deinit(void);

uint8_t gb_link_transfer_byte(uint8_t send_byte);
bool gb_link_wait_for_connection(void);

gb_trade_state_t gb_link_get_state(void);
void gb_link_set_state(gb_trade_state_t state);

// Bidirectional trading functions
bool gb_link_trade_or_store(uint8_t* pokemon_data, size_t data_len);
bool gb_link_bidirectional_trade(uint8_t send_slot, uint8_t receive_slot);

// Protocol handler for continuous operation
void gb_link_handle_protocol_step(uint8_t* pokemon_data);

// Health check for interrupt handler
bool gb_link_check_isr_health(void);

// Internal function for setting output byte (used by interrupt handler)
void gb_link_set_output_byte(uint8_t byte);

// Pokemon selection for bidirectional trading
void gb_link_set_selected_pokemon_slot(uint8_t slot);
uint8_t gb_link_get_selected_pokemon_slot(void);

// Party management functions
bool create_party_from_pokemon(const uint8_t* pokemon_data, uint8_t* party_buffer);
bool extract_pokemon_from_party(const uint8_t* party_data, uint8_t slot, uint8_t* pokemon_buffer);
void debug_party_data(const uint8_t* party_data, const char* title);

#endif // GB_LINK_H