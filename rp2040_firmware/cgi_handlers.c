#include "include/cgi_handlers.h"
#include "lwip/apps/httpd.h" // For tCGI and http_set_cgi_handlers
#include "include/pokemon_storage.h" // For pokemon_storage_get_info and MAX_STORED_POKEMON
#include "include/gb_link_protocol.h" // For GblinkState and gb_link_get_status()
#include <stdio.h>  // For snprintf
#include <string.h> // For strlen, strcpy (though snprintf is safer)
#include <stdlib.h> // For atoi

// Static buffers to hold the JSON responses.
// Ensure these are large enough for the biggest possible response.
// These need to be static so the pointer returned by the CGI handler remains valid.
static char json_response_buffer_small[128];
static char json_response_buffer_large[1024]; // For pokemon list

// Helper function to convert GblinkState to string
static const char* gblink_state_to_string(GblinkState state) {
    switch (state) {
        case GBLINK_DISCONNECTED: return "Disconnected";
        case GBLINK_INIT_FAILED: return "Initialization Failed";
        case GBLINK_PIO_LOAD_FAILED: return "PIO Load Failed";
        case GBLINK_CONNECTED_IDLE: return "Connected - Idle";
        case GBLINK_READY_TO_TRADE: return "Ready to Trade";
        case GBLINK_TRADING: return "Trading";
        case GBLINK_TRADE_COMPLETE: return "Trade Complete";
        case GBLINK_ERROR: return "Error";
        default: return "Unknown";
    }
}

const char* cgi_handler_status(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]) {
    (void)iIndex; (void)iNumParams; (void)pcParam; (void)pcValue; // Unused parameters

    GblinkState current_status = gb_link_get_status();
    const char* status_str = gblink_state_to_string(current_status);

    snprintf(json_response_buffer_small, sizeof(json_response_buffer_small),
             "{ \"status\": \"%s\" }", status_str);
    return json_response_buffer_small;
}

const char* cgi_handler_pokemon_list(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]) {
    (void)iIndex; (void)iNumParams; (void)pcParam; (void)pcValue; // Unused parameters

    char* p = json_response_buffer_large;
    int remaining_len = sizeof(json_response_buffer_large);
    int written;

    written = snprintf(p, remaining_len, "{\"pokemon\": ["); // Start of JSON object with a "pokemon" array
    p += written;
    remaining_len -= written;

    for (uint8_t i = 0; i < MAX_STORED_POKEMON; ++i) {
        uint8_t gen = 0;
        uint8_t species_id = 0;
        uint8_t level = 0;
        bool is_valid_slot_data = false; // Indicates if the data in the slot is a valid Pokemon
        bool header_read_ok;

        header_read_ok = pokemon_storage_get_info(i, &gen, &species_id, &level, &is_valid_slot_data);

        if (i > 0) {
            written = snprintf(p, remaining_len, ",");
            if (written < 0 || written >= remaining_len) goto end_json; // Check for snprintf error or overflow
            p += written;
            remaining_len -= written;
        }

        if (header_read_ok && is_valid_slot_data) {
            // Valid Pokemon data in the slot
            // The actual name would require looking up species_id in pokemon_table.c
            // For now, use a placeholder name.
            char temp_name[32];
            snprintf(temp_name, sizeof(temp_name), "Pkmn (ID:%d)", species_id);

            written = snprintf(p, remaining_len,
                               "{ \"slot\": %d, \"valid\": true, \"gen\": %d, \"species_id\": %d, \"level\": %d, \"name\": \"%s\" }",
                               i, gen, species_id, level, temp_name);
        } else {
            // Slot is empty, or header invalid, or data within slot is invalid
            written = snprintf(p, remaining_len,
                               "{ \"slot\": %d, \"valid\": false, \"name\": \"Empty\" }", i);
        }
        if (written < 0 || written >= remaining_len) goto end_json; // Check for snprintf error or overflow
        p += written;
        remaining_len -= written;
    }

end_json:
    written = snprintf(p, remaining_len, "]}"); // End of "pokemon" array and JSON object
    if (written < 0 || written >= remaining_len) {
        // If we even fail to write the closing part, we might be in trouble.
        // Consider writing a fallback error JSON if this happens. For now, just terminate.
        if (sizeof(json_response_buffer_large) > 30) { // Check if buffer is reasonably sized
             snprintf(json_response_buffer_large, sizeof(json_response_buffer_large), "{\"error\": \"Buffer overflow\"}");
        }
    }
    return json_response_buffer_large;
}

const char* cgi_handler_trade_start(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]) {
    (void)iIndex; // Unused
    
    // Example: Check for a "slot" parameter
    int slot_to_trade = -1;
    if (iNumParams > 0) {
        for (int i = 0; i < iNumParams; i++) {
            if (strcmp(pcParam[i], "slot") == 0) {
                slot_to_trade = atoi(pcValue[i]);
                break;
            }
        }
    }

    if (slot_to_trade < 0 || slot_to_trade >= MAX_STORED_POKEMON) {
        snprintf(json_response_buffer_small, sizeof(json_response_buffer_small),
                 "{ \"success\": false, \"message\": \"Invalid or missing slot parameter.\" }");
        return json_response_buffer_small;
    }
    
    // Placeholder for actual trade logic
    // uint8_t received_pokemon_buffer[MAX_POKEMON_RAW_DATA_SIZE]; // Defined in pokemon_storage.h
    // uint8_t received_gen;
    // bool trade_success = start_trade((uint8_t)slot_to_trade, received_pokemon_buffer, sizeof(received_pokemon_buffer), &received_gen);

#include "include/trade_logic.h" // For start_trade

// ... (other includes and static buffers remain the same) ...

// (cgi_handler_status and cgi_handler_pokemon_list remain the same)
// ...

const char* cgi_handler_trade_start(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]) {
    (void)iIndex; // Unused
    
    int slot_to_trade = -1;
    if (iNumParams > 0) {
        for (int i = 0; i < iNumParams; i++) {
            if (strcmp(pcParam[i], "slot") == 0) {
                slot_to_trade = atoi(pcValue[i]);
                break;
            }
        }
    }

    if (slot_to_trade < 0 || slot_to_trade >= MAX_STORED_POKEMON) {
        snprintf(json_response_buffer_small, sizeof(json_response_buffer_small),
                 "{ \"success\": false, \"message\": \"Invalid or missing slot parameter.\" }");
        return json_response_buffer_small;
    }
    
    uint8_t received_pokemon_buffer[MAX_POKEMON_RAW_DATA_SIZE]; // Defined in pokemon_storage.h
    uint8_t received_gen;
    bool trade_success = start_trade((uint8_t)slot_to_trade, received_pokemon_buffer, sizeof(received_pokemon_buffer), &received_gen);

    if (trade_success) {
        // Stub: "Save" the received Pokémon.
        // In a real scenario, we'd find an empty slot or decide where to save it.
        // For this stub, we'll just log that we would save it.
        // The actual size of received data from start_trade (if it returned it) would be used.
        // For now, SIMULATED_TRADE_DATA_SIZE is used internally by start_trade for the loop.
        printf("CGI: Trade successful for slot %d. Received dummy Gen %d Pokemon. (Not actually saving to storage in this stub).\n", slot_to_trade, received_gen);
        // Example: pokemon_storage_save(SOME_AVAILABLE_SLOT, received_pokemon_buffer, SIMULATED_TRADE_DATA_SIZE, received_gen);

        snprintf(json_response_buffer_small, sizeof(json_response_buffer_small),
                 "{ \"success\": true, \"message\": \"Trade with slot %d completed (stub - received dummy data).\" }", slot_to_trade);
    } else {
        snprintf(json_response_buffer_small, sizeof(json_response_buffer_small),
                 "{ \"success\": false, \"message\": \"Trade failed for slot %d.\" }", slot_to_trade);
    }
    return json_response_buffer_small;
}

// Array of CGI handlers, mapping URI to function
static const tCGI cgi_uri_handlers[] = {
    {"/api/status", cgi_handler_status},
    {"/api/pokemon/list", cgi_handler_pokemon_list},
    {"/api/trade/start", cgi_handler_trade_start},
};

void cgi_init(void) {
    http_set_cgi_handlers(cgi_uri_handlers, LWIP_ARRAYSIZE(cgi_uri_handlers));
    printf("CGI Handlers Initialized.\n");
}
