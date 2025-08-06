#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Storage configuration - using onboard flash
#define MAX_POKEMON_STORAGE 20  // Reduced for flash storage
#define POKEMON_DATA_SIZE   415
#define FLASH_STORAGE_OFFSET 0x100000  // 1MB offset in flash

// Function prototypes
bool storage_init(void);
void storage_deinit(void);

bool storage_save_pokemon(uint8_t slot, const uint8_t* pokemon_data, size_t data_len);
bool storage_load_pokemon(uint8_t slot, uint8_t* pokemon_data, size_t* data_len);

bool storage_list_pokemon(uint8_t* slot_list, size_t max_slots, size_t* count);
bool storage_delete_pokemon(uint8_t slot);

bool storage_format_flash(void);

#endif // STORAGE_H