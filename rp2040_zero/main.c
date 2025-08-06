#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"

#include "gb_link.h"
#include "storage.h"
#include "ui.h"
#include "web_ui.h"

// LED pin is now defined in ui.h

// Static Pokemon data for Level 10 Bulbasaur (from Flipper source)
static const uint8_t default_pokemon_data[POKEMON_DATA_SIZE] = {
    // Pokemon data structure based on Generation I/II format
    0x99,  // Species: Bulbasaur
    0x1C,  // Current HP (28)
    0x0A,  // Level (10)
    0x00,  // Status condition
    0x04,  // Type 1: Grass
    0x03,  // Type 2: Poison
    0x00,  // Catch rate
    0x2D,  // Move 1: Tackle
    0x49,  // Move 2: Growl
    0x4A,  // Move 3: Leech Seed
    0x4D,  // Move 4: Vine Whip
    // OT ID
    0x12, 0x34,
    // Experience (1000 for level 10 Bulbasaur, big-endian)
    0x00, 0x03, 0xE8,
    // HP EV
    0x00, 0x00,
    // ATK EV
    0x00, 0x00,
    // DEF EV
    0x00, 0x00,
    // SPD EV
    0x00, 0x00,
    // SPC EV
    0x00, 0x00,
    // IV data (Attack/Defense in upper/lower nibbles)
    0xAA,
    // IV data (Speed/Special in upper/lower nibbles)  
    0xAA,
    // Move PP
    0x23, 0x28, 0x19, 0x0F,
    // Level again (padding)
    0x0A,
    // Max HP (2 bytes, little-endian)
    0x1C, 0x00,
    // Attack (2 bytes, little-endian)
    0x13, 0x00,
    // Defense (2 bytes, little-endian)
    0x13, 0x00,
    // Speed (2 bytes, little-endian)
    0x11, 0x00,
    // Special (2 bytes, little-endian)
    0x15, 0x00,
    // Nickname (BULBASAUR) - Pokemon internal character encoding
    0x81, 0x94, 0x8B, 0x81, 0x80, 0x92, 0x80, 0x94, 0x91, 0x50, 0x50,
    // OT Name (FLIPPER) - Pokemon internal character encoding  
    0x85, 0x8B, 0x88, 0x8F, 0x8F, 0x84, 0x91, 0x50, 0x50, 0x50, 0x50,
    // Padding to reach 415 bytes
    [66 ... 414] = 0x00
};

static uint8_t current_pokemon[POKEMON_DATA_SIZE];
static bool pokemon_loaded = false;

// HTTP command buffer for web UI
static char http_command_buffer[512];
static size_t http_command_len = 0;
static uint64_t last_char_time = 0;

void process_http_commands(void) {
    uint64_t current_time = time_us_64();
    
    // Check for incoming data on stdin (USB)
    int c = getchar_timeout_us(0);
    
    if (c != PICO_ERROR_TIMEOUT) {
        last_char_time = current_time;
        
        // Debug: show received characters (less verbose)
        if (c >= 32 && c < 127) {
            printf("RX: '%c'", c);
        } else {
            printf("RX: 0x%02X", c);
        }
        
        if (c == '\n' || c == '\r') {
            printf(" [NEWLINE]\n");
            if (http_command_len > 0) {
                http_command_buffer[http_command_len] = '\0';
                printf("Complete command received: '%s' (length: %d)\n", http_command_buffer, http_command_len);
                
                // Process the command
                if (strncmp(http_command_buffer, "GET ", 4) == 0) {
                    printf("\n=== HTTP REQUEST ===\n");
                    web_ui_handle_request(http_command_buffer);
                    printf("\n=== END HTTP RESPONSE ===\n");
                } else {
                    printf("Non-HTTP command: '%s'\n", http_command_buffer);
                }
                
                http_command_len = 0;
            }
        } else if (http_command_len < sizeof(http_command_buffer) - 1) {
            http_command_buffer[http_command_len++] = c;
            printf(" ");
        } else {
            printf(" [OVERFLOW]\n");
            http_command_len = 0;
        }
    } else {
        // No new character, check for timeout-based processing
        if (http_command_len > 0 && (current_time - last_char_time) > 100000) { // 100ms timeout
            http_command_buffer[http_command_len] = '\0';
            printf("\nTIMEOUT - Processing command: '%s' (length: %d)\n", http_command_buffer, http_command_len);
            
            // Process the command
            if (strncmp(http_command_buffer, "GET ", 4) == 0) {
                printf("\n=== HTTP REQUEST (TIMEOUT) ===\n");
                web_ui_handle_request(http_command_buffer);
                printf("\n=== END HTTP RESPONSE ===\n");
            } else {
                printf("Non-HTTP command (timeout): '%s'\n", http_command_buffer);
            }
            
            http_command_len = 0;
        }
    }
}

void core1_entry() {
    // Core 1 handles UI updates and web requests
    while (true) {
        ui_update();
        process_http_commands();
        sleep_ms(10);
    }
}

bool load_default_pokemon() {
    memcpy(current_pokemon, default_pokemon_data, POKEMON_DATA_SIZE);
    pokemon_loaded = true;
    return true;
}

void display_pokemon_data(const uint8_t* pokemon_data, const char* title) {
    printf("\n=== %s ===\n", title);
    
    if (pokemon_data == NULL) {
        printf("No Pokemon data available\n");
        return;
    }
    
    // Parse basic Pokemon data (Generation I format) with debugging
    printf("ANALYSIS OF RECEIVED DATA:\n");
    printf("Species ID: 0x%02X (%d)\n", pokemon_data[0], pokemon_data[0]);
    printf("Byte 1 (Current HP?): 0x%02X (%d)\n", pokemon_data[1], pokemon_data[1]);
    printf("Byte 2 (Level?): 0x%02X (%d)\n", pokemon_data[2], pokemon_data[2]);
    printf("Byte 3 (Status?): 0x%02X (%d)\n", pokemon_data[3], pokemon_data[3]);
    printf("Byte 4 (Type 1?): 0x%02X (%d)\n", pokemon_data[4], pokemon_data[4]);
    printf("Byte 5 (Type 2?): 0x%02X (%d)\n", pokemon_data[5], pokemon_data[5]);
    
    // Try different interpretations of HP (F35 = 62261 decimal)
    printf("\nHP ANALYSIS:\n");
    printf("If byte 1 is HP: %d\n", pokemon_data[1]);
    printf("If bytes 1-2 are HP (little-endian): %d\n", pokemon_data[1] | (pokemon_data[2] << 8));
    printf("If bytes 1-2 are HP (big-endian): %d\n", (pokemon_data[1] << 8) | pokemon_data[2]);
    
    // Look for the F35 pattern (0xF35 = 3893, 0x35F3 = 13811)
    printf("\nLOOKING FOR F35 PATTERN:\n");
    for (int i = 0; i < POKEMON_DATA_SIZE-1; i++) {
        uint16_t val_le = pokemon_data[i] | (pokemon_data[i+1] << 8);
        uint16_t val_be = (pokemon_data[i] << 8) | pokemon_data[i+1];
        if (val_le == 0xF35 || val_be == 0xF35 || val_le == 62261 || val_be == 62261) {
            printf("Found F35-like pattern at byte %d: LE=%04X(%d) BE=%04X(%d)\n", 
                   i, val_le, val_le, val_be, val_be);
        }
    }
    
    // Moves
    printf("Moves: 0x%02X, 0x%02X, 0x%02X, 0x%02X\n", 
           pokemon_data[8], pokemon_data[9], pokemon_data[10], pokemon_data[11]);
    
    // OT ID
    uint16_t ot_id = (pokemon_data[12] << 8) | pokemon_data[13];
    printf("OT ID: %d\n", ot_id);
    
    // Experience (3 bytes, big-endian) - but try different interpretations
    uint32_t exp_be = (pokemon_data[14] << 16) | (pokemon_data[15] << 8) | pokemon_data[16];
    uint32_t exp_le = pokemon_data[16] | (pokemon_data[15] << 8) | (pokemon_data[14] << 16);
    printf("Experience BE: %lu (0x%06X)\n", exp_be, exp_be);
    printf("Experience LE: %lu (0x%06X)\n", exp_le, exp_le);
    printf("Experience bytes: %02X %02X %02X\n", pokemon_data[14], pokemon_data[15], pokemon_data[16]);
    
    // Stats (2-byte little-endian values) - Fixed offsets
    printf("Max HP: %d\n", pokemon_data[33] | (pokemon_data[34] << 8));
    printf("Attack: %d\n", pokemon_data[35] | (pokemon_data[36] << 8));
    printf("Defense: %d\n", pokemon_data[37] | (pokemon_data[38] << 8));
    printf("Speed: %d\n", pokemon_data[39] | (pokemon_data[40] << 8));
    printf("Special: %d\n", pokemon_data[41] | (pokemon_data[42] << 8));
    
    // Nickname (11 bytes) - Fixed offset
    printf("Nickname: ");
    for (int i = 0; i < 11; i++) {
        uint8_t c = pokemon_data[43 + i];
        if (c == 0x50 || c == 0x00) break; // Terminator
        if (c >= 0x80 && c <= 0x99) {
            printf("%c", 'A' + (c - 0x80)); // Convert Pokemon encoding to ASCII
        } else if (c >= 0x00 && c <= 0x19) {
            printf("%c", 'A' + c);
        } else {
            printf("?");
        }
    }
    printf("\n");
    
    // OT Name (7 bytes) - Fixed offset  
    printf("OT Name: ");
    for (int i = 0; i < 7; i++) {
        uint8_t c = pokemon_data[54 + i];
        if (c == 0x50 || c == 0x00) break; // Terminator
        if (c >= 0x80 && c <= 0x99) {
            printf("%c", 'A' + (c - 0x80)); // Convert Pokemon encoding to ASCII
        } else if (c >= 0x00 && c <= 0x19) {
            printf("%c", 'A' + c);
        } else {
            printf("?");
        }
    }
    printf("\n");
    
    // Raw hex dump of first 128 bytes for debugging
    printf("\nRaw data (first 128 bytes for debugging):\n");
    for (int i = 0; i < 128 && i < POKEMON_DATA_SIZE; i++) {
        if (i % 16 == 0) printf("%04X: ", i);
        printf("%02X ", pokemon_data[i]);
        if (i % 16 == 15) printf("\n");
    }
    if (128 % 16 != 0) printf("\n");
    
    // Also show the full 415-byte structure in groups for analysis
    printf("\nFull 415-byte structure overview:\n");
    for (int i = 0; i < POKEMON_DATA_SIZE; i += 32) {
        printf("Bytes %03d-%03d: ", i, (i+31 < POKEMON_DATA_SIZE) ? i+31 : POKEMON_DATA_SIZE-1);
        for (int j = 0; j < 32 && (i+j) < POKEMON_DATA_SIZE; j++) {
            printf("%02X ", pokemon_data[i+j]);
        }
        printf("\n");
    }
    
    printf("========================\n\n");
}

bool handle_trade_process() {
    gb_trade_state_t initial_state = gb_link_get_state();
    
    printf("Starting trade process, current state: %d\n", initial_state);
    ui_show_status(initial_state);
    
    if (!pokemon_loaded) {
        if (!load_default_pokemon()) {
            ui_show_error("Failed to load Pokemon data");
            return false;
        }
    }
    
    // Attempt to trade or store Pokemon
    bool success = gb_link_trade_or_store(current_pokemon, POKEMON_DATA_SIZE);
    
    if (success) {
        ui_show_success("Trade completed!");
        // Optionally save the traded Pokemon to storage
        storage_save_pokemon(0, current_pokemon, POKEMON_DATA_SIZE);
    } else {
        ui_show_error("Trade failed");
    }
    
    return success;
}

bool handle_bidirectional_trade(uint8_t send_slot, uint8_t receive_slot) {
    gb_trade_state_t initial_state = gb_link_get_state();
    
    printf("Starting bidirectional trade process, current state: %d\n", initial_state);
    printf("Will send Pokemon from slot %d and receive to slot %d\n", send_slot, receive_slot);
    ui_show_status(initial_state);
    
    // Attempt bidirectional trade
    bool success = gb_link_bidirectional_trade(send_slot, receive_slot);
    
    if (success) {
        ui_show_success("Bidirectional trade completed!");
        printf("Trade successful: sent slot %d, received to slot %d\n", send_slot, receive_slot);
    } else {
        ui_show_error("Bidirectional trade failed");
    }
    
    return success;
}

int main() {
    stdio_init_all();
    
    printf("Pokemon Trade Tool for RP2040 Zero starting...\n");
    
    // Initialize all subsystems
    if (!ui_init()) {
        printf("Failed to initialize UI\n");
        return -1;
    }
    
    if (!storage_init()) {
        printf("Failed to initialize storage\n");
        return -1;
    }
    
    if (!gb_link_init()) {
        printf("Failed to initialize Game Boy link\n");
        return -1;
    }
    
    if (!web_ui_init()) {
        printf("Failed to initialize Web UI\n");
        return -1;
    }
    
    // Start UI core
    multicore_launch_core1(core1_entry);
    
    printf("Initialization complete. Waiting for Game Boy connection...\n");
    printf("Pin assignments: CLK=GP%d, SO=GP%d, SI=GP%d, LED=GP%d, BUTTON=GP%d\n", 
           GB_CLK_PIN, GB_SO_PIN, GB_SI_PIN, LED_PIN, BUTTON_PIN);
    printf("Game Boy Link configuration:\n");
    printf("  GPIO %d: Serial Clock (input with pullup)\n", GB_CLK_PIN);
    printf("  GPIO %d: Serial Out from Game Boy (input with pullup)\n", GB_SO_PIN);
    printf("  GPIO %d: Serial In to Game Boy (output, normally high)\n", GB_SI_PIN);
    printf("\n=== WEB UI TEST ===\n");
    printf("You can test the web UI by typing commands like:\n");
    printf("  GET /\n");
    printf("  GET /api/pokemon/list\n");
    printf("Or run the Python bridge: python3 usb_bridge.py\n");
    printf("==================\n");
    
    // Test LED directly first
    printf("Testing LED directly...\n");
    gpio_put(LED_PIN, 1);
    sleep_ms(1000);
    gpio_put(LED_PIN, 0);
    sleep_ms(1000);
    gpio_put(LED_PIN, 1);
    sleep_ms(1000);
    gpio_put(LED_PIN, 0);
    printf("Direct LED test complete\n");
    
    ui_set_led_pattern(LED_SLOW_BLINK);
    
    // Load default Pokemon
    load_default_pokemon();
    
    // Save default Pokemon to slot 0 if not already there
    uint8_t test_pokemon[POKEMON_DATA_SIZE];
    size_t test_len;
    if (!storage_load_pokemon(0, test_pokemon, &test_len)) {
        printf("Saving default Pokemon to slot 0\n");
        storage_save_pokemon(0, current_pokemon, POKEMON_DATA_SIZE);
    }
    
    // Display what we're sending
    display_pokemon_data(current_pokemon, "DEFAULT POKEMON (WHAT WE SEND)");
    
    // Check if there are any stored Pokemon from previous trades
    uint8_t stored_pokemon[POKEMON_DATA_SIZE];
    size_t stored_len;
    if (storage_load_pokemon(0, stored_pokemon, &stored_len)) {
        display_pokemon_data(stored_pokemon, "STORED POKEMON (SLOT 0)");
    } else {
        printf("No Pokemon stored in slot 0 yet\n");
    }
    
    // Display available Pokemon storage slots
    printf("\n=== POKEMON STORAGE STATUS ===\n");
    uint8_t slot_list[MAX_POKEMON_STORAGE];
    size_t slot_count;
    if (storage_list_pokemon(slot_list, MAX_POKEMON_STORAGE, &slot_count)) {
        printf("Found %zu stored Pokemon in slots: ", slot_count);
        for (size_t i = 0; i < slot_count; i++) {
            printf("%d ", slot_list[i]);
        }
        printf("\n");
    } else {
        printf("No stored Pokemon found\n");
    }
    printf("===============================\n");
    
    // Set initial state
    gb_link_set_state(TRADE_STATE_NOT_CONNECTED);
    
    printf("Current state: %d, LED should be slow blinking\n", gb_link_get_state());
    
    // Main loop
    int loop_count = 0;
    uint64_t last_heartbeat = time_us_64();
    
    while (true) {
        // Watchdog - print heartbeat every 5 seconds
        uint64_t current_time = time_us_64();
        if ((current_time - last_heartbeat) > 5000000) {
            printf("Heartbeat: Loop %d, State=%d, LED should be %s\n", 
                   loop_count, gb_link_get_state(), 
                   gb_link_get_state() == TRADE_STATE_NOT_CONNECTED ? "slow blinking" : "fast blinking");
            
            // Check ISR health
            if (!gb_link_check_isr_health()) {
                printf("ISR was reset due to error\n");
                gb_link_set_state(TRADE_STATE_NOT_CONNECTED);
                ui_set_led_pattern(LED_SLOW_BLINK);
            }
            
            last_heartbeat = current_time;
        }
        loop_count++;
        
        // Check for incoming Game Boy connection only if not already connected
        if (gb_link_get_state() == TRADE_STATE_NOT_CONNECTED) {
            if (gb_link_wait_for_connection()) {
                printf("Game Boy connected!\n");
                ui_show_status(gb_link_get_state());
                ui_set_led_pattern(LED_FAST_BLINK);
                
                // For demonstration, use regular trade process with default Pokemon
                // This bypasses the storage checksum issue
                printf("\n=== STARTING TRADE PROCESS ===\n");
                printf("Will send default Pokemon (bypassing storage for now)\n");
                handle_trade_process();
                printf("=== TRADE PROCESS COMPLETE ===\n\n");
                
                // Reset connection state after trade
                gb_link_set_state(TRADE_STATE_NOT_CONNECTED);
                ui_set_led_pattern(LED_SLOW_BLINK);
            }
        } else {
            // If connected, continuously handle protocol steps
            gb_link_handle_protocol_step(current_pokemon);
            
            // Update UI based on current status  
            ui_show_status(gb_link_get_state());
        }
        
        sleep_ms(100);
    }
    
    // Cleanup (never reached in this example)
    gb_link_deinit();
    storage_deinit();
    ui_deinit();
    
    return 0;
}