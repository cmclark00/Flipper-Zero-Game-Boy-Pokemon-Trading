#ifndef TRADE_LOGIC_H
#define TRADE_LOGIC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h> // For size_t

void trade_logic_init(void);

// slot_index_rp2040: The Pokemon from RP2040's storage to be traded.
// received_pokemon_data: Buffer to store the Pokemon data received from Game Boy.
// buffer_size: Size of received_pokemon_data.
// received_gen: To store the generation of the received Pokemon.
// Returns true if the trade (stub) was "successful."
bool start_trade(uint8_t slot_index_rp2040, void* received_pokemon_data, size_t buffer_size, uint8_t* received_gen);

#endif // TRADE_LOGIC_H
