#include "include/trade_logic.h"
#include "include/gb_link_protocol.h"
#include "include/pokemon_storage.h"
#include "include/pokemon_data.h" // For GEN_I, GEN_II, and trade block size estimates
#include <stdio.h>  // For printf
#include <string.h> // For memcpy

// Approximate size of a trade block for simulation.
// Gen I: TradeBlockGenI is ~415 bytes. PokemonPartyGenI is 44 bytes.
// Gen II: TradeBlockGenII is slightly larger. PokemonPartyGenII is 48 bytes.
// For a single Pokemon trade, we are interested in the size of one PokemonPartyData struct.
// Let's use a size that covers one Pokemon's party data.
#define SIMULATED_TRADE_DATA_SIZE sizeof(PokemonPartyGenI) // Approx 44 bytes for Gen I

void trade_logic_init(void) {
    printf("Trade Logic Initialized.\n");
    // Any future initialization for trade state can go here.
}

bool start_trade(uint8_t slot_index_rp2040, void* received_pokemon_data, size_t buffer_size, uint8_t* received_gen) {
    printf("start_trade: Initiating trade for RP2040 slot %d.\n", slot_index_rp2040);

    if (gb_link_get_status() != GBLINK_CONNECTED_IDLE) {
        printf("start_trade: Error - Link not in CONNECTED_IDLE state. Current state: %d\n", gb_link_get_status());
        return false;
    }

    uint8_t pokemon_to_send_buffer[MAX_POKEMON_RAW_DATA_SIZE];
    uint8_t gen_to_send;
    size_t actual_data_size; // Not used in this stub for sending, but good for load.

    // 1. Load the Pokémon from RP2040's storage
    if (!pokemon_storage_load(slot_index_rp2040, pokemon_to_send_buffer, sizeof(pokemon_to_send_buffer), &gen_to_send)) {
        printf("start_trade: Failed to load Pokemon from slot %d.\n", slot_index_rp2040);
        return false;
    }
    // In a real scenario, actual_data_size would be set by pokemon_storage_load and used.
    // For this stub, we'll use SIMULATED_TRADE_DATA_SIZE for the exchange loop.
    printf("start_trade: Loaded Pokemon from slot %d (Gen %d) to send.\n", slot_index_rp2040, gen_to_send);

    // Simulate Trade: Exchange bytes
    printf("start_trade: Simulating byte exchange (%d bytes)...\n", (int)SIMULATED_TRADE_DATA_SIZE);
    bool exchange_success;
    for (size_t i = 0; i < SIMULATED_TRADE_DATA_SIZE; ++i) {
        uint8_t byte_to_send = pokemon_to_send_buffer[i]; // Sending actual loaded data
        uint8_t received_byte = gb_link_exchange_byte(byte_to_send, &exchange_success);

        if (!exchange_success) {
            printf("start_trade: Byte exchange failed at byte %d.\n", (int)i);
            // Potentially update GblinkState to GBLINK_ERROR via a new function if needed
            return false;
        }

        if (i < buffer_size) {
            ((uint8_t*)received_pokemon_data)[i] = received_byte; // Store received byte
        }
    }

    // Set dummy received generation
    *received_gen = GEN_I; // Or GEN_II, or based on gen_to_send for symmetry

    printf("start_trade: Trade simulation complete for slot %d. Received dummy data for Gen %d.\n", slot_index_rp2040, *received_gen);
    return true;
}
