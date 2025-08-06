#include "gb_link.h"
#include "storage.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
#include <stdio.h>
#include <string.h>

// Trade protocol states
typedef enum {
    TRADE_RESET,
    TRADE_INIT,
    TRADE_RANDOM,
    TRADE_DATA,
    TRADE_PATCH_HEADER,
    TRADE_PATCH_DATA,
    TRADE_SELECT,
    TRADE_PENDING,
    TRADE_CONFIRMATION,
    TRADE_DONE,
    TRADE_CANCEL
} trade_centre_state_t;

// Render states for UI feedback
typedef enum {
    GAMEBOY_CONN_FALSE,
    GAMEBOY_CONN_TRUE,
    GAMEBOY_READY,
    GAMEBOY_WAITING,
    GAMEBOY_TRADE_PENDING,
    GAMEBOY_TRADING,
    GAMEBOY_TRADE_CANCEL,
    GAMEBOY_COLOSSEUM
} render_gameboy_state_t;

static gb_trade_state_t current_state = TRADE_STATE_NOT_CONNECTED;
static trade_centre_state_t trade_centre_state = TRADE_RESET;
static render_gameboy_state_t gameboy_status = GAMEBOY_CONN_FALSE;

static uint8_t shift_register = 0;
static uint8_t bit_count = 0;
static uint8_t last_received = 0;
static bool transfer_complete = false;

// Party management buffer
static uint8_t party_buffer[PARTY_DATA_SIZE];

// Party management functions
bool create_party_from_pokemon(const uint8_t* pokemon_data, uint8_t* party_buffer) {
    if (pokemon_data == NULL || party_buffer == NULL) {
        printf("ERROR: NULL pointers in create_party_from_pokemon\n");
        return false;
    }
    
    // Clear the party buffer
    memset(party_buffer, 0, PARTY_DATA_SIZE);
    
    // Set party count to 1 (we have one Pokemon)
    party_buffer[0] = 1;
    
    // Set species list - first entry is our Pokemon's species, rest are 0xFF
    party_buffer[1] = pokemon_data[0]; // Species ID from Pokemon data
    for (int i = 2; i < 7; i++) {
        party_buffer[i] = 0xFF; // Terminator/empty slots
    }
    
    // Convert 415-byte individual Pokemon data to 44-byte party format
    // Mapping based on Generation I Pokemon data structure:
    uint8_t* party_pokemon = &party_buffer[8]; // Start of first Pokemon slot (after count + species list)
    
    // Copy essential Pokemon data (44 bytes total)
    party_pokemon[0] = pokemon_data[0];   // Species
    party_pokemon[1] = pokemon_data[1];   // Current HP high byte
    party_pokemon[2] = pokemon_data[1];   // Current HP low byte (duplicate for simplicity)
    party_pokemon[3] = pokemon_data[2];   // Level
    party_pokemon[4] = pokemon_data[3];   // Status condition
    party_pokemon[5] = pokemon_data[4];   // Type 1
    party_pokemon[6] = pokemon_data[5];   // Type 2
    party_pokemon[7] = pokemon_data[6];   // Catch rate/hold item
    
    // Moves (4 bytes)
    party_pokemon[8] = pokemon_data[8];   // Move 1
    party_pokemon[9] = pokemon_data[9];   // Move 2
    party_pokemon[10] = pokemon_data[10]; // Move 3
    party_pokemon[11] = pokemon_data[11]; // Move 4
    
    // OT ID (2 bytes)
    party_pokemon[12] = pokemon_data[12]; // OT ID high
    party_pokemon[13] = pokemon_data[13]; // OT ID low
    
    // Experience (3 bytes)
    party_pokemon[14] = pokemon_data[14]; // Experience high
    party_pokemon[15] = pokemon_data[15]; // Experience mid
    party_pokemon[16] = pokemon_data[16]; // Experience low
    
    // HP EV (2 bytes)
    party_pokemon[17] = pokemon_data[17];
    party_pokemon[18] = pokemon_data[18];
    
    // Attack EV (2 bytes)
    party_pokemon[19] = pokemon_data[19];
    party_pokemon[20] = pokemon_data[20];
    
    // Defense EV (2 bytes)
    party_pokemon[21] = pokemon_data[21];
    party_pokemon[22] = pokemon_data[22];
    
    // Speed EV (2 bytes)
    party_pokemon[23] = pokemon_data[23];
    party_pokemon[24] = pokemon_data[24];
    
    // Special EV (2 bytes)
    party_pokemon[25] = pokemon_data[25];
    party_pokemon[26] = pokemon_data[26];
    
    // IV data (2 bytes)
    party_pokemon[27] = pokemon_data[27]; // Attack/Defense IV
    party_pokemon[28] = pokemon_data[28]; // Speed/Special IV
    
    // Move PP (4 bytes)
    party_pokemon[29] = pokemon_data[29]; // Move 1 PP
    party_pokemon[30] = pokemon_data[30]; // Move 2 PP
    party_pokemon[31] = pokemon_data[31]; // Move 3 PP
    party_pokemon[32] = pokemon_data[32]; // Move 4 PP
    
    // Level (1 byte)
    party_pokemon[33] = pokemon_data[33];
    
    // Current stats (10 bytes) - use values from original Pokemon data
    party_pokemon[34] = pokemon_data[52]; // Max HP low
    party_pokemon[35] = pokemon_data[51]; // Max HP high
    party_pokemon[36] = pokemon_data[54]; // Attack low
    party_pokemon[37] = pokemon_data[53]; // Attack high
    party_pokemon[38] = pokemon_data[56]; // Defense low
    party_pokemon[39] = pokemon_data[55]; // Defense high
    party_pokemon[40] = pokemon_data[58]; // Speed low
    party_pokemon[41] = pokemon_data[57]; // Speed high
    party_pokemon[42] = pokemon_data[60]; // Special low
    party_pokemon[43] = pokemon_data[59]; // Special high
    
    // Copy OT names (11 bytes * 6 Pokemon = 66 bytes starting at offset 8 + 6*44 = 272)
    uint8_t* ot_names = &party_buffer[272];
    memcpy(ot_names, &pokemon_data[63], 11); // Copy first OT name
    
    // Copy nicknames (11 bytes * 6 Pokemon = 66 bytes starting at offset 272 + 66 = 338)
    uint8_t* nicknames = &party_buffer[338];
    memcpy(nicknames, &pokemon_data[52], 11); // Copy first nickname
    
    printf("Created party data: count=%d, species=0x%02X\n", party_buffer[0], party_buffer[1]);
    return true;
}

bool extract_pokemon_from_party(const uint8_t* party_data, uint8_t slot, uint8_t* pokemon_buffer) {
    if (party_data == NULL || pokemon_buffer == NULL) {
        printf("ERROR: NULL pointers in extract_pokemon_from_party\n");
        return false;
    }
    
    if (slot >= 6 || slot >= party_data[0]) {
        printf("ERROR: Invalid slot %d (party has %d Pokemon)\n", slot, party_data[0]);
        return false;
    }
    
    // Clear the Pokemon buffer first
    memset(pokemon_buffer, 0, POKEMON_DATA_SIZE);
    
    // Extract Pokemon data from the party structure
    const uint8_t* party_pokemon = &party_data[8 + (slot * 44)]; // Each Pokemon is 44 bytes
    
    // Convert 44-byte party format back to 415-byte individual format
    // This is a simplified conversion - copy the essential data
    pokemon_buffer[0] = party_pokemon[0];   // Species
    pokemon_buffer[1] = party_pokemon[1];   // HP
    pokemon_buffer[2] = party_pokemon[3];   // Level
    pokemon_buffer[3] = party_pokemon[4];   // Status
    pokemon_buffer[4] = party_pokemon[5];   // Type 1
    pokemon_buffer[5] = party_pokemon[6];   // Type 2
    pokemon_buffer[6] = party_pokemon[7];   // Catch rate
    
    // Copy moves
    memcpy(&pokemon_buffer[8], &party_pokemon[8], 4);
    
    // Copy OT ID, Experience, EVs, IVs, Move PP
    memcpy(&pokemon_buffer[12], &party_pokemon[12], 21);
    
    // Copy level again
    pokemon_buffer[33] = party_pokemon[33];
    
    // Copy stats (convert back to little-endian format)
    pokemon_buffer[51] = party_pokemon[35]; // Max HP high
    pokemon_buffer[52] = party_pokemon[34]; // Max HP low
    pokemon_buffer[53] = party_pokemon[37]; // Attack high
    pokemon_buffer[54] = party_pokemon[36]; // Attack low
    pokemon_buffer[55] = party_pokemon[39]; // Defense high
    pokemon_buffer[56] = party_pokemon[38]; // Defense low
    pokemon_buffer[57] = party_pokemon[41]; // Speed high
    pokemon_buffer[58] = party_pokemon[40]; // Speed low
    pokemon_buffer[59] = party_pokemon[43]; // Special high
    pokemon_buffer[60] = party_pokemon[42]; // Special low
    
    // Copy OT name and nickname from their respective sections
    const uint8_t* ot_names = &party_data[272];
    const uint8_t* nicknames = &party_data[338];
    
    memcpy(&pokemon_buffer[63], &ot_names[slot * 11], 11);
    memcpy(&pokemon_buffer[52], &nicknames[slot * 11], 11);
    
    printf("Extracted Pokemon from party slot %d: species=0x%02X, level=%d\n", 
           slot, pokemon_buffer[0], pokemon_buffer[2]);
    return true;
}

void debug_party_data(const uint8_t* party_data, const char* title) {
    printf("\n=== %s ===\n", title);
    if (party_data == NULL) {
        printf("Party data is NULL\n");
        return;
    }
    
    printf("Party count: %d\n", party_data[0]);
    printf("Species list: ");
    for (int i = 1; i < 7; i++) {
        if (party_data[i] == 0xFF) {
            printf("FF ");
        } else {
            printf("%02X ", party_data[i]);
        }
    }
    printf("\n");
    
    // Show first Pokemon data if present
    if (party_data[0] > 0) {
        const uint8_t* first_pokemon = &party_data[8];
        printf("First Pokemon: species=0x%02X, level=%d, HP=%d/%d\n", 
               first_pokemon[0], first_pokemon[3], 
               first_pokemon[1], (first_pokemon[35] << 8) | first_pokemon[34]);
    }
    
    printf("Party data size: %d bytes\n", PARTY_DATA_SIZE);
    printf("==========================\n\n");
}

// Additional protocol constants not in header
#define PKMN_CONNECTED_II       0x61
#define ITEM_1_SELECTED         0xD4  // TRADE_CENTRE
#define ITEM_2_SELECTED         0xD5  // COLOSSEUM  
#define ITEM_3_SELECTED         0xD6  // BREAK_LINK

#define SERIAL_PREAMBLE_BYTE    0xFD
#define SERIAL_NO_DATA_BYTE     0xFE
#define SERIAL_RNS_LENGTH       10
#define SERIAL_TRADE_PREAMBLE_LENGTH 9
#define SERIAL_PATCH_LIST_PART_TERMINATOR 0xFF

#define PKMN_TRADE_ACCEPT_GEN_I 0x62
#define PKMN_TRADE_REJECT_GEN_I 0x61
#define PKMN_TABLE_LEAVE_GEN_I  0x6f
#define PKMN_SEL_NUM_MASK_GEN_I 0x60
#define PKMN_SEL_NUM_ONE_GEN_I  0x60

// Timing constants (in microseconds)
#define BIT_TIMEOUT_US      1000
#define BYTE_TIMEOUT_US     10000
#define CONNECTION_TIMEOUT_US 5000000  // 5 seconds

static uint64_t last_bit_time = 0;
static uint64_t last_byte_time = 0;

// Trade data exchange - now handles party data (404 bytes) instead of individual Pokemon (415 bytes)
static uint8_t received_pokemon_data[PARTY_DATA_SIZE];
static size_t trade_data_counter = 0;
static bool patch_pt_2 = false;
static uint8_t in_pkmn_idx = 0;

// Bidirectional trading support
static uint8_t selected_pokemon_slot = 0;
static uint8_t receive_pokemon_slot = 0;
static bool bidirectional_mode = false;

// Trade center negotiation tracking
static bool trade_center_confirmed = false;
static int negotiation_attempts = 0;
static int consecutive_ff_count = 0;
static int trade_init_attempts = 0;
static uint64_t negotiation_start_time = 0;

// Output data for sending to Game Boy
static uint8_t output_byte = 0x00;
static int output_bit_pos = 7;

// Simplified interrupt handler - minimal processing to avoid crashes
static volatile bool isr_error = false;
static volatile uint32_t isr_call_count = 0;

void gb_clock_isr() {
    isr_call_count++;
    
    // Safety check - prevent runaway interrupts
    if (isr_call_count > 10000) {
        isr_error = true;
        return;
    }
    
    // Very simple interrupt handler - just track basic state
    bool clock_high = gpio_get(GB_CLK_PIN);
    last_bit_time = time_us_64();
    
    if (clock_high) {
        // Rising edge - read data from Game Boy
        if (bit_count < 8) {
            shift_register <<= 1;
            if (gpio_get(GB_SO_PIN)) {
                shift_register |= 1;
            }
            bit_count++;
            
            if (bit_count >= 8) {
                // Byte complete
                last_received = shift_register;
                transfer_complete = true;
                bit_count = 0;
                shift_register = 0;
            }
        }
    } else {
        // Falling edge - send data to Game Boy
        if (output_bit_pos >= 0 && output_bit_pos < 8) {
            gpio_put(GB_SI_PIN, (output_byte >> output_bit_pos) & 1);
            output_bit_pos--;
        } else {
            gpio_put(GB_SI_PIN, 1); // Default high
        }
    }
}

// Function to set the next byte to send
void gb_link_set_output_byte(uint8_t byte) {
    output_byte = byte;
    output_bit_pos = 7;
}

// Post-trade cleanup function
void gb_link_post_trade_cleanup(void) {
    printf("Entering post-trade cleanup phase...\n");
    
    int cleanup_attempts = 0;
    const int max_cleanup_attempts = 500; // Allow up to 500 response cycles
    int consecutive_timeouts = 0;
    const int max_consecutive_timeouts = 50; // Stop after 50 consecutive timeouts
    bool communication_ended = false;
    
    while (cleanup_attempts < max_cleanup_attempts && !communication_ended) {
        uint64_t start_time = time_us_64();
        bool timeout = false;
        
        // Wait for incoming data with timeout
        while (!transfer_complete && (time_us_64() - start_time) < 1000000) { // 1 second timeout
            sleep_ms(1);
        }
        
        if (!transfer_complete) {
            consecutive_timeouts++;
            printf("Cleanup timeout %d (attempt %d)\n", consecutive_timeouts, cleanup_attempts);
            
            if (consecutive_timeouts >= max_consecutive_timeouts) {
                printf("Too many consecutive timeouts - ending cleanup\n");
                break;
            }
            timeout = true;
        } else {
            consecutive_timeouts = 0; // Reset timeout counter
        }
        
        uint8_t in = timeout ? 0x00 : last_received;
        transfer_complete = false;
        
        // Send cleanup response
        gb_link_set_output_byte(0x62);
        
        if (!timeout) {
            printf("Post-trade response %d: sent 0x62, received 0x%02X\n", cleanup_attempts, in);
        }
        
        // Log potential end signals but don't terminate - let timeout handle the end
        if (in == 0x00) {
            printf("Game Boy sent 0x00 (potential intermediate signal) - continuing cleanup\n");
        } else if (in == 0xFF) {
            printf("Game Boy sent 0xFF (potential error signal) - continuing cleanup\n");
        }
        
        cleanup_attempts++;
        sleep_ms(50); // Small delay between cleanup attempts
    }
    
    printf("Post-trade cleanup completed after %d responses\n", cleanup_attempts);
    
    // Final delay to ensure Game Boy has finished all processing
    sleep_ms(3000);
    
    printf("Trade session fully completed - ready for new connections\n");
}

void gb_link_set_selected_pokemon_slot(uint8_t slot) {
    selected_pokemon_slot = slot;
}

uint8_t gb_link_get_selected_pokemon_slot(void) {
    return selected_pokemon_slot;
}

bool gb_link_init(void) {
    // Initialize GPIO pins
    gpio_init(GB_CLK_PIN);
    gpio_set_dir(GB_CLK_PIN, GPIO_IN);
    gpio_pull_up(GB_CLK_PIN);  // Pull-up to prevent floating
    
    gpio_init(GB_SO_PIN);
    gpio_set_dir(GB_SO_PIN, GPIO_IN);
    gpio_pull_up(GB_SO_PIN);   // Pull-up to prevent floating
    
    gpio_init(GB_SI_PIN);
    gpio_set_dir(GB_SI_PIN, GPIO_OUT);
    gpio_put(GB_SI_PIN, 1);    // Default high
    
    // Initialize protocol state
    current_state = TRADE_STATE_NOT_CONNECTED;
    gameboy_status = GAMEBOY_CONN_FALSE;
    trade_centre_state = TRADE_RESET;
    
    // Initialize transfer state
    transfer_complete = false;
    bit_count = 0;
    shift_register = 0;
    last_received = 0x00;
    
    // Initialize output state
    output_byte = PKMN_BLANK;
    output_bit_pos = 7;
    
    // Initialize timing variables to prevent false connections
    last_bit_time = 0;
    last_byte_time = 0;
    
    // Set up interrupt on clock pin
    gpio_set_irq_enabled_with_callback(GB_CLK_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, 
                                       true, &gb_clock_isr);
    
    printf("Game Boy link initialized on pins: CLK=%d, SO=%d, SI=%d\n", 
           GB_CLK_PIN, GB_SO_PIN, GB_SI_PIN);
    return true;
}

void gb_link_deinit(void) {
    gpio_set_irq_enabled(GB_CLK_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
    
    gpio_deinit(GB_CLK_PIN);
    gpio_deinit(GB_SO_PIN);
    gpio_deinit(GB_SI_PIN);
    
    current_state = TRADE_STATE_NOT_CONNECTED;
}

uint8_t gb_link_transfer_byte(uint8_t send_byte) {
    // Just set the byte we want to send - the interrupt handler will send it
    gb_link_set_output_byte(send_byte);
    return last_received;
}

bool gb_link_wait_for_connection(void) {
    // Check for recent clock activity indicating a Game Boy connection
    uint64_t current_time = time_us_64();
    
    // If we've seen recent clock activity, check if it looks like a connection attempt
    if ((current_time - last_bit_time) < 500000) { // 500ms window
        // Look for PKMN_MASTER bytes which indicate the Game Boy is trying to connect
        if (last_received == PKMN_MASTER) {
            printf("Game Boy connection detected (MASTER byte received)\n");
            current_state = TRADE_STATE_CONNECTED;
            // DON'T set gameboy_status yet - let the protocol handler do it
            return true;
        }
        
        // Also check for CONNECTED bytes
        if (last_received == PKMN_CONNECTED || last_received == PKMN_CONNECTED_II) {
            printf("Game Boy connection confirmed\n");
            current_state = TRADE_STATE_CONNECTED;
            gameboy_status = GAMEBOY_CONN_TRUE;
            return true;
        }
    }
    
    return false;
}

gb_trade_state_t gb_link_get_state(void) {
    return current_state;
}

void gb_link_set_state(gb_trade_state_t state) {
    current_state = state;
}

// Helper functions for protocol responses
static uint8_t get_connect_response(uint8_t in_data) {
    uint8_t ret = in_data;
    
    switch(in_data) {
        case PKMN_CONNECTED:
        case PKMN_CONNECTED_II:
            gameboy_status = GAMEBOY_CONN_TRUE;
            ret = in_data; // Echo back the connected byte
            printf("Connection confirmed with byte 0x%02X\n", in_data);
            break;
        case PKMN_MASTER:
            ret = PKMN_SLAVE; // Respond as slave
            // Stay in GAMEBOY_CONN_FALSE until we get CONNECTED bytes
            printf("Game Boy is master, we are slave - waiting for connection confirmation\n");
            break;
        case PKMN_BLANK:
            ret = PKMN_BLANK;
            break;
        default:
            // Don't immediately break link, just echo back unknown bytes
            ret = in_data;
            break;
    }
    
    return ret;
}

static uint8_t get_menu_response(uint8_t in_data) {
    uint8_t response = PKMN_BLANK;
    
    // Count all menu interactions after trade center confirmation
    if (trade_center_confirmed) {
        negotiation_attempts++;
    }
    
    switch(in_data) {
        case PKMN_CONNECTED:
        case PKMN_CONNECTED_II:
            printf("Connection status byte (0x%02X) during menu\n", in_data);
            response = in_data; // Echo back
            break;
        case ITEM_1_SELECTED: // Trade Centre selected
            if (trade_center_confirmed) {
                // If we've already confirmed once, try different responses to advance
                if (negotiation_attempts > 2) {
                    printf("Extended D4 sequence - trying 0x00 to advance (attempt %d)\n", negotiation_attempts);
                    response = 0x00;
                    gameboy_status = GAMEBOY_READY;
                    current_state = TRADE_STATE_READY;
                    trade_centre_state = TRADE_RESET;
                } else {
                    printf("Trade Center re-confirmed! Responding with 0xD4 (attempt %d)\n", negotiation_attempts);
                    response = in_data; // Echo back
                }
            } else {
                printf("Trade Centre selected - initial confirmation\n");
                response = in_data; // Echo back
                trade_center_confirmed = true;
                negotiation_start_time = time_us_64(); // Start negotiation timer
            }
            break;
        case ITEM_2_SELECTED: // Colosseum selected
            gameboy_status = GAMEBOY_COLOSSEUM;
            response = in_data; // Echo back
            break;
        case ITEM_3_SELECTED: // Break Link selected
        case PKMN_MASTER:
            gameboy_status = GAMEBOY_CONN_FALSE;
            current_state = TRADE_STATE_NOT_CONNECTED;
            response = ITEM_3_SELECTED;
            // Reset negotiation state
            trade_center_confirmed = false;
            negotiation_attempts = 0;
            break;
        case PKMN_BLANK:
            if (trade_center_confirmed && negotiation_attempts > 2) {
                printf("Blank negotiation after trade center selection - responding with 0xD0 (attempt %d)\n", negotiation_attempts);
                response = 0xD0;
                // After several 0xD0 responses, try to advance
                if (negotiation_attempts > 4) {
                    printf("Extended blank negotiation - trying to advance to trade protocol\n");
                    gameboy_status = GAMEBOY_READY;
                    current_state = TRADE_STATE_READY;
                    trade_centre_state = TRADE_RESET;
                }
            } else {
                printf("Blank byte during early negotiation - echoing back\n");
                response = in_data;
            }
            break;
        default:
            printf("Unknown menu byte: 0x%02X\n", in_data);
            response = in_data;
            break;
    }
    
    return response;
}

static uint8_t get_trade_centre_response(uint8_t in_data, uint8_t* pokemon_data) {
    uint8_t send = in_data;
    
    switch(trade_centre_state) {
        case TRADE_RESET:
            // Reset counters and static variables
            trade_data_counter = 0;
            patch_pt_2 = false;
            trade_centre_state = TRADE_INIT;
            break;
            
        case TRADE_INIT:
            trade_init_attempts++;
            
            // If we've been stuck in TRADE_INIT for too long, just skip ahead to data exchange
            if (trade_init_attempts > 50) {
                printf("TRADE_INIT: Stuck for %d attempts, forcing advance to TRADE_DATA for Pokemon reception\n", trade_init_attempts);
                trade_centre_state = TRADE_DATA;
                trade_data_counter = 0;
                trade_init_attempts = 0;
                gameboy_status = GAMEBOY_WAITING;
                send = in_data; // Echo whatever they sent
                break;
            }
            
            if(in_data == SERIAL_PREAMBLE_BYTE) {
                trade_data_counter++;
                consecutive_ff_count = 0; // Reset FF counter
                gameboy_status = GAMEBOY_WAITING;
                printf("TRADE_INIT: Received preamble %d/%d (attempt %d)\n", trade_data_counter, SERIAL_RNS_LENGTH, trade_init_attempts);
            } else if (in_data == 0xFF) {
                consecutive_ff_count++;
                printf("TRADE_INIT: Received 0xFF #%d (attempt %d)\n", consecutive_ff_count, trade_init_attempts);
                
                // Try different responses based on how many 0xFF we've seen
                if (consecutive_ff_count < 10) {
                    // First few: respond with preamble
                    send = SERIAL_PREAMBLE_BYTE;
                    printf("TRADE_INIT: Responding with preamble (0xFD)\n");
                } else if (consecutive_ff_count < 20) {
                    // If preamble isn't working, try echoing back
                    send = 0xFF;
                    printf("TRADE_INIT: Echoing 0xFF back\n");
                } else {
                    // After many attempts, force advance
                    printf("TRADE_INIT: Too many 0xFF bytes, forcing advance to TRADE_DATA\n");
                    trade_centre_state = TRADE_DATA;
                    trade_data_counter = 0;
                    consecutive_ff_count = 0;
                    trade_init_attempts = 0;
                    send = 0xFF;
                }
                gameboy_status = GAMEBOY_WAITING;
            } else if (in_data == PKMN_BLANK) {
                // Sometimes Game Boy sends blank bytes during init
                consecutive_ff_count = 0; // Reset FF counter
                trade_data_counter++; // Count blank bytes as progress too
                printf("TRADE_INIT: Received blank byte, counting as progress %d/%d (attempt %d)\n", trade_data_counter, SERIAL_RNS_LENGTH, trade_init_attempts);
                send = PKMN_BLANK;
            } else {
                // For any other byte, just count it as progress - Game Boy might be trying to advance
                trade_data_counter++;
                printf("TRADE_INIT: Unexpected byte 0x%02X, counting as progress %d/%d (attempt %d)\n", in_data, trade_data_counter, SERIAL_RNS_LENGTH, trade_init_attempts);
                send = in_data; // Echo back
            }
            
            if(trade_data_counter >= SERIAL_RNS_LENGTH) {
                trade_centre_state = TRADE_RANDOM;
                trade_data_counter = 0;
                trade_init_attempts = 0;
                printf("TRADE_INIT complete, advancing to TRADE_RANDOM\n");
            }
            break;
            
        case TRADE_RANDOM:
            trade_data_counter++;
            if(trade_data_counter == (SERIAL_RNS_LENGTH + SERIAL_TRADE_PREAMBLE_LENGTH)) {
                trade_centre_state = TRADE_DATA;
                trade_data_counter = 0;
            }
            break;
            
        case TRADE_DATA:
            // Exchange trade block data using party format (404 bytes)
            if (trade_data_counter >= PARTY_DATA_SIZE) {
                printf("ERROR: Party data overflow, resetting trade\n");
                trade_centre_state = TRADE_RESET;
                break;
            }
            
            received_pokemon_data[trade_data_counter] = in_data;
            
            // Send party data instead of individual Pokemon data
            send = party_buffer[trade_data_counter];
            trade_data_counter++;
            
            if(trade_data_counter == PARTY_DATA_SIZE) {
                trade_centre_state = TRADE_PATCH_HEADER;
                trade_data_counter = 0;
                printf("Party data exchange complete (%d bytes)\n", PARTY_DATA_SIZE);
            }
            break;
            
        case TRADE_PATCH_HEADER:
            if(in_data == SERIAL_PREAMBLE_BYTE) {
                trade_data_counter++;
            }
            
            if(trade_data_counter == 6) {
                trade_data_counter = 0;
                trade_centre_state = TRADE_PATCH_DATA;
            }
            break;
            
        case TRADE_PATCH_DATA:
            trade_data_counter++;
            // Send blank bytes for patch list (simplified)
            if(trade_data_counter > 8) {
                send = PKMN_BLANK;
            }
            
            // Handle received patch data (simplified)
            switch(in_data) {
                case PKMN_BLANK:
                    break;
                case SERIAL_PATCH_LIST_PART_TERMINATOR:
                    patch_pt_2 = true;
                    break;
                default:
                    // Apply patches to received party data with bounds checking
                    if(!patch_pt_2 && in_data > 0 && (in_data - 1) < PARTY_DATA_SIZE) {
                        // Only allow patches to non-critical party fields
                        uint16_t patch_offset = in_data - 1;
                        // Avoid patching party count, species list, or core Pokemon data
                        if (patch_offset >= 50) { // Skip critical party structure fields
                            received_pokemon_data[patch_offset] = SERIAL_NO_DATA_BYTE;
                        }
                    }
                    break;
            }
            
            if(trade_data_counter == 196) {
                trade_centre_state = TRADE_SELECT;
                trade_data_counter = 0;
            }
            break;
            
        case TRADE_SELECT:
            in_pkmn_idx = 0;
            if(in_data == PKMN_BLANK) {
                trade_centre_state = TRADE_PENDING;
            }
            break;
            
        case TRADE_PENDING:
            if(in_data == PKMN_TABLE_LEAVE_GEN_I) {
                trade_centre_state = TRADE_RESET;
                send = PKMN_TABLE_LEAVE_GEN_I;
                gameboy_status = GAMEBOY_READY;
            } else if((in_data & PKMN_SEL_NUM_MASK_GEN_I) == PKMN_SEL_NUM_MASK_GEN_I) {
                in_pkmn_idx = in_data;
                // Send our selected Pokemon slot instead of always slot 1
                send = PKMN_SEL_NUM_MASK_GEN_I | (selected_pokemon_slot & 0x0F);
                gameboy_status = GAMEBOY_TRADE_PENDING;
            } else if(in_data == PKMN_BLANK && in_pkmn_idx != 0) {
                send = 0;
                trade_centre_state = TRADE_CONFIRMATION;
                in_pkmn_idx &= 0x0F;
            }
            break;
            
        case TRADE_CONFIRMATION:
            if(in_data == PKMN_TRADE_REJECT_GEN_I) {
                trade_centre_state = TRADE_SELECT;
                gameboy_status = GAMEBOY_WAITING;
            } else if(in_data == PKMN_TRADE_ACCEPT_GEN_I) {
                trade_centre_state = TRADE_DONE;
                send = PKMN_TRADE_ACCEPT_GEN_I;
            }
            break;
            
        case TRADE_DONE:
            if(in_data == PKMN_BLANK) {
                printf("Pokemon trade completed! Received party data from Game Boy\n");
                
                // Debug the received party data
                debug_party_data(received_pokemon_data, "RECEIVED PARTY DATA FROM GAME BOY");
                
                // Extract the first Pokemon from the received party data
                static uint8_t extracted_pokemon[POKEMON_DATA_SIZE];
                if (extract_pokemon_from_party(received_pokemon_data, 0, extracted_pokemon)) {
                    printf("Successfully extracted Pokemon from received party\n");
                    
                    // Display the extracted Pokemon data
                    extern void display_pokemon_data(const uint8_t* pokemon_data, const char* title);
                    display_pokemon_data(extracted_pokemon, "EXTRACTED POKEMON FROM RECEIVED PARTY");
                    
                    // In bidirectional mode, save extracted Pokemon to designated slot
                    if (bidirectional_mode) {
                        extern bool storage_save_pokemon(uint8_t slot, const uint8_t* pokemon_data, size_t data_len);
                        if (storage_save_pokemon(receive_pokemon_slot, extracted_pokemon, POKEMON_DATA_SIZE)) {
                            printf("Received Pokemon saved to slot %d\n", receive_pokemon_slot);
                        } else {
                            printf("Failed to save received Pokemon to slot %d\n", receive_pokemon_slot);
                        }
                    } else {
                        // Legacy mode: copy extracted data over current Pokemon buffer
                        memcpy(pokemon_data, extracted_pokemon, POKEMON_DATA_SIZE);
                    }
                } else {
                    printf("ERROR: Failed to extract Pokemon from received party data\n");
                }
                
                // Mark trade as complete and trigger cleanup
                trade_centre_state = TRADE_RESET;
                gameboy_status = GAMEBOY_TRADING;
                send = PKMN_BLANK; // Send blank to acknowledge completion
            }
            break;
            
        case TRADE_CANCEL:
            if(in_data == PKMN_TABLE_LEAVE_GEN_I) {
                trade_centre_state = TRADE_RESET;
                gameboy_status = GAMEBOY_READY;
            }
            send = PKMN_TABLE_LEAVE_GEN_I;
            break;
            
        default:
            break;
    }
    
    return send;
}

// Check for ISR errors and reset if needed
bool gb_link_check_isr_health(void) {
    if (isr_error) {
        printf("ERROR: ISR error detected, resetting interrupt handler\n");
        
        // Disable interrupt
        gpio_set_irq_enabled(GB_CLK_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
        
        // Reset state
        isr_error = false;
        isr_call_count = 0;
        transfer_complete = false;
        bit_count = 0;
        shift_register = 0;
        output_bit_pos = 7;
        
        // Re-enable interrupt
        gpio_set_irq_enabled_with_callback(GB_CLK_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, 
                                           true, &gb_clock_isr);
        
        return false;
    }
    return true;
}

// Simpler continuous protocol handler that runs in the background
void gb_link_handle_protocol_step(uint8_t* pokemon_data) {
    // Check ISR health first
    if (!gb_link_check_isr_health()) {
        return;
    }
    
    if (!transfer_complete) {
        return; // No new data to process
    }
    
    transfer_complete = false;
    uint8_t response = PKMN_BLANK;
    
    // Safety check
    if (pokemon_data == NULL) {
        printf("ERROR: pokemon_data is NULL\n");
        return;
    }
    
    printf("Received: 0x%02X, Status: %d, Trade State: %d, ISR calls: %lu\n", 
           last_received, gameboy_status, trade_centre_state, isr_call_count);
    
    // Reset ISR call counter periodically
    if (isr_call_count > 1000) {
        isr_call_count = 0;
    }
    
    // Handle the received byte based on current state
    printf("Current gameboy_status: %d\n", gameboy_status);
    
    switch(gameboy_status) {
        case GAMEBOY_CONN_FALSE:
            printf("Handling connection attempt\n");
            response = get_connect_response(last_received);
            printf("get_connect_response returned: 0x%02X\n", response);
            break;
        case GAMEBOY_CONN_TRUE:
            printf("Handling menu selection\n");
            response = get_menu_response(last_received);
            
            // Check if we should advance to trade protocol after sufficient negotiation
            uint64_t negotiation_time = negotiation_start_time > 0 ? (time_us_64() - negotiation_start_time) : 0;
            if (trade_center_confirmed && (negotiation_attempts > 3 || negotiation_time > 10000000)) { // 10 seconds max
                printf("Negotiation complete - advancing to trade protocol (attempts: %d, time: %llu ms)\n", 
                       negotiation_attempts, negotiation_time / 1000);
                gameboy_status = GAMEBOY_READY;
                current_state = TRADE_STATE_READY;
                trade_centre_state = TRADE_RESET; // Make sure trade state is ready
            }
            break;
        case GAMEBOY_COLOSSEUM:
            response = last_received; // Echo back for colosseum
            break;
        default:
            printf("Handling trade state: %d\n", gameboy_status);
            
            // Handle special bytes that might need different responses
            if (last_received == 0x62) {
                printf("Received cleanup byte 0x62, responding with acknowledgment\n");
                response = 0x62; // Echo back cleanup byte
            } else {
                // Let the trade center response handler deal with all trade protocol bytes
                response = get_trade_centre_response(last_received, pokemon_data);
            }
            break;
    }
    
    // Set the response byte for the next transfer
    gb_link_set_output_byte(response);
    printf("Responding with: 0x%02X\n", response);
}

bool gb_link_trade_or_store(uint8_t* pokemon_data, size_t data_len) {
    printf("Trade protocol handler active - will respond to Game Boy automatically\n");
    
    // Disable bidirectional mode for legacy compatibility
    bidirectional_mode = false;
    
    // Convert individual Pokemon data to party format
    if (!create_party_from_pokemon(pokemon_data, party_buffer)) {
        printf("ERROR: Failed to create party data from Pokemon\n");
        return false;
    }
    
    // Debug the party data we created
    debug_party_data(party_buffer, "PARTY DATA TO SEND");
    
    // Reset trade state and negotiation tracking
    trade_centre_state = TRADE_RESET;
    trade_center_confirmed = false;
    negotiation_attempts = 0;
    consecutive_ff_count = 0;
    trade_init_attempts = 0;
    trade_data_counter = 0;
    negotiation_start_time = 0;
    
    // The actual protocol handling happens in the main loop via gb_link_handle_protocol_step
    // This function just sets up and waits for completion
    uint64_t start_time = time_us_64();
    
    while ((time_us_64() - start_time) < 120000000) { // 2 minute timeout
        // Handle each protocol step as data comes in
        gb_link_handle_protocol_step(pokemon_data);
        
        // Check if trade completed
        if (gameboy_status == GAMEBOY_TRADING && trade_centre_state == TRADE_RESET) {
            printf("Trade completed successfully! Starting post-trade cleanup...\n");
            
            // Run dedicated post-trade cleanup
            gb_link_post_trade_cleanup();
            
            return true;
        }
        
        // Check for connection loss - much longer timeout for full trade duration
        uint64_t current_time = time_us_64();
        
        // Fix potential timer overflow by checking if last_bit_time is reasonable
        if (last_bit_time == 0 || last_bit_time > current_time) {
            last_bit_time = current_time; // Reset if invalid
        }
        
        uint64_t time_since_activity = current_time - last_bit_time;
        
        // Debug timing every 30 seconds
        static uint64_t last_debug_time = 0;
        if ((current_time - last_debug_time) > 30000000) {
            printf("Trade activity: %llu seconds since last bit (current=%llu, last=%llu)\n", 
                   time_since_activity / 1000000, current_time / 1000000, last_bit_time / 1000000);
            last_debug_time = current_time;
        }
        
        if (time_since_activity > 300000000) { // 5 minute timeout
            printf("No activity for %llu seconds - Game Boy may have disconnected\n", time_since_activity / 1000000);
            current_state = TRADE_STATE_NOT_CONNECTED;
            gameboy_status = GAMEBOY_CONN_FALSE;
            return false;
        }
        
        sleep_ms(10);
    }
    
    printf("Trade protocol timeout\n");
    return false;
}

bool gb_link_bidirectional_trade(uint8_t send_slot, uint8_t receive_slot) {
    printf("Starting bidirectional trade: sending slot %d, receiving to slot %d\n", send_slot, receive_slot);
    
    // Enable bidirectional mode
    bidirectional_mode = true;
    selected_pokemon_slot = send_slot;
    receive_pokemon_slot = receive_slot;
    
    // Load Pokemon data from the selected slot
    uint8_t send_pokemon_data[POKEMON_DATA_SIZE];
    size_t data_len;
    
    extern bool storage_load_pokemon(uint8_t slot, uint8_t* pokemon_data, size_t* data_len);
    if (!storage_load_pokemon(send_slot, send_pokemon_data, &data_len)) {
        printf("Failed to load Pokemon from slot %d\n", send_slot);
        bidirectional_mode = false;
        return false;
    }
    
    printf("Loaded Pokemon from slot %d for trading\n", send_slot);
    extern void display_pokemon_data(const uint8_t* pokemon_data, const char* title);
    display_pokemon_data(send_pokemon_data, "POKEMON TO SEND");
    
    // Convert individual Pokemon data to party format
    if (!create_party_from_pokemon(send_pokemon_data, party_buffer)) {
        printf("ERROR: Failed to create party data from Pokemon\n");
        bidirectional_mode = false;
        return false;
    }
    
    // Debug the party data we created
    debug_party_data(party_buffer, "PARTY DATA TO SEND (BIDIRECTIONAL)");
    
    // Reset trade state and negotiation tracking
    trade_centre_state = TRADE_RESET;
    trade_center_confirmed = false;
    negotiation_attempts = 0;
    consecutive_ff_count = 0;
    trade_init_attempts = 0;
    trade_data_counter = 0;
    negotiation_start_time = 0;
    
    uint64_t start_time = time_us_64();
    
    while ((time_us_64() - start_time) < 120000000) { // 2 minute timeout
        // Handle each protocol step as data comes in
        gb_link_handle_protocol_step(send_pokemon_data);
        
        // Check if trade completed
        if (gameboy_status == GAMEBOY_TRADING && trade_centre_state == TRADE_RESET) {
            printf("Bidirectional trade completed successfully! Starting post-trade cleanup...\n");
            
            // Run dedicated post-trade cleanup
            gb_link_post_trade_cleanup();
            
            bidirectional_mode = false;
            return true;
        }
        
        // Check for connection loss - much longer timeout for full trade duration
        uint64_t current_time = time_us_64();
        
        // Fix potential timer overflow by checking if last_bit_time is reasonable
        if (last_bit_time == 0 || last_bit_time > current_time) {
            last_bit_time = current_time; // Reset if invalid
        }
        
        uint64_t time_since_activity = current_time - last_bit_time;
        
        // Debug timing every 30 seconds
        static uint64_t last_debug_time_bi = 0;
        if ((current_time - last_debug_time_bi) > 30000000) {
            printf("Bidirectional trade activity: %llu seconds since last bit (current=%llu, last=%llu)\n", 
                   time_since_activity / 1000000, current_time / 1000000, last_bit_time / 1000000);
            last_debug_time_bi = current_time;
        }
        
        if (time_since_activity > 300000000) { // 5 minute timeout
            printf("No activity for %llu seconds - Game Boy may have disconnected\n", time_since_activity / 1000000);
            current_state = TRADE_STATE_NOT_CONNECTED;
            gameboy_status = GAMEBOY_CONN_FALSE;
            bidirectional_mode = false;
            return false;
        }
        
        sleep_ms(10);
    }
    
    printf("Bidirectional trade timeout\n");
    bidirectional_mode = false;
    return false;
}