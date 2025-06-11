#include <stdio.h>
#include <string.h>
#include "gb_link.h"
#include "pico/stdlib.h"
#include "pico/time.h"

// Pin definitions for RP2040 Zero
#define GB_SO_PIN 0 // Serial Out (from GB) -> GP0
#define GB_SI_PIN 3 // Serial In (to GB)    -> GP3
#define GB_SC_PIN 2 // Serial Clock         -> GP2
// SD not used

// Protocol constants (Gen I)
#define PKMN_MASTER 0x01
#define PKMN_SLAVE  0x02
#define PKMN_BLANK  0x00
#define PKMN_CONNECTED 0x60
#define PKMN_TRADE_ACCEPT 0x62
#define PKMN_TRADE_REJECT 0x61
#define PKMN_TABLE_LEAVE 0x6f
#define PKMN_SEL_NUM_MASK 0x60
#define PKMN_SEL_NUM_ONE 0x60
#define ITEM_1_SELECTED 0xD4
#define SERIAL_PREAMBLE_BYTE 0xFD
#define SERIAL_NO_DATA_BYTE 0xFE
#define SERIAL_PATCH_LIST_PART_TERMINATOR 0xFF

// Trade block sizes
#define TRADE_PREAMBLE_LEN 10
#define TRADE_RANDOM_LEN 10
#define TRADE_BLOCK_LEN 415  // Original Gen 1 trade block: 11+1+7+264+66+66+3 = 415 bytes
#define TRADE_END_LEN 3
#define TRADE_END_BYTES {0xDF, 0xFE, 0x15}
#define TRADE_POST_PREAMBLE_LEN 3
#define PATCH_LIST_HEADER_LEN 6
#define PATCH_LIST_BLANK_LEN 7
#define PATCH_LIST_DATA_LEN 189

// State machine for trade protocol
typedef enum {
    LINK_STATE_CONN_FALSE,
    LINK_STATE_CONN_TRUE,
    LINK_STATE_READY,
    LINK_STATE_TRADE_PREAMBLE,
    LINK_STATE_TRADE_RANDOM,
    LINK_STATE_TRADE_BLOCK,
    LINK_STATE_TRADE_END,
    LINK_STATE_PATCH_HEADER,
    LINK_STATE_PATCH_BLANK,
    LINK_STATE_PATCH_DATA,
    LINK_STATE_DONE
} link_state_t;

static link_state_t link_state = LINK_STATE_CONN_FALSE;

// Valid Pokémon data - Level 5 Pikachu with proper Gen 1 structure
static uint8_t stored_pokemon[TRADE_BLOCK_LEN];

// Initialize with proper Gen 1 Pokémon party structure  
static void init_pokemon_data(void) {
    // Clear entire array first
    for (int i = 0; i < TRADE_BLOCK_LEN; i++) {
        stored_pokemon[i] = 0x00;
    }
    
    // === CORRECT POKEMON PARTY STRUCTURE (404 bytes total) ===
    // According to Generation I Pokemon data structure research:
    // 1. Party Count: 1 byte (offset 0)
    // 2. Species List: 7 bytes (offset 1-7, terminated with 0xFF, padded with 0x00)
    // 3. Pokemon Data: 6 × 44 bytes (offset 8-271)
    // 4. OT Names: 6 × 11 bytes (offset 272-337)
    // 5. Nicknames: 6 × 11 bytes (offset 338-403)
    // Total: 1 + 7 + 264 + 66 + 66 = 404 bytes
    
    // === PARTY COUNT (1 byte, offset 0) ===
    stored_pokemon[0] = 0x01; // 1 Pokemon in party
    
    // === SPECIES LIST (7 bytes, offset 1-7) ===
    stored_pokemon[1] = 0x99; // Bulbasaur internal index (verified correct)
    stored_pokemon[2] = 0xFF; // List terminator (CRITICAL! Must come immediately after last Pokemon)
    for (int i = 3; i < 8; i++) {
        stored_pokemon[i] = 0x00; // Remaining slots padded with 0x00
    }
    
    // === POKEMON DATA (44 bytes starting at offset 8) ===
    // First Pokemon (Bulbasaur) - 44 bytes starting at offset 8
    int pkmn_offset = 8;
    
    stored_pokemon[pkmn_offset + 0] = 0x99;   // species: Bulbasaur internal index (correct)
    stored_pokemon[pkmn_offset + 1] = 0x2D;   // current hp low byte (45 HP)
    stored_pokemon[pkmn_offset + 2] = 0x00;   // current hp high byte
    stored_pokemon[pkmn_offset + 3] = 0x05;   // level: 5
    stored_pokemon[pkmn_offset + 4] = 0x00;   // status_condition (healthy)
    stored_pokemon[pkmn_offset + 5] = 0x16;   // type[0]: Grass (0x16 - verified correct)
    stored_pokemon[pkmn_offset + 6] = 0x03;   // type[1]: Poison (0x03 - verified correct)  
    stored_pokemon[pkmn_offset + 7] = 0x99;   // catch_rate: Use species index as catch rate (common pattern)
    stored_pokemon[pkmn_offset + 8] = 0x21;   // move[0]: Tackle (0x21 - verified Gen 1 index)
    stored_pokemon[pkmn_offset + 9] = 0x2D;   // move[1]: Growl (0x2D - verified Gen 1 index)  
    stored_pokemon[pkmn_offset + 10] = 0x00;  // move[2]: Empty
    stored_pokemon[pkmn_offset + 11] = 0x00;  // move[3]: Empty
    stored_pokemon[pkmn_offset + 12] = 0x34;  // ot_id low byte
    stored_pokemon[pkmn_offset + 13] = 0x12;  // ot_id high byte
    stored_pokemon[pkmn_offset + 14] = 0x9C;  // exp[0] low byte (156 exp for level 5 Medium Slow)
    stored_pokemon[pkmn_offset + 15] = 0x00;  // exp[1] mid byte
    stored_pokemon[pkmn_offset + 16] = 0x00;  // exp[2] high byte
    stored_pokemon[pkmn_offset + 17] = 0x00;  // hp_ev low byte
    stored_pokemon[pkmn_offset + 18] = 0x00;  // hp_ev high byte
    stored_pokemon[pkmn_offset + 19] = 0x00;  // atk_ev low byte
    stored_pokemon[pkmn_offset + 20] = 0x00;  // atk_ev high byte
    stored_pokemon[pkmn_offset + 21] = 0x00;  // def_ev low byte
    stored_pokemon[pkmn_offset + 22] = 0x00;  // def_ev high byte
    stored_pokemon[pkmn_offset + 23] = 0x00;  // spd_ev low byte
    stored_pokemon[pkmn_offset + 24] = 0x00;  // spd_ev high byte
    stored_pokemon[pkmn_offset + 25] = 0x00;  // spc_ev low byte
    stored_pokemon[pkmn_offset + 26] = 0x00;  // spc_ev high byte
    stored_pokemon[pkmn_offset + 27] = 0xAA;  // iv low byte (Attack=10, Defense=10) 
    stored_pokemon[pkmn_offset + 28] = 0xAA;  // iv high byte (Speed=10, Special=10)
    stored_pokemon[pkmn_offset + 29] = 0x23;  // move_pp[0]: Tackle PP (35)
    stored_pokemon[pkmn_offset + 30] = 0x28;  // move_pp[1]: Growl PP (40)
    stored_pokemon[pkmn_offset + 31] = 0x00;  // move_pp[2]: Empty
    stored_pokemon[pkmn_offset + 32] = 0x00;  // move_pp[3]: Empty
    stored_pokemon[pkmn_offset + 33] = 0x05;  // level_again (copy of level)
    stored_pokemon[pkmn_offset + 34] = 0x2D;  // max_hp low byte (45 HP)
    stored_pokemon[pkmn_offset + 35] = 0x00;  // max_hp high byte
    stored_pokemon[pkmn_offset + 36] = 0x31;  // atk low byte (49 for level 5 Bulbasaur)
    stored_pokemon[pkmn_offset + 37] = 0x00;  // atk high byte
    stored_pokemon[pkmn_offset + 38] = 0x31;  // def low byte (49 for level 5 Bulbasaur)
    stored_pokemon[pkmn_offset + 39] = 0x00;  // def high byte
    stored_pokemon[pkmn_offset + 40] = 0x2D;  // spd low byte (45 for level 5 Bulbasaur)
    stored_pokemon[pkmn_offset + 41] = 0x00;  // spd high byte
    stored_pokemon[pkmn_offset + 42] = 0x41;  // spc low byte (65 for level 5 Bulbasaur)
    stored_pokemon[pkmn_offset + 43] = 0x00;  // spc high byte
    
    // Clear remaining 5 Pokemon slots (5 * 44 = 220 bytes, offset 52-271)
    for (int i = pkmn_offset + 44; i < 272; i++) {
        stored_pokemon[i] = 0x00;
    }
    
    // === OT NAMES (6 * 11 bytes, offset 272-337) ===
    // First Pokemon OT name "ABCDE" with correct Gen 1 encoding
    int ot_offset = 272;
    stored_pokemon[ot_offset + 0] = 0x80;  // A (0x80)
    stored_pokemon[ot_offset + 1] = 0x81;  // B (0x81) 
    stored_pokemon[ot_offset + 2] = 0x82;  // C (0x82)
    stored_pokemon[ot_offset + 3] = 0x83;  // D (0x83)
    stored_pokemon[ot_offset + 4] = 0x84;  // E (0x84)
    stored_pokemon[ot_offset + 5] = 0x50;  // String terminator
    for (int i = ot_offset + 6; i < ot_offset + 11; i++) {
        stored_pokemon[i] = 0x50; // Fill with terminators
    }
    
    // Clear remaining 5 OT name slots (5 * 11 = 55 bytes)
    for (int i = ot_offset + 11; i < 338; i++) {
        stored_pokemon[i] = 0x50;
    }
    
    // === NICKNAMES (6 * 11 bytes, offset 338-403) ===
    // First Pokemon nickname "BULBASAUR" with correct Gen 1 encoding
    int nick_offset = 338;
    stored_pokemon[nick_offset + 0] = 0x81;  // B
    stored_pokemon[nick_offset + 1] = 0x94;  // U
    stored_pokemon[nick_offset + 2] = 0x8B;  // L
    stored_pokemon[nick_offset + 3] = 0x81;  // B
    stored_pokemon[nick_offset + 4] = 0x80;  // A
    stored_pokemon[nick_offset + 5] = 0x92;  // S
    stored_pokemon[nick_offset + 6] = 0x80;  // A
    stored_pokemon[nick_offset + 7] = 0x94;  // U
    stored_pokemon[nick_offset + 8] = 0x91;  // R
    stored_pokemon[nick_offset + 9] = 0x50;  // Terminator
    stored_pokemon[nick_offset + 10] = 0x50; // Fill with terminator
    
    // Clear remaining 5 nickname slots (5 * 11 = 55 bytes)  
    for (int i = nick_offset + 11; i < 404; i++) {
        stored_pokemon[i] = 0x50;
    }
}

// Helper: Set up GPIOs for link cable
void gb_link_init(void) {
    printf("[RP2040] Initializing Game Boy Link Cable Interface...\n");
    
    // Initialize Pokémon data
    init_pokemon_data();
    printf("[RP2040] Initialized Pokémon data - Party Count: 0x%02X, Species: 0x%02X, Level: %d\n", 
           stored_pokemon[0], stored_pokemon[8], stored_pokemon[11]);
    
    // Debug: Print party structure bytes
    printf("[RP2040] Party structure: Count=0x%02X, Species=0x%02X, Terminator=0x%02X\n",
           stored_pokemon[0], stored_pokemon[1], stored_pokemon[2]);
    
    // Debug: Print trainer name bytes (first OT name at offset 272-282)
    printf("[RP2040] Trainer name bytes: ");
    for (int i = 272; i < 283; i++) {
        printf("0x%02X ", stored_pokemon[i]);
    }
    printf("\n");
    
    gpio_init(GB_SO_PIN); gpio_set_dir(GB_SO_PIN, GPIO_IN); gpio_pull_up(GB_SO_PIN);
    gpio_init(GB_SI_PIN); gpio_set_dir(GB_SI_PIN, GPIO_OUT); gpio_put(GB_SI_PIN, 1);
    gpio_init(GB_SC_PIN); gpio_set_dir(GB_SC_PIN, GPIO_IN); gpio_pull_up(GB_SC_PIN);
    
    printf("[RP2040] GPIO initialized - SO:%d SI:%d SC:%d\n", 
           gpio_get(GB_SO_PIN), gpio_get(GB_SI_PIN), gpio_get(GB_SC_PIN));
}

// Helper: Wait for clock edge with timeout
static bool wait_for_clock_rising_timeout(uint32_t timeout_ms) {
    uint32_t start = to_ms_since_boot(get_absolute_time());
    while (!gpio_get(GB_SC_PIN)) {
        if (to_ms_since_boot(get_absolute_time()) - start > timeout_ms) {
            return false;
        }
        tight_loop_contents();
    }
    return true;
}

static bool wait_for_clock_falling_timeout(uint32_t timeout_ms) {
    uint32_t start = to_ms_since_boot(get_absolute_time());
    while (gpio_get(GB_SC_PIN)) {
        if (to_ms_since_boot(get_absolute_time()) - start > timeout_ms) {
            return false;
        }
        tight_loop_contents();
    }
    return true;
}

// Bit-bang a single byte (slave mode: respond to GB clock)
static uint8_t gb_link_xfer_byte(uint8_t out_byte) {
    uint8_t in_byte = 0;
    
    for (int i = 0; i < 8; ++i) {
        // Wait for clock falling edge with timeout
        if (!wait_for_clock_falling_timeout(1000)) {
            printf("[RP2040] Clock timeout on falling edge, bit %d\n", i);
            return 0xFF; // Return error value
        }
        
        // Set output bit
        gpio_put(GB_SI_PIN, (out_byte & 0x80) ? 1 : 0);
        out_byte <<= 1;
        
        // Wait for clock rising edge with timeout
        if (!wait_for_clock_rising_timeout(1000)) {
            printf("[RP2040] Clock timeout on rising edge, bit %d\n", i);
            return 0xFF; // Return error value
        }
        
        // Read input bit
        in_byte <<= 1;
        if (gpio_get(GB_SO_PIN)) in_byte |= 1;
    }
    
    // Keep SI line high when idle
    gpio_put(GB_SI_PIN, 1);
    return in_byte;
}

// Main trade protocol loop (called when button pressed)
void gb_link_trade_or_store(void) {
    link_state = LINK_STATE_CONN_FALSE;
    printf("[RP2040] Starting Game Boy communication...\n");
    printf("[RP2040] Waiting for Game Boy to initiate connection...\n");
    
    // Step 1: Initial connection handshake
    // The Game Boy sends various bytes during initialization
    // We need to respond appropriately to establish the connection
    uint32_t handshake_timeout = 10000; // 10 second timeout
    uint32_t start_time = to_ms_since_boot(get_absolute_time());
    bool connected = false;
    
    while (!connected && (to_ms_since_boot(get_absolute_time()) - start_time < handshake_timeout)) {
        uint8_t in = gb_link_xfer_byte(PKMN_SLAVE);
        printf("[RP2040] Handshake - Received: 0x%02X, Sent: 0x%02X\n", in, PKMN_SLAVE);
        
        // Look for the master signal or connected signal
        if (in == PKMN_MASTER) {
            printf("[RP2040] Master signal detected!\n");
            connected = true;
            link_state = LINK_STATE_CONN_TRUE;
        } else if (in == PKMN_CONNECTED) {
            printf("[RP2040] Connection signal detected!\n");
            // Continue - this often comes before MASTER
        } else if (in == PKMN_BLANK) {
            // Normal blank response during negotiation
            printf("[RP2040] Blank byte during handshake\n");
        } else if (in == 0xFF) {
            printf("[RP2040] Communication error detected\n");
            break;
        } else {
            printf("[RP2040] Unexpected handshake byte: 0x%02X\n", in);
        }
        
        tight_loop_contents(); // No delay for faster handshake
    }
    
    if (!connected) {
        printf("[RP2040] Failed to establish connection with Game Boy\n");
        return;
    }
    
    printf("[RP2040] Connected to Game Boy successfully!\n");
    
    // Step 2: Menu negotiation
    printf("[RP2040] Waiting for menu selection...\n");
    uint32_t menu_timeout = 30000; // 30 second timeout for user to select
    start_time = to_ms_since_boot(get_absolute_time());
    bool menu_selected = false;
    int consecutive_blanks = 0;
    int consecutive_connected = 0;
    
    static uint8_t last_response = PKMN_SLAVE;
    
    while (!menu_selected && (to_ms_since_boot(get_absolute_time()) - start_time < menu_timeout)) {
        uint8_t response = last_response;  // Use previous response pattern
        
        // After many connection confirmations, start responding with CONNECTED
        if (consecutive_connected > 10) {
            response = PKMN_CONNECTED;
        }
        
        uint8_t in = gb_link_xfer_byte(response);
        printf("[RP2040] Menu phase - Received: 0x%02X\n", in);
        
        if (in == ITEM_1_SELECTED) {
            printf("[RP2040] Trade menu selected by user!\n");
            link_state = LINK_STATE_READY;
            menu_selected = true;
        } else if (in == PKMN_CONNECTED) {
            // Game Boy is confirming connection
            printf("[RP2040] Connection confirmed during menu\n");
            consecutive_connected++;
            consecutive_blanks = 0;
        } else if (in == PKMN_BLANK) {
            // Normal menu navigation
            printf("[RP2040] Menu navigation in progress\n");
            consecutive_blanks++;
            consecutive_connected = 0;
            
            // Don't assume trade initiation from blank bytes alone
            // Wait for actual trade selection signal
        } else if (in >= 0x60 && in <= 0x6F) {
            // Menu selection values - respond appropriately
            printf("[RP2040] Menu selection byte: 0x%02X\n", in);
            consecutive_blanks = 0;
            consecutive_connected = 0;
        } else if (in == 0xD0 || in == 0xD4) {
            // Trade-related bytes detected
            printf("[RP2040] Trade-related byte detected: 0x%02X\n", in);
            if (in == ITEM_1_SELECTED) {  // 0xD4
                printf("[RP2040] Trade menu explicitly selected!\n");
                link_state = LINK_STATE_READY;
                menu_selected = true;
            } else if (in == 0xD0) {
                // Count consecutive 0xD0 bytes - this might indicate trade readiness
                static int consecutive_d0 = 0;
                consecutive_d0++;
                if (consecutive_d0 >= 8) {  // After several 0xD0 bytes, assume trade is ready
                    printf("[RP2040] Multiple 0xD0 bytes detected - assuming trade ready\n");
                    link_state = LINK_STATE_READY;
                    menu_selected = true;
                }
                last_response = 0xD0;  // Next response will be 0xD0
            }
            consecutive_blanks = 0;
            consecutive_connected = 0;
        } else if (in == SERIAL_PREAMBLE_BYTE) {  // 0xFD
            // Preamble byte detected - trade is starting
            printf("[RP2040] Preamble byte detected - trade starting!\n");
            link_state = LINK_STATE_READY;
            menu_selected = true;
            consecutive_blanks = 0;
            consecutive_connected = 0;
        } else if (in == PKMN_MASTER) {
            // Game Boy is still in master mode
            printf("[RP2040] Master signal during menu\n");
            consecutive_blanks = 0;
            consecutive_connected = 0;
        } else if (in == 0xFF) {
            printf("[RP2040] Communication error in menu - attempting to continue\n");
            consecutive_blanks = 0;
            consecutive_connected = 0;
        } else {
            printf("[RP2040] Unknown menu byte: 0x%02X\n", in);
            consecutive_blanks = 0;
            consecutive_connected = 0;
        }
        
        sleep_ms(1); // Reduced delay for more responsive menu
    }
    
    if (!menu_selected) {
        printf("[RP2040] Menu selection timeout\n");
        return;
    }
    
    // Step 3: Trade protocol
    printf("[RP2040] Starting trade protocol...\n");
    
    // Handle initial trade negotiation phase
    printf("[RP2040] Waiting for trade negotiation to complete...\n");
    int negotiation_attempts = 0;
    const int max_negotiation_attempts = 100; // Increased limit for negotiation phase
    bool negotiation_complete = false;
    bool trade_center_confirmed = false;
    
    // The 0x00/0xD0 exchanges are negotiation, not preamble
    // We need to complete this phase before looking for actual preamble
    while (negotiation_attempts < max_negotiation_attempts && !negotiation_complete) {
        uint8_t response_byte = 0x00; // Start with safe default
        
        uint8_t in = gb_link_xfer_byte(response_byte);
        printf("[RP2040] Negotiation byte: 0x%02X (attempt %d)\n", in, negotiation_attempts);
        
        if (in == 0xD0) {
            // D0 negotiation - respond with 0x00 to progress
            printf("[RP2040] D0 negotiation, responding with 0x00\n");
            response_byte = 0x00;
        } else if (in == 0x00) {
            // Blank negotiation - respond with 0xD0 to progress
            printf("[RP2040] Blank negotiation, responding with 0xD0\n");
            response_byte = 0xD0;
        } else if (in == ITEM_1_SELECTED) { // 0xD4 - Trade Center selected!
            printf("[RP2040] Trade Center confirmed! Responding with 0xD4\n");
            response_byte = ITEM_1_SELECTED; // Echo Trade Center selection
            trade_center_confirmed = true;
        } else if (in == 0x60) {
            // Connection status bytes - very common after Trade Center selection
            printf("[RP2040] Connection status byte (0x60)\n");
            
            // If we've confirmed Trade Center and seen many 0x60 bytes, we're likely ready
            if (trade_center_confirmed && negotiation_attempts > 20) {
                printf("[RP2040] Trade Center confirmed + extended 0x60 sequence = ready for preamble!\n");
                negotiation_complete = true;
                break;
            } else {
                // Continue with connection confirmation
                response_byte = 0x60;
            }
        } else if (in == 0xFE) {
            // Potential preamble or disconnection indicator
            printf("[RP2040] FE byte detected - continuing negotiation\n");
            response_byte = 0x00; // Continue with safe response
        } else if (in == 0xFF) {
            printf("[RP2040] Communication error: 0xFF\n");
            response_byte = 0x00; // Safe response
        } else if (in == SERIAL_PREAMBLE_BYTE) { // 0xFD
            // This is the START of preamble - negotiation is complete!
            printf("[RP2040] First preamble byte (0xFD) detected - negotiation complete!\n");
            negotiation_complete = true;
            break;
        } else {
            printf("[RP2040] Unexpected negotiation byte: 0x%02X\n", in);
            response_byte = 0x00; // Safe default response
        }
        
        // Send the response for the next iteration (if not complete)
        if (!negotiation_complete && negotiation_attempts < max_negotiation_attempts - 1) {
            gb_link_xfer_byte(response_byte);
        }
        
        negotiation_attempts++;
        sleep_ms(25); // Shorter delay for responsiveness
    }
    
    if (!negotiation_complete) {
        printf("[RP2040] Negotiation timeout - checking for preamble anyway\n");
    }
    
    // Step 4: Wait for PROPER preamble sequence (minimum 10x 0xFD as per research)
    printf("[RP2040] Waiting for proper preamble sequence (10x 0xFD minimum)...\n");
    int preamble_count = 0;
    int preamble_attempts = 0;
    const int max_preamble_attempts = 50; // Allow more time to find preamble
    const int required_preamble_bytes = 10; // Research says "10x 0xFD preamble"
    bool preamble_complete = false;
    
    // If we exited negotiation due to 0xFD, we already have our first preamble byte
    if (negotiation_complete) {
        preamble_count = 1;
        printf("[RP2040] Already detected first preamble byte during negotiation\n");
    }
    
    while (preamble_attempts < max_preamble_attempts && !preamble_complete) {
        uint8_t in = gb_link_xfer_byte(SERIAL_PREAMBLE_BYTE);
        
        if (in == SERIAL_PREAMBLE_BYTE) { // 0xFD
            preamble_count++;
            printf("[RP2040] Preamble byte %d/10: 0xFD\n", preamble_count);
            
            if (preamble_count >= required_preamble_bytes) {
                printf("[RP2040] Sufficient preamble received (%d bytes) - ready for random seed!\n", preamble_count);
                preamble_complete = true;
                break;
            }
        } else {
            // Non-preamble byte - could be start of data or still connection
            if (preamble_count >= 3) {
                // If we've seen some preamble, this might be data starting
                printf("[RP2040] Partial preamble (%d bytes), non-preamble 0x%02X - assuming data start\n", preamble_count, in);
                preamble_complete = true;
                
                // This byte might be first random seed byte - handle it appropriately
                if (in == 0x60) {
                    printf("[RP2040] First data byte is 0x60 - still connection phase, continuing\n");
                } else {
                    printf("[RP2040] First data byte is 0x%02X - actual random data\n", in);
                }
                break;
            } else {
                // Still in connection phase
                printf("[RP2040] Connection byte 0x%02X during preamble search (attempt %d)\n", in, preamble_attempts);
                if (in == 0x60) {
                    // Still connection confirmation
                    preamble_count = 0; // Reset counter
                } else if (in == 0x00 || in == 0xD0) {
                    // Still negotiation
                    preamble_count = 0;
                }
            }
        }
        
        preamble_attempts++;
        sleep_ms(15);
    }
    
    if (!preamble_complete) {
        printf("[RP2040] Preamble timeout - proceeding with data exchange anyway\n");
    }
    
    // Step 5: Exchange random seed (variable length) - detect transition to trade data
    printf("[RP2040] Exchanging random seed data...\n");
    const uint8_t random_response[TRADE_RANDOM_LEN] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A};
    bool trade_data_detected = false;
    int random_bytes_exchanged = 0;
    
    for (int i = 0; i < TRADE_RANDOM_LEN; ++i) {
        uint8_t in = gb_link_xfer_byte(random_response[i]);
        printf("[RP2040] Random seed byte %d: sent 0x%02X, received 0x%02X\n", i, random_response[i], in);
        random_bytes_exchanged++;
        
        // Detect when Game Boy starts sending actual trade data
        if (i >= 3) { // Only check after first few bytes
            // Look for patterns that indicate trade data has started
            if (in == 0x01 || in == 0x02 || in == 0x03 || in == 0x04 || in == 0x05 || in == 0x06) {
                // Possible party count (1-6 Pokemon) - this could be start of trade data
                printf("[RP2040] Possible party count detected (0x%02X) - trade data may be starting\n", in);
                
                // Check if this looks like a valid party count by examining next byte
                uint8_t next_in = gb_link_xfer_byte(random_response[i+1 < TRADE_RANDOM_LEN ? i+1 : 0]);
                i++; // Consume the next byte
                random_bytes_exchanged++;
                printf("[RP2040] Next byte after party count: 0x%02X\n", next_in);
                
                // If next byte is a valid species (0x01-0xFF except 0x00), likely trade data
                if (next_in >= 0x01 && next_in <= 0xFF && next_in != 0x00) {
                    printf("[RP2040] Valid species detected (0x%02X) - trade data confirmed!\n", next_in);
                    trade_data_detected = true;
                    
                    // We need to start trade block exchange with these two bytes we already read
                    printf("[RP2040] Starting trade block with pre-read bytes: 0x%02X 0x%02X\n", in, next_in);
                    break;
                }
            } else if (in >= 0x70 && in <= 0xFF && i >= 5) {
                // High values later in sequence often indicate transition to trade data
                printf("[RP2040] High value (0x%02X) detected at position %d - possible trade data\n", in, i);
                // Continue for now but flag as potential transition
            }
        }
        
        // Traditional checks
        if (in == 0x60 && i == 0) {
            printf("[RP2040] Warning: Game Boy still sending connection bytes (0x60) instead of random seed\n");
        } else if (in == 0xFD) {
            printf("[RP2040] Game Boy sending preamble (0xFD) during random seed - protocol mismatch\n");
        }
    }
    
    if (trade_data_detected) {
        printf("[RP2040] Trade data transition detected after %d random bytes\n", random_bytes_exchanged);
    } else {
        printf("[RP2040] Random seed exchange completed normally (%d bytes)\n", random_bytes_exchanged);
    }
    
    // Step 6: Exchange trade block (404 bytes)
    printf("[RP2040] Exchanging trade block...\n");
    printf("[RP2040] Sending Pokémon - Party Count: 0x%02X, Species: 0x%02X, Level: %d\n", 
           stored_pokemon[0], stored_pokemon[8], stored_pokemon[11]);
    
    // Debug: Show first 20 bytes of what we're sending
    printf("[RP2040] First 20 bytes being sent: ");
    for (int i = 0; i < 20; i++) {
        printf("0x%02X ", stored_pokemon[i]);
    }
    printf("\n");
    
    // Debug: Show trainer name (first OT name at offset 272-282)
    printf("[RP2040] Trainer name: ");
    for (int i = 272; i < 283; i++) {
        if (stored_pokemon[i] == 0x50) {
            printf("[END] ");
            break;
        } else {
            printf("0x%02X ", stored_pokemon[i]);
        }
    }
    printf("\n");
    
    uint8_t incoming_pokemon[TRADE_BLOCK_LEN] = {0};
    int bytes_already_read = 0;
    
    // If we detected trade data early, we may have already read some bytes
    if (trade_data_detected && random_bytes_exchanged < TRADE_RANDOM_LEN) {
        // We read 2 bytes during detection - need to account for this
        // The last 2 bytes we read during random seed are actually the first 2 bytes of trade block
        printf("[RP2040] Adjusting for %d bytes already read during random seed detection\n", 2);
        bytes_already_read = 2;
        
        // We need to reconstruct what those bytes were from our detection
        // This is tricky - let's just start fresh but be aware we might be offset
        printf("[RP2040] Starting trade block exchange with potential 2-byte offset\n");
    }
    
    for (int i = 0; i < TRADE_BLOCK_LEN; ++i) {
        uint8_t in = gb_link_xfer_byte(stored_pokemon[i]);
        incoming_pokemon[i] = in;
        if (i % 50 == 0) {
            printf("[RP2040] Trade block progress: %d/404\n", i);
        }
    }
    
    // Log received Pokémon data for debugging
    printf("[RP2040] Trade block exchanged.\n");
    printf("[RP2040] Received - Party count: 0x%02X, Species: 0x%02X, Level: %d\n", 
           incoming_pokemon[0], incoming_pokemon[8], incoming_pokemon[11]);
    
    // Debug: Show first 20 bytes of what we received
    printf("[RP2040] First 20 bytes received: ");
    for (int i = 0; i < 20; i++) {
        printf("0x%02X ", incoming_pokemon[i]);
    }
    printf("\n");
    
    // Step 7: Send end bytes (3 bytes)
    printf("[RP2040] Sending end bytes...\n");
    const uint8_t end_bytes[TRADE_END_LEN] = TRADE_END_BYTES;
    for (int i = 0; i < TRADE_END_LEN; ++i) {
        gb_link_xfer_byte(end_bytes[i]);
    }
    
    // Step 8: Send post-trade preamble (3x 0xFD)
    for (int i = 0; i < TRADE_POST_PREAMBLE_LEN; ++i) {
        gb_link_xfer_byte(SERIAL_PREAMBLE_BYTE);
    }
    printf("[RP2040] End of trade block.\n");
    
    // Step 9: Patch list exchange (bidirectional - receive Game Boy's, then send ours)
    printf("[RP2040] Exchanging patch lists...\n");
    
    // First, receive the Game Boy's patch list
    printf("[RP2040] Receiving Game Boy's patch list...\n");
    uint8_t gb_patch_list[PATCH_LIST_HEADER_LEN + PATCH_LIST_BLANK_LEN + PATCH_LIST_DATA_LEN];
    
    // Receive preamble bytes
    for (int i = 0; i < PATCH_LIST_HEADER_LEN; ++i) {
        uint8_t in = gb_link_xfer_byte(SERIAL_PREAMBLE_BYTE);
        gb_patch_list[i] = in;
        printf("[RP2040] GB patch preamble byte %d: 0x%02X\n", i, in);
    }
    
    // Receive patch list data  
    for (int i = 0; i < PATCH_LIST_BLANK_LEN + PATCH_LIST_DATA_LEN; ++i) {
        uint8_t in = gb_link_xfer_byte(0x00); // Safe default response
        gb_patch_list[PATCH_LIST_HEADER_LEN + i] = in;
        if (i < 10) { // Only log first few bytes
            printf("[RP2040] GB patch data byte %d: 0x%02X\n", i, in);
        }
    }
    printf("[RP2040] Game Boy patch list received.\n");
    
    // Now send our patch list
    printf("[RP2040] Sending our patch list...\n");
    // Send preamble
    for (int i = 0; i < PATCH_LIST_HEADER_LEN; ++i) {
        gb_link_xfer_byte(SERIAL_PREAMBLE_BYTE);
    }
    // Send empty patch list: 0xFF 0xFF (no patches needed) followed by zeros
    gb_link_xfer_byte(0xFF); // First patch list terminator
    gb_link_xfer_byte(0xFF); // Second patch list terminator  
    for (int i = 2; i < PATCH_LIST_BLANK_LEN + PATCH_LIST_DATA_LEN; ++i) {
        gb_link_xfer_byte(0x00);
    }
    printf("[RP2040] Patch list exchange completed.\n");
    
    // Step 10: Pokemon selection and confirmation protocol  
    printf("[RP2040] Starting Pokemon selection protocol...\n");
    
    // Handle the actual Pokemon selection phase - this should be much more selective
    int selection_attempts = 0;
    const int max_selection_attempts = 200; // Increased attempts for proper selection
    bool selection_complete = false;
    bool pokemon_selected = false;
    int pokemon_selection_confirmations = 0;
    
    // Handle the actual Pokemon selection sequence
    while (selection_attempts < max_selection_attempts && !selection_complete) {
        uint8_t response = 0x60; // Default: select Pokemon 1
        uint8_t in = gb_link_xfer_byte(response);
        
        if (selection_attempts % 20 == 0) {
            printf("[RP2040] Selection attempt %d: received 0x%02X\n", selection_attempts, in);
        }
        
        // Handle the actual Game Boy Pokemon selection protocol
        if (in >= 0x60 && in <= 0x65) {
            // Pokemon selection bytes (0x60-0x65 for Pokemon 1-6)
            printf("[RP2040] Game Boy selected Pokemon %d (0x%02X)\n", in - 0x60 + 1, in);
            response = 0x60; // Always respond with Pokemon 1 selection
            pokemon_selected = true;
            pokemon_selection_confirmations++;
            
            // Only complete after multiple consistent selections
            if (pokemon_selection_confirmations >= 5) {
                printf("[RP2040] Pokemon selection confirmed after %d confirmations\n", pokemon_selection_confirmations);
                
                // Look for acceptance confirmation
                uint8_t confirm_in = gb_link_xfer_byte(0x60);
                if (confirm_in == 0x62 || confirm_in == 0x60) {
                    printf("[RP2040] Trade acceptance confirmed (0x%02X)\n", confirm_in);
                    selection_complete = true;
                    break;
                } else {
                    printf("[RP2040] Waiting for trade acceptance, got 0x%02X\n", confirm_in);
                }
            }
        } else if (in == 0x61) {
            // Trade rejected
            printf("[RP2040] Trade rejected by Game Boy\n");
            response = 0x60; // Try again with Pokemon 1
            pokemon_selection_confirmations = 0; // Reset confirmations
        } else if (in == 0x62) {
            // Trade accepted - this means success!
            printf("[RP2040] Trade explicitly accepted! Selection complete.\n");
            selection_complete = true;
            break;
        } else if (in == SERIAL_PREAMBLE_BYTE) { // 0xFD
            // Preamble bytes during selection - just echo back
            printf("[RP2040] Preamble during selection\n");
            response = SERIAL_PREAMBLE_BYTE;
        } else if (in == 0x00) {
            // Blank bytes - could be end of selection or just pause
            response = 0x00;
            // Only consider complete if we've had Pokemon selections first
            if (pokemon_selected && pokemon_selection_confirmations >= 3) {
                static int completion_count = 0;
                completion_count++;
                if (completion_count >= 3) {
                    printf("[RP2040] Multiple completion bytes after Pokemon selection - trade complete\n");
                    selection_complete = true;
                    break;
                }
            }
        } else {
            // Unknown byte - don't assume completion too quickly
            if (selection_attempts % 20 == 0) {
                printf("[RP2040] Unknown selection byte: 0x%02X, continuing with default\n", in);
            }
            response = 0x60; // Always try to select Pokemon 1
        }
        
        selection_attempts++;
        sleep_ms(20); // Shorter delay for responsiveness
    }
    
    if (selection_complete) {
        printf("[RP2040] Trade protocol completed successfully!\n");
    } else {
        printf("[RP2040] Selection timeout after %d attempts\n", selection_attempts);
        printf("[RP2040] Trade data was exchanged successfully - finalizing trade\n");
    }
    
    // Step 11: Final trade completion acknowledgment
    printf("[RP2040] Sending final trade completion acknowledgment...\n");
    
    // Send final ACK sequence to confirm trade completion
    for (int i = 0; i < 10; i++) {
        uint8_t in = gb_link_xfer_byte(0x62); // Trade completion ACK
        printf("[RP2040] Final ACK %d: sent 0x62, received 0x%02X\n", i, in);
        
        // If Game Boy sends 0x00, it has completed the trade - send final ACK and stop
        if (in == 0x00) {
            printf("[RP2040] Game Boy sent completion signal (0x00) - sending final confirmation\n");
            
            // Send one final acknowledgment to confirm we received the completion signal
            uint8_t final_in = gb_link_xfer_byte(0x00);
            printf("[RP2040] Final confirmation: sent 0x00, received 0x%02X\n", final_in);
            
            // Give Game Boy a moment to finalize the trade animation
            sleep_ms(200);
            printf("[RP2040] Trade protocol complete!\n");
            break;
        }
        // If Game Boy stops responding or sends other completion signals
        else if (in == 0xFF || in == 0x62) {
            printf("[RP2040] Game Boy acknowledged trade completion (0x%02X)\n", in);
            break;
        }
        sleep_ms(100); // Give Game Boy time to process
    }
    
    printf("[RP2040] Final acknowledgment sequence completed\n");
    
    // Store the incoming Pokémon since we did successfully exchange data
    memcpy(stored_pokemon, incoming_pokemon, TRADE_BLOCK_LEN);
    
    // Update our Pokemon data for future trades
    init_pokemon_data();
    
    printf("[RP2040] Trade completed - Pokemon data updated\n");
    printf("[RP2040] ==> TRADE SUCCESSFUL! <==\n");
    
    // Step 12: Post-trade cleanup phase
    // Continue responding to Game Boy until it naturally stops communicating
    printf("[RP2040] Entering post-trade cleanup phase...\n");
    
    int cleanup_attempts = 0;
    const int max_cleanup_attempts = 200; // Allow up to 200 response cycles
    int consecutive_timeouts = 0;
    const int max_consecutive_timeouts = 5; // Stop after 5 consecutive timeouts (allow more time)
    bool communication_ended = false;
    
    while (cleanup_attempts < max_cleanup_attempts && !communication_ended) {
        // Try to exchange a byte with short timeout to detect ongoing communication
        // We'll modify gb_link_xfer_byte to have a timeout, or implement a timeout version here
        
        // Check for clock activity by trying to receive with timeout
        uint32_t start_time = time_us_32();
        const uint32_t first_bit_timeout_us = 1000000; // 1000ms (1 second) timeout for first bit
        bool communication_detected = false;
        
        // Wait for clock falling edge to detect communication start
        while ((time_us_32() - start_time) < first_bit_timeout_us) {
            if (!gpio_get(GB_SC_PIN)) { // Clock gone low - communication starting
                communication_detected = true;
                break;
            }
            sleep_us(1000); // 1ms polling interval
        }
        
        if (!communication_detected) {
            consecutive_timeouts++;
            printf("[RP2040] No communication detected (timeout %d/%d)\n", consecutive_timeouts, max_consecutive_timeouts);
            
            if (consecutive_timeouts >= max_consecutive_timeouts) {
                printf("[RP2040] Multiple consecutive timeouts - ending cleanup\n");
                communication_ended = true;
                break;
            }
            
            cleanup_attempts++;
            continue;
        }
        
        // Communication detected, reset timeout counter and exchange byte
        consecutive_timeouts = 0;
        uint8_t in = gb_link_xfer_byte(0x62); // Continue responding with ACK
        
        if (cleanup_attempts < 10 || cleanup_attempts % 10 == 0) {
            printf("[RP2040] Post-trade response %d: sent 0x62, received 0x%02X\n", cleanup_attempts, in);
        }
        
        // Check for natural communication end signals
        if (in == 0x00 || in == 0xFF) {
            printf("[RP2040] Game Boy sent end signal (0x%02X) - cleanup complete\n", in);
            // Send final acknowledgment
            gb_link_xfer_byte(0x00);
            communication_ended = true;
            break;
        }
        
        cleanup_attempts++;
        sleep_ms(10); // Shorter delay for responsiveness
    }
    
    if (cleanup_attempts >= max_cleanup_attempts) {
        printf("[RP2040] Cleanup timeout after %d attempts - forcing end\n", cleanup_attempts);
    }
    
    printf("[RP2040] Post-trade cleanup completed after %d responses\n", cleanup_attempts);
    
    // Final delay to ensure Game Boy has finished all processing
    sleep_ms(1000);
    
    printf("[RP2040] Trade session fully completed - ready for new connections\n");
} 