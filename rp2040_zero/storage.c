#include "storage.h"
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include <stdio.h>
#include <string.h>

// Storage layout in flash
#define STORAGE_HEADER_SIZE     16
#define POKEMON_SLOT_SIZE       (POKEMON_DATA_SIZE + 16)  // Data + metadata
#define TOTAL_STORAGE_SIZE      (STORAGE_HEADER_SIZE + (MAX_POKEMON_STORAGE * POKEMON_SLOT_SIZE))

// Magic header to identify valid storage
#define STORAGE_MAGIC           0x504B4D4E  // "PKMN"
#define STORAGE_VERSION         1

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t slot_count;
    uint32_t reserved;
} storage_header_t;

typedef struct {
    uint32_t magic;        // Magic number to identify valid slot
    uint32_t data_size;    // Size of Pokemon data
    uint32_t checksum;     // Simple checksum
    uint32_t timestamp;    // When stored
    uint8_t data[POKEMON_DATA_SIZE];
} pokemon_slot_t;

static storage_header_t storage_header;
static uint32_t flash_offset = FLASH_STORAGE_OFFSET;

// Simple checksum calculation
static uint32_t calculate_checksum(const uint8_t* data, size_t len) {
    uint32_t checksum = 0;
    for (size_t i = 0; i < len; i++) {
        checksum += data[i];
    }
    return checksum;
}

static uint32_t get_flash_address(void) {
    return XIP_BASE + flash_offset;
}

static uint32_t get_slot_flash_address(uint8_t slot) {
    return get_flash_address() + STORAGE_HEADER_SIZE + (slot * POKEMON_SLOT_SIZE);
}

bool storage_init(void) {
    printf("Initializing storage...\n");
    
    // Read storage header from flash
    const storage_header_t* flash_header = (const storage_header_t*)get_flash_address();
    
    if (flash_header->magic == STORAGE_MAGIC && flash_header->version == STORAGE_VERSION) {
        // Valid header found
        memcpy(&storage_header, flash_header, sizeof(storage_header_t));
        printf("Found valid storage with %lu slots\n", storage_header.slot_count);
    } else {
        // Initialize new storage area
        printf("Initializing new storage area\n");
        storage_header.magic = STORAGE_MAGIC;
        storage_header.version = STORAGE_VERSION;
        storage_header.slot_count = 0;
        storage_header.reserved = 0;
        
        // Write header to flash
        uint32_t interrupts = save_and_disable_interrupts();
        flash_range_erase(flash_offset, FLASH_SECTOR_SIZE);
        flash_range_program(flash_offset, (const uint8_t*)&storage_header, sizeof(storage_header_t));
        restore_interrupts(interrupts);
    }
    
    printf("Storage initialized successfully\n");
    return true;
}

void storage_deinit(void) {
    // Nothing specific needed for flash storage
}

bool storage_save_pokemon(uint8_t slot, const uint8_t* pokemon_data, size_t data_len) {
    if (slot >= MAX_POKEMON_STORAGE) {
        printf("Invalid slot number: %d\n", slot);
        return false;
    }
    
    if (data_len > POKEMON_DATA_SIZE) {
        printf("Pokemon data too large: %zu bytes\n", data_len);
        return false;
    }
    
    printf("Saving Pokemon to slot %d (data_len=%zu)\n", slot, data_len);
    
    // Prepare slot data
    pokemon_slot_t slot_data = {0};
    slot_data.magic = STORAGE_MAGIC;
    slot_data.data_size = data_len;
    slot_data.checksum = calculate_checksum(pokemon_data, data_len);
    slot_data.timestamp = time_us_32();
    memcpy(slot_data.data, pokemon_data, data_len);
    
    printf("Save: checksum=0x%08X, magic=0x%08X, data_size=%u\n", 
           slot_data.checksum, slot_data.magic, slot_data.data_size);
    printf("Save first 16 bytes: ");
    for (int i = 0; i < 16 && i < data_len; i++) {
        printf("%02X ", pokemon_data[i]);
    }
    printf("\n");
    
    // Calculate flash address for this slot
    uint32_t slot_offset = flash_offset + STORAGE_HEADER_SIZE + (slot * POKEMON_SLOT_SIZE);
    
    // Erase and write slot data
    uint32_t interrupts = save_and_disable_interrupts();
    
    // We need to erase in 4K sectors, so find which sector this slot is in
    uint32_t sector_start = (slot_offset / FLASH_SECTOR_SIZE) * FLASH_SECTOR_SIZE;
    
    // For simplicity, erase the entire sector and rewrite
    // In a production system, you'd want to preserve other data in the sector
    flash_range_erase(sector_start, FLASH_SECTOR_SIZE);
    flash_range_program(slot_offset, (const uint8_t*)&slot_data, sizeof(pokemon_slot_t));
    
    restore_interrupts(interrupts);
    
    // Update header if this is a new slot
    if (slot >= storage_header.slot_count) {
        storage_header.slot_count = slot + 1;
        
        interrupts = save_and_disable_interrupts();
        flash_range_erase(flash_offset, FLASH_SECTOR_SIZE);
        flash_range_program(flash_offset, (const uint8_t*)&storage_header, sizeof(storage_header_t));
        restore_interrupts(interrupts);
    }
    
    printf("Pokemon saved successfully to slot %d\n", slot);
    return true;
}

bool storage_load_pokemon(uint8_t slot, uint8_t* pokemon_data, size_t* data_len) {
    if (slot >= MAX_POKEMON_STORAGE) {
        printf("Invalid slot number: %d\n", slot);
        return false;
    }
    
    // Read slot data from flash
    const pokemon_slot_t* slot_data = (const pokemon_slot_t*)get_slot_flash_address(slot);
    
    if (slot_data->magic != STORAGE_MAGIC) {
        printf("No valid Pokemon data in slot %d\n", slot);
        return false;
    }
    
    // Verify checksum
    uint32_t calculated_checksum = calculate_checksum(slot_data->data, slot_data->data_size);
    printf("Slot %d: stored_checksum=0x%08X, calculated_checksum=0x%08X, data_size=%u\n", 
           slot, slot_data->checksum, calculated_checksum, slot_data->data_size);
    if (calculated_checksum != slot_data->checksum) {
        printf("Checksum mismatch in slot %d (stored=0x%08X vs calculated=0x%08X)\n", 
               slot, slot_data->checksum, calculated_checksum);
        // Show first few bytes for debugging
        printf("First 16 bytes: ");
        for (int i = 0; i < 16 && i < slot_data->data_size; i++) {
            printf("%02X ", slot_data->data[i]);
        }
        printf("\n");
        return false;
    }
    
    // Copy data
    memcpy(pokemon_data, slot_data->data, slot_data->data_size);
    if (data_len) {
        *data_len = slot_data->data_size;
    }
    
    printf("Pokemon loaded successfully from slot %d\n", slot);
    return true;
}

bool storage_list_pokemon(uint8_t* slot_list, size_t max_slots, size_t* count) {
    size_t found_count = 0;
    
    printf("Scanning %d Pokemon storage slots...\n", MAX_POKEMON_STORAGE);
    
    for (uint8_t slot = 0; slot < MAX_POKEMON_STORAGE && found_count < max_slots; slot++) {
        printf("Checking slot %d... ", slot);
        
        // Add safety check for flash access
        uint32_t flash_addr = get_slot_flash_address(slot);
        printf("flash_addr=0x%08X ", flash_addr);
        
        // Check if the address is reasonable (within flash bounds)
        if (flash_addr < XIP_BASE || flash_addr >= (XIP_BASE + PICO_FLASH_SIZE_BYTES)) {
            printf("invalid address, skipping\n");
            continue;
        }
        
        const pokemon_slot_t* slot_data = (const pokemon_slot_t*)flash_addr;
        
        // Read magic with error handling
        uint32_t magic;
        __builtin_memcpy(&magic, &slot_data->magic, sizeof(magic));
        printf("magic=0x%08X ", magic);
        
        if (magic == STORAGE_MAGIC) {
            printf("valid magic, checking checksum... ");
            // Verify checksum with bounds checking
            uint32_t data_size;
            __builtin_memcpy(&data_size, &slot_data->data_size, sizeof(data_size));
            
            if (data_size <= POKEMON_DATA_SIZE) {
                uint32_t calculated_checksum = calculate_checksum(slot_data->data, data_size);
                uint32_t stored_checksum;
                __builtin_memcpy(&stored_checksum, &slot_data->checksum, sizeof(stored_checksum));
                
                if (calculated_checksum == stored_checksum) {
                    printf("valid!\n");
                    slot_list[found_count] = slot;
                    found_count++;
                } else {
                    printf("checksum mismatch\n");
                }
            } else {
                printf("invalid data_size=%u\n", data_size);
            }
        } else {
            printf("empty/invalid\n");
        }
    }
    
    printf("Found %zu valid Pokemon slots\n", found_count);
    
    if (count) {
        *count = found_count;
    }
    
    return true;
}

bool storage_delete_pokemon(uint8_t slot) {
    if (slot >= MAX_POKEMON_STORAGE) {
        printf("Invalid slot number: %d\n", slot);
        return false;
    }
    
    // Simply write zeros to invalidate the slot
    pokemon_slot_t empty_slot = {0};
    
    uint32_t slot_offset = flash_offset + STORAGE_HEADER_SIZE + (slot * POKEMON_SLOT_SIZE);
    uint32_t sector_start = (slot_offset / FLASH_SECTOR_SIZE) * FLASH_SECTOR_SIZE;
    
    uint32_t interrupts = save_and_disable_interrupts();
    flash_range_erase(sector_start, FLASH_SECTOR_SIZE);
    flash_range_program(slot_offset, (const uint8_t*)&empty_slot, sizeof(pokemon_slot_t));
    restore_interrupts(interrupts);
    
    printf("Pokemon deleted from slot %d\n", slot);
    return true;
}

bool storage_format_flash(void) {
    printf("Formatting storage area...\n");
    
    // Erase the entire storage area
    uint32_t interrupts = save_and_disable_interrupts();
    
    // Calculate how many sectors we need to erase
    uint32_t sectors_needed = (TOTAL_STORAGE_SIZE + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE;
    
    for (uint32_t i = 0; i < sectors_needed; i++) {
        flash_range_erase(flash_offset + (i * FLASH_SECTOR_SIZE), FLASH_SECTOR_SIZE);
    }
    
    restore_interrupts(interrupts);
    
    // Reinitialize storage
    return storage_init();
}