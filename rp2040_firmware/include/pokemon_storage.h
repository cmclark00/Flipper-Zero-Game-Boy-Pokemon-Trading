#ifndef POKEMON_STORAGE_H
#define POKEMON_STORAGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "pico/stdlib.h" // For PICO_FLASH_SIZE_BYTES if available, or general types
#include "hardware/flash.h"

// --- Storage Configuration ---
#define MAX_STORED_POKEMON 6

// Define a magic number to identify valid Pokemon data in flash
#define POKEMON_STORAGE_MAGIC 0xBADF00D5

// Define a simple header structure for each stored Pokemon
typedef struct {
    uint32_t magic;
    uint8_t generation; // Gen I or Gen II
    uint16_t data_len;  // Actual length of the Pokemon data that follows
    bool is_empty;      // True if the slot is considered empty/erased
    uint8_t reserved[22]; // Padding to make header 32 bytes for easier alignment/management if needed
} PokemonStorageHeader;

// Estimate max size for Pokemon data (TradeBlockGenI/II)
// TradeBlockGenI is roughly 415 bytes. TradeBlockGenII is slightly larger.
// Let's allocate a generous buffer per Pokemon, including the header.
#define MAX_POKEMON_RAW_DATA_SIZE 512 // Max size of TradeBlockGenI/II
#define POKEMON_SLOT_HEADER_SIZE sizeof(PokemonStorageHeader)
#define MAX_POKEMON_SLOT_SIZE (POKEMON_SLOT_HEADER_SIZE + MAX_POKEMON_RAW_DATA_SIZE) // Total size for one Pokemon with header

// Flash Memory Layout
// PICO_FLASH_SIZE_BYTES is defined by pico_sdk, usually (2 * 1024 * 1024) for 2MB
// We need to place our storage at an offset that doesn't collide with the program.
// For safety, let's assume the program uses less than 1MB and place storage at 1MB offset.
// IMPORTANT: This is a simplification. A robust implementation would determine this offset
// more dynamically (e.g., by using a linker script to place it after the program binary,
// checking `(uint32_t) __flash_binary_end`, or using a fixed known-good offset for a specific board).
// For now, a fixed offset from XIP_BASE is used. This assumes the program binary is smaller than this offset.
#define POKEMON_STORAGE_FLASH_OFFSET (1 * 1024 * 1024) // 1MB offset from start of flash (XIP_BASE)

// Calculate the total size needed for storage, must be a multiple of FLASH_SECTOR_SIZE
#define POKEMON_STORAGE_TOTAL_SIZE_UNALIGNED (MAX_STORED_POKEMON * MAX_POKEMON_SLOT_SIZE)
#define POKEMON_STORAGE_SECTORS ((POKEMON_STORAGE_TOTAL_SIZE_UNALIGNED + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE)
#define POKEMON_STORAGE_TOTAL_SIZE (POKEMON_STORAGE_SECTORS * FLASH_SECTOR_SIZE)


// --- Function Declarations ---

void pokemon_storage_init(void);

bool pokemon_storage_save(uint8_t slot_index, const void* pokemon_data, size_t data_size, uint8_t gen);

bool pokemon_storage_load(uint8_t slot_index, void* buffer, size_t buffer_size, uint8_t* gen);

void pokemon_storage_erase(uint8_t slot_index);

// Gets basic info.
// Returns true if header was read successfully (slot may still be empty or invalid).
// is_valid_slot will be true if the slot contains a non-empty, valid Pokemon.
// species_id and level will be populated if is_valid_slot is true.
bool pokemon_storage_get_info(uint8_t slot_index, uint8_t* gen, uint8_t* species_id, uint8_t* level, bool* is_valid_slot);

#endif // POKEMON_STORAGE_H
