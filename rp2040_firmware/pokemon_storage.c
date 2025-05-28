#include "include/pokemon_storage.h"
#include "include/pokemon_data.h" // For GEN_I, GEN_II if needed for validation, and TradeBlock definitions
#include "hardware/flash.h"
#include "hardware/sync.h" // For save_and_disable_interrupts
#include <string.h> // For memcpy, memset
#include <stdio.h>  // For printf (debugging)

// Helper function to get the flash address for a given slot
static inline uint32_t get_slot_flash_address(uint8_t slot_index) {
    return XIP_BASE + POKEMON_STORAGE_FLASH_OFFSET + (slot_index * MAX_POKEMON_SLOT_SIZE);
}

void pokemon_storage_init(void) {
    // Future: Could verify flash integrity or initialize empty slots if needed.
    // For now, we assume flash is either valid or will be written to.
    printf("Pokemon Storage Initialized. Flash offset: 0x%08X, Slot size: %d, Total storage size: %d\n",
           (unsigned int)POKEMON_STORAGE_FLASH_OFFSET,
           (int)MAX_POKEMON_SLOT_SIZE,
           (int)POKEMON_STORAGE_TOTAL_SIZE);

    // One-time erase of the entire storage area if it's the very first boot
    // This is a destructive operation and should ideally be handled by a
    // more sophisticated provisioning process or first-time setup routine.
    // For now, we'll skip automatic full erasure on init.
    // Users can call erase on individual slots or a dedicated "erase all" function if needed.
}

bool pokemon_storage_save(uint8_t slot_index, const void* pokemon_data, size_t data_size, uint8_t gen) {
    if (slot_index >= MAX_STORED_POKEMON) {
        printf("Save Error: Slot index %d out of bounds.\n", slot_index);
        return false;
    }
    if (data_size == 0 || data_size > MAX_POKEMON_RAW_DATA_SIZE) {
        printf("Save Error: Invalid data size %u for slot %d.\n", (unsigned int)data_size, slot_index);
        return false;
    }
    if (pokemon_data == NULL) {
        printf("Save Error: Null pokemon_data for slot %d.\n", slot_index);
        return false;
    }

    uint32_t slot_address = get_slot_flash_address(slot_index);
    
    // Prepare header
    PokemonStorageHeader header;
    header.magic = POKEMON_STORAGE_MAGIC;
    header.generation = gen;
    header.data_len = (uint16_t)data_size;
    header.is_empty = false;
    memset(header.reserved, 0xFF, sizeof(header.reserved)); // Fill reserved with 0xFF

    // Create a temporary buffer for the entire slot data (header + pokemon_data)
    // This must be padded to be a multiple of FLASH_PAGE_SIZE for programming.
    uint8_t slot_buffer[MAX_POKEMON_SLOT_SIZE]; 
    memset(slot_buffer, 0xFF, MAX_POKEMON_SLOT_SIZE); // Initialize with 0xFF (erased state)

    memcpy(slot_buffer, &header, POKEMON_SLOT_HEADER_SIZE);
    memcpy(slot_buffer + POKEMON_SLOT_HEADER_SIZE, pokemon_data, data_size);

    // Calculate actual data to write (header + pokemon_data), padded to FLASH_PAGE_SIZE
    size_t total_data_to_write = POKEMON_SLOT_HEADER_SIZE + data_size;
    size_t padded_write_size = (total_data_to_write + FLASH_PAGE_SIZE - 1) & ~(FLASH_PAGE_SIZE - 1);
    if (padded_write_size > MAX_POKEMON_SLOT_SIZE) {
         // This should not happen if MAX_POKEMON_SLOT_SIZE is defined correctly
        printf("Save Error: Padded write size %u exceeds MAX_POKEMON_SLOT_SIZE %d for slot %d.\n", (unsigned int)padded_write_size, (int)MAX_POKEMON_SLOT_SIZE, slot_index);
        return false;
    }


    printf("Saving to Slot %d: Addr 0x%08X, Gen %d, Data Size %u, Padded Write Size %u\n",
           slot_index, (unsigned int)slot_address, gen, (unsigned int)data_size, (unsigned int)padded_write_size);

    uint32_t irq_status = save_and_disable_interrupts();
    // Erase the part of the sector that this slot occupies.
    // Erasing only the necessary pages for this slot.
    // flash_range_erase expects offset from start of flash (XIP_BASE).
    flash_range_erase(POKEMON_STORAGE_FLASH_OFFSET + (slot_index * MAX_POKEMON_SLOT_SIZE), padded_write_size);
    flash_range_program(POKEMON_STORAGE_FLASH_OFFSET + (slot_index * MAX_POKEMON_SLOT_SIZE), slot_buffer, padded_write_size);
    restore_interrupts(irq_status);
    
    // Verification (optional but good for debugging)
    PokemonStorageHeader verify_header;
    memcpy(&verify_header, (const void*)slot_address, sizeof(PokemonStorageHeader));
    if (verify_header.magic != POKEMON_STORAGE_MAGIC || verify_header.data_len != data_size) {
        printf("Save Error: Verification failed for slot %d. Magic: 0x%0X, Len: %d\n", slot_index, (unsigned int)verify_header.magic, verify_header.data_len);
        return false;
    }

    printf("Save successful for slot %d.\n", slot_index);
    return true;
}

bool pokemon_storage_load(uint8_t slot_index, void* buffer, size_t buffer_size, uint8_t* gen) {
    if (slot_index >= MAX_STORED_POKEMON) {
        printf("Load Error: Slot index %d out of bounds.\n", slot_index);
        return false;
    }
    if (buffer == NULL || gen == NULL) {
        printf("Load Error: Null buffer or gen pointer for slot %d.\n", slot_index);
        return false;
    }

    uint32_t slot_address = get_slot_flash_address(slot_index);
    const PokemonStorageHeader* header = (const PokemonStorageHeader*)slot_address;

    if (header->magic != POKEMON_STORAGE_MAGIC || header->is_empty) {
        printf("Load Info: Slot %d is empty or magic number invalid (0x%08X).\n", slot_index, (unsigned int)header->magic);
        return false; // Slot is empty or invalid
    }

    if (header->data_len == 0 || header->data_len > MAX_POKEMON_RAW_DATA_SIZE) {
        printf("Load Error: Invalid data length %d in header for slot %d.\n", header->data_len, slot_index);
        return false; // Invalid data length
    }
    
    if (buffer_size < header->data_len) {
        printf("Load Error: Buffer too small (%u) for data size %d in slot %d.\n", (unsigned int)buffer_size, header->data_len, slot_index);
        return false; // Buffer too small
    }

    *gen = header->generation;
    memcpy(buffer, (const void*)(slot_address + POKEMON_SLOT_HEADER_SIZE), header->data_len);

    printf("Load successful for slot %d: Gen %d, Data Size %d\n", slot_index, *gen, header->data_len);
    return true;
}

void pokemon_storage_erase(uint8_t slot_index) {
    if (slot_index >= MAX_STORED_POKEMON) {
        printf("Erase Error: Slot index %d out of bounds.\n", slot_index);
        return;
    }

    uint32_t slot_address = get_slot_flash_address(slot_index);
    printf("Erasing Slot %d: Addr 0x%08X\n", slot_index, (unsigned int)slot_address);

    // Prepare a header indicating an empty slot
    PokemonStorageHeader empty_header;
    empty_header.magic = POKEMON_STORAGE_MAGIC; // Keep magic to show it was intentionally erased
    empty_header.generation = 0;
    empty_header.data_len = 0;
    empty_header.is_empty = true;
    memset(empty_header.reserved, 0xFF, sizeof(empty_header.reserved));

    uint8_t header_buffer[POKEMON_SLOT_HEADER_SIZE];
    memcpy(header_buffer, &empty_header, POKEMON_SLOT_HEADER_SIZE);
    
    // For simplicity, we erase the pages covering the header.
    // A more robust erase might erase the entire slot or use a different strategy.
    size_t erase_size = (POKEMON_SLOT_HEADER_SIZE + FLASH_PAGE_SIZE -1) & ~(FLASH_PAGE_SIZE - 1);
    if (erase_size == 0) erase_size = FLASH_PAGE_SIZE; // ensure at least one page

    uint32_t irq_status = save_and_disable_interrupts();
    flash_range_erase(POKEMON_STORAGE_FLASH_OFFSET + (slot_index * MAX_POKEMON_SLOT_SIZE), erase_size);
    flash_range_program(POKEMON_STORAGE_FLASH_OFFSET + (slot_index * MAX_POKEMON_SLOT_SIZE), header_buffer, erase_size);
    restore_interrupts(irq_status);

#include "include/pokemon_data_i.h" // For PokemonPartyGenI/II struct definitions

// ... (other functions remain the same) ...

void pokemon_storage_erase(uint8_t slot_index) {
    if (slot_index >= MAX_STORED_POKEMON) {
        printf("Erase Error: Slot index %d out of bounds.\n", slot_index);
        return;
    }

    uint32_t slot_address = get_slot_flash_address(slot_index);
    printf("Erasing Slot %d: Addr 0x%08X\n", slot_index, (unsigned int)slot_address);

    // Prepare a header indicating an empty slot
    PokemonStorageHeader empty_header;
    empty_header.magic = POKEMON_STORAGE_MAGIC; // Keep magic to show it was intentionally erased
    empty_header.generation = 0;
    empty_header.data_len = 0;
    empty_header.is_empty = true;
    memset(empty_header.reserved, 0xFF, sizeof(empty_header.reserved));

    uint8_t header_buffer[POKEMON_SLOT_HEADER_SIZE];
    memcpy(header_buffer, &empty_header, POKEMON_SLOT_HEADER_SIZE);
    
    // Erase pages covering the header and potentially a bit more of the data area.
    // For simplicity, erasing the whole slot might be easier if performance allows.
    // Here, we'll erase enough for the header and a minimal part of the data.
    size_t erase_size = (MAX_POKEMON_SLOT_SIZE + FLASH_SECTOR_SIZE -1) & ~(FLASH_SECTOR_SIZE - 1);
    if (erase_size == 0) erase_size = FLASH_SECTOR_SIZE; 
    if (erase_size > MAX_POKEMON_SLOT_SIZE) erase_size = MAX_POKEMON_SLOT_SIZE;


    uint32_t irq_status = save_and_disable_interrupts();
    // Erase the number of pages that this slot would occupy.
    flash_range_erase(POKEMON_STORAGE_FLASH_OFFSET + (slot_index * MAX_POKEMON_SLOT_SIZE), erase_size);
    // Only program the header to mark as empty. The rest of the erased data is 0xFF.
    flash_range_program(POKEMON_STORAGE_FLASH_OFFSET + (slot_index * MAX_POKEMON_SLOT_SIZE), header_buffer, POKEMON_SLOT_HEADER_SIZE);
    restore_interrupts(irq_status);

    printf("Erase complete for slot %d.\n", slot_index);
}


bool pokemon_storage_get_info(uint8_t slot_index, uint8_t* gen, uint8_t* species_id, uint8_t* level, bool* is_valid_slot) {
    if (slot_index >= MAX_STORED_POKEMON) {
        if(is_valid_slot) *is_valid_slot = false;
        return false; // Invalid parameters
    }
    if (gen == NULL || species_id == NULL || level == NULL || is_valid_slot == NULL) {
        if(is_valid_slot) *is_valid_slot = false; // Can't set other output params if is_valid_slot is NULL
        return false; // Invalid parameters
    }

    *is_valid_slot = false; // Default to invalid
    *species_id = 0;
    *level = 0;
    *gen = 0;

    uint32_t slot_address = get_slot_flash_address(slot_index);
    const PokemonStorageHeader* header = (const PokemonStorageHeader*)slot_address;

    // Check magic number first
    if (header->magic != POKEMON_STORAGE_MAGIC) {
        // printf("Get Info: Slot %d - Bad magic number 0x%08X\n", slot_index, (unsigned int)header->magic);
        return false; // Header itself is invalid or not initialized
    }

    *gen = header->generation; // Get generation even if empty, might be useful

    if (header->is_empty) {
        // printf("Get Info: Slot %d is marked empty.\n", slot_index);
        return true; // Header read successfully, slot is empty
    }

    if (header->data_len == 0) {
        // printf("Get Info: Slot %d has zero data length but not marked empty.\n", slot_index);
        return true; // Header read, but data is effectively empty/invalid
    }
    
    // At this point, header is valid and not empty, and data_len > 0.
    // Now, attempt to read species and level from the stored Pokemon data.
    const uint8_t* pokemon_data_ptr = (const uint8_t*)(slot_address + POKEMON_SLOT_HEADER_SIZE);

    if (*gen == GEN_I) {
        if (header->data_len >= sizeof(PokemonPartyGenI)) { // Ensure enough data was stored for Gen I
            // For Gen I, species ID is at offset 0, level is at offset 0x21 (33)
            // These offsets are relative to the start of the PokemonPartyGenI struct.
            // The `pokemon_data_ptr` points to the start of this struct in flash.
            *species_id = pokemon_data_ptr[0]; // index (Species ID)
            *level = pokemon_data_ptr[33];     // level
            *is_valid_slot = true;
            // printf("Get Info: Slot %d, Gen I, Species %d, Level %d\n", slot_index, *species_id, *level);
        } else {
            printf("Get Info Error: Slot %d, Gen I, data_len %u too small for PokemonPartyGenI (%u).\n", slot_index, header->data_len, (unsigned int)sizeof(PokemonPartyGenI));
        }
    } else if (*gen == GEN_II) {
        if (header->data_len >= sizeof(PokemonPartyGenII)) { // Ensure enough data was stored for Gen II
            // For Gen II, species ID is at offset 0, level is at offset 0x1F (31)
            *species_id = pokemon_data_ptr[0]; // index (Species ID)
            *level = pokemon_data_ptr[31];     // level
            *is_valid_slot = true;
            // printf("Get Info: Slot %d, Gen II, Species %d, Level %d\n", slot_index, *species_id, *level);
        } else {
            printf("Get Info Error: Slot %d, Gen II, data_len %u too small for PokemonPartyGenII (%u).\n", slot_index, header->data_len, (unsigned int)sizeof(PokemonPartyGenII));
        }
    } else {
        printf("Get Info: Slot %d - Unknown generation %d in header.\n", slot_index, *gen);
        // *is_valid_slot remains false
    }
    
    return true; // Header was read successfully
}
