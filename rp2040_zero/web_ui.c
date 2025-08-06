#include "web_ui.h"
#include "storage.h"
#include "gb_link.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// HTTP response buffer
static char response_buffer[8192];
static bool web_ui_enabled = false;

// Pokemon name lookup table (Gen I)
static const char* pokemon_names[] = {
    "MissingNo", "Bulbasaur", "Ivysaur", "Venusaur", "Charmander", "Charmeleon", 
    "Charizard", "Squirtle", "Wartortle", "Blastoise", "Caterpie", "Metapod",
    "Butterfree", "Weedle", "Kakuna", "Beedrill", "Pidgey", "Pidgeotto", "Pidgeot",
    "Rattata", "Raticate", "Spearow", "Fearow", "Ekans", "Arbok", "Pikachu"
    // Add more as needed
};

// Type names
static const char* type_names[] = {
    "Normal", "Fighting", "Flying", "Poison", "Ground", "Rock", "Bug", "Ghost",
    "Steel", "Fire", "Water", "Grass", "Electric", "Psychic", "Ice", "Dragon", "Dark"
};

// Move names (basic set)
static const char* move_names[] = {
    "None", "Pound", "Karate Chop", "Double Slap", "Comet Punch", "Mega Punch",
    "Pay Day", "Fire Punch", "Ice Punch", "Thunder Punch", "Scratch", "Vice Grip",
    "Guillotine", "Razor Wind", "Swords Dance", "Cut", "Gust", "Wing Attack"
    // Add more as needed
};

bool web_ui_init(void) {
    web_ui_enabled = true;
    printf("Web UI initialized - Connect via USB and navigate to http://localhost:8080\n");
    return true;
}

void web_ui_deinit(void) {
    web_ui_enabled = false;
}

const char* web_ui_get_pokemon_name(uint8_t species_id) {
    if (species_id < sizeof(pokemon_names) / sizeof(pokemon_names[0])) {
        return pokemon_names[species_id];
    }
    return "Unknown";
}

const char* web_ui_get_type_name(uint8_t type_id) {
    if (type_id < sizeof(type_names) / sizeof(type_names[0])) {
        return type_names[type_id];
    }
    return "Unknown";
}

const char* web_ui_get_move_name(uint8_t move_id) {
    if (move_id < sizeof(move_names) / sizeof(move_names[0])) {
        return move_names[move_id];
    }
    return "Unknown";
}

void web_ui_send_response(const char* content_type, const char* content) {
    printf("HTTP/1.1 200 OK\r\n");
    printf("Content-Type: %s\r\n", content_type);
    printf("Content-Length: %d\r\n", strlen(content));
    printf("Access-Control-Allow-Origin: *\r\n");
    printf("Connection: close\r\n");
    printf("\r\n");
    printf("%s", content);
}

void web_ui_send_json_pokemon(uint8_t slot) {
    uint8_t pokemon_data[POKEMON_DATA_SIZE];
    size_t data_len;
    
    if (!storage_load_pokemon(slot, pokemon_data, &data_len)) {
        snprintf(response_buffer, sizeof(response_buffer), 
                 "{\"error\": \"No Pokemon in slot %d\"}", slot);
        web_ui_send_response("application/json", response_buffer);
        return;
    }
    
    // Parse Pokemon data and create JSON
    snprintf(response_buffer, sizeof(response_buffer),
        "{\n"
        "  \"slot\": %d,\n"
        "  \"species_id\": %d,\n"
        "  \"species_name\": \"%s\",\n"
        "  \"level\": %d,\n"
        "  \"current_hp\": %d,\n"
        "  \"max_hp\": %d,\n"
        "  \"attack\": %d,\n"
        "  \"defense\": %d,\n"
        "  \"speed\": %d,\n"
        "  \"special\": %d,\n"
        "  \"type1\": \"%s\",\n"
        "  \"type2\": \"%s\",\n"
        "  \"status\": %d,\n"
        "  \"ot_id\": %d,\n"
        "  \"experience\": %lu,\n"
        "  \"moves\": [%d, %d, %d, %d],\n"
        "  \"move_names\": [\"%s\", \"%s\", \"%s\", \"%s\"]\n"
        "}",
        slot,
        pokemon_data[0],
        web_ui_get_pokemon_name(pokemon_data[0]),
        pokemon_data[2],
        pokemon_data[1],
        data_len > 34 ? (pokemon_data[33] | (pokemon_data[34] << 8)) : 0,
        data_len > 36 ? (pokemon_data[35] | (pokemon_data[36] << 8)) : 0,
        data_len > 38 ? (pokemon_data[37] | (pokemon_data[38] << 8)) : 0,
        data_len > 40 ? (pokemon_data[39] | (pokemon_data[40] << 8)) : 0,
        data_len > 42 ? (pokemon_data[41] | (pokemon_data[42] << 8)) : 0,
        web_ui_get_type_name(pokemon_data[4]),
        web_ui_get_type_name(pokemon_data[5]),
        pokemon_data[3],
        (pokemon_data[12] << 8) | pokemon_data[13],
        (uint32_t)((pokemon_data[14] << 16) | (pokemon_data[15] << 8) | pokemon_data[16]),
        pokemon_data[8], pokemon_data[9], pokemon_data[10], pokemon_data[11],
        web_ui_get_move_name(pokemon_data[8]),
        web_ui_get_move_name(pokemon_data[9]),
        web_ui_get_move_name(pokemon_data[10]),
        web_ui_get_move_name(pokemon_data[11])
    );
    
    web_ui_send_response("application/json", response_buffer);
}

void web_ui_send_pokemon_list(void) {
    uint8_t slot_list[MAX_POKEMON_STORAGE];
    size_t count;
    
    if (!storage_list_pokemon(slot_list, MAX_POKEMON_STORAGE, &count)) {
        snprintf(response_buffer, sizeof(response_buffer), 
                 "{\"error\": \"Failed to list Pokemon\"}");
        web_ui_send_response("application/json", response_buffer);
        return;
    }
    
    snprintf(response_buffer, sizeof(response_buffer), 
             "{\n  \"count\": %zu,\n  \"slots\": [", count);
    
    for (size_t i = 0; i < count; i++) {
        char slot_info[256];
        uint8_t pokemon_data[POKEMON_DATA_SIZE];
        size_t data_len;
        
        if (storage_load_pokemon(slot_list[i], pokemon_data, &data_len)) {
            snprintf(slot_info, sizeof(slot_info),
                "%s\n    {\n"
                "      \"slot\": %d,\n"
                "      \"species_id\": %d,\n"
                "      \"species_name\": \"%s\",\n"
                "      \"level\": %d\n"
                "    }",
                i > 0 ? "," : "",
                slot_list[i],
                pokemon_data[0],
                web_ui_get_pokemon_name(pokemon_data[0]),
                pokemon_data[2]
            );
            strcat(response_buffer, slot_info);
        }
    }
    
    strcat(response_buffer, "\n  ]\n}");
    web_ui_send_response("application/json", response_buffer);
}

static const char* html_page = 
"<!DOCTYPE html>\n"
"<html>\n"
"<head>\n"
"    <title>Pokemon Trade Tool - RP2040 Zero</title>\n"
"    <style>\n"
"        body { font-family: Arial, sans-serif; margin: 20px; background: #f0f8ff; }\n"
"        .container { max-width: 1200px; margin: 0 auto; }\n"
"        .header { text-align: center; color: #2c5aa0; margin-bottom: 30px; }\n"
"        .pokemon-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(300px, 1fr)); gap: 20px; }\n"
"        .pokemon-card { background: white; border-radius: 10px; padding: 20px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); }\n"
"        .pokemon-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 15px; }\n"
"        .pokemon-name { font-size: 1.4em; font-weight: bold; color: #2c5aa0; }\n"
"        .pokemon-level { background: #4CAF50; color: white; padding: 4px 8px; border-radius: 12px; font-size: 0.9em; }\n"
"        .pokemon-stats { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }\n"
"        .stat { display: flex; justify-content: space-between; padding: 4px 0; border-bottom: 1px solid #eee; }\n"
"        .moves { margin-top: 15px; }\n"
"        .move { background: #e3f2fd; padding: 4px 8px; margin: 2px; border-radius: 4px; display: inline-block; }\n"
"        .loading { text-align: center; padding: 50px; color: #666; }\n"
"        .error { color: #d32f2f; background: #ffebee; padding: 20px; border-radius: 8px; margin: 20px 0; }\n"
"        .refresh-btn { background: #2196F3; color: white; border: none; padding: 10px 20px; border-radius: 5px; cursor: pointer; margin: 10px; }\n"
"        .refresh-btn:hover { background: #1976D2; }\n"
"    </style>\n"
"</head>\n"
"<body>\n"
"    <div class='container'>\n"
"        <div class='header'>\n"
"            <h1>🎮 Pokemon Trade Tool</h1>\n"
"            <p>RP2040 Zero - USB Pokemon Storage Manager</p>\n"
"            <button class='refresh-btn' onclick='loadPokemon()'>Refresh Pokemon</button>\n"
"        </div>\n"
"        <div id='content' class='loading'>Loading Pokemon...</div>\n"
"    </div>\n"
"\n"
"    <script>\n"
"    async function loadPokemon() {\n"
"        const content = document.getElementById('content');\n"
"        content.innerHTML = '<div class=\"loading\">Loading Pokemon...</div>';\n"
"        \n"
"        try {\n"
"            const response = await fetch('/api/pokemon/list');\n"
"            if (!response.ok) throw new Error('Failed to fetch');\n"
"            const data = await response.json();\n"
"            \n"
"            if (data.error) {\n"
"                content.innerHTML = `<div class=\"error\">Error: ${data.error}</div>`;\n"
"                return;\n"
"            }\n"
"            \n"
"            if (data.count === 0) {\n"
"                content.innerHTML = '<div class=\"loading\">No Pokemon stored yet. Trade with your Game Boy to see Pokemon here!</div>';\n"
"                return;\n"
"            }\n"
"            \n"
"            let html = `<h2>Stored Pokemon (${data.count})</h2><div class=\"pokemon-grid\">`;\n"
"            \n"
"            for (const slot of data.slots) {\n"
"                const pokemon = await loadPokemonDetails(slot.slot);\n"
"                if (pokemon && !pokemon.error) {\n"
"                    html += createPokemonCard(pokemon);\n"
"                }\n"
"            }\n"
"            \n"
"            html += '</div>';\n"
"            content.innerHTML = html;\n"
"            \n"
"        } catch (error) {\n"
"            content.innerHTML = `<div class=\"error\">Connection error: ${error.message}</div>`;\n"
"        }\n"
"    }\n"
"    \n"
"    async function loadPokemonDetails(slot) {\n"
"        try {\n"
"            const response = await fetch(`/api/pokemon/${slot}`);\n"
"            return await response.json();\n"
"        } catch (error) {\n"
"            return { error: error.message };\n"
"        }\n"
"    }\n"
"    \n"
"    function createPokemonCard(pokemon) {\n"
"        return `\n"
"            <div class=\"pokemon-card\">\n"
"                <div class=\"pokemon-header\">\n"
"                    <div class=\"pokemon-name\">${pokemon.species_name}</div>\n"
"                    <div class=\"pokemon-level\">Lv ${pokemon.level}</div>\n"
"                </div>\n"
"                <div class=\"pokemon-stats\">\n"
"                    <div class=\"stat\"><span>HP:</span><span>${pokemon.current_hp}/${pokemon.max_hp}</span></div>\n"
"                    <div class=\"stat\"><span>Attack:</span><span>${pokemon.attack}</span></div>\n"
"                    <div class=\"stat\"><span>Defense:</span><span>${pokemon.defense}</span></div>\n"
"                    <div class=\"stat\"><span>Speed:</span><span>${pokemon.speed}</span></div>\n"
"                    <div class=\"stat\"><span>Special:</span><span>${pokemon.special}</span></div>\n"
"                    <div class=\"stat\"><span>Type:</span><span>${pokemon.type1}/${pokemon.type2}</span></div>\n"
"                </div>\n"
"                <div class=\"moves\">\n"
"                    <strong>Moves:</strong><br>\n"
"                    ${pokemon.move_names.map(move => `<span class=\"move\">${move}</span>`).join('')}\n"
"                </div>\n"
"                <div style=\"margin-top: 10px; font-size: 0.9em; color: #666;\">\n"
"                    Slot ${pokemon.slot} | OT ID: ${pokemon.ot_id} | EXP: ${pokemon.experience}\n"
"                </div>\n"
"            </div>\n"
"        `;\n"
"    }\n"
"    \n"
"    // Load Pokemon on page load\n"
"    loadPokemon();\n"
"    </script>\n"
"</body>\n"
"</html>";

void web_ui_handle_request(const char* request) {
    if (!web_ui_enabled) return;
    
    printf("Handling request: %s\n", request);
    
    // Extract URL from request line "GET /path"
    char url[256] = {0};
    if (sscanf(request, "GET %255s", url) != 1) {
        printf("Failed to parse URL from request\n");
        const char* error = "<h1>400 Bad Request</h1><p>Could not parse request.</p>";
        printf("HTTP/1.1 400 Bad Request\r\n");
        printf("Content-Type: text/html\r\n");
        printf("Content-Length: %d\r\n", strlen(error));
        printf("\r\n");
        printf("%s", error);
        return;
    }
    
    printf("Parsed URL: %s\n", url);
    
    // Handle different URLs
    if (strcmp(url, "/") == 0 || strcmp(url, "/index.html") == 0) {
        printf("Serving main page\n");
        web_ui_send_response("text/html", html_page);
    }
    else if (strcmp(url, "/api/pokemon/list") == 0) {
        printf("Serving Pokemon list\n");
        web_ui_send_pokemon_list();
    }
    else if (strncmp(url, "/api/trade/", 11) == 0) {
        // Handle bidirectional trade API: /api/trade/{send_slot}/{receive_slot}
        const char* params = url + 11;
        int send_slot, receive_slot;
        if (sscanf(params, "%d/%d", &send_slot, &receive_slot) == 2) {
            printf("Starting bidirectional trade: send slot %d, receive slot %d\n", send_slot, receive_slot);
            if (send_slot >= 0 && send_slot < MAX_POKEMON_STORAGE && 
                receive_slot >= 0 && receive_slot < MAX_POKEMON_STORAGE) {
                // Call bidirectional trade function
                extern bool gb_link_bidirectional_trade(uint8_t send_slot, uint8_t receive_slot);
                bool success = gb_link_bidirectional_trade(send_slot, receive_slot);
                
                snprintf(response_buffer, sizeof(response_buffer), 
                         "{\"success\": %s, \"send_slot\": %d, \"receive_slot\": %d, \"message\": \"%s\"}",
                         success ? "true" : "false", send_slot, receive_slot,
                         success ? "Trade completed successfully" : "Trade failed");
                web_ui_send_response("application/json", response_buffer);
            } else {
                snprintf(response_buffer, sizeof(response_buffer), 
                         "{\"error\": \"Invalid slot numbers: %d, %d\"}", send_slot, receive_slot);
                web_ui_send_response("application/json", response_buffer);
            }
        } else {
            snprintf(response_buffer, sizeof(response_buffer), 
                     "{\"error\": \"Invalid trade URL format. Use /api/trade/{send_slot}/{receive_slot}\"}");
            web_ui_send_response("application/json", response_buffer);
        }
    }
    else if (strncmp(url, "/api/pokemon/", 13) == 0) {
        // Extract slot number from URL
        const char* slot_str = url + 13;
        int slot = atoi(slot_str);
        printf("Serving Pokemon slot %d\n", slot);
        if (slot >= 0 && slot < MAX_POKEMON_STORAGE) {
            web_ui_send_json_pokemon(slot);
        } else {
            snprintf(response_buffer, sizeof(response_buffer), 
                     "{\"error\": \"Invalid slot number: %d\"}", slot);
            web_ui_send_response("application/json", response_buffer);
        }
    }
    else {
        // 404 Not Found
        printf("404 Not Found for URL: %s\n", url);
        const char* not_found = "<h1>404 Not Found</h1><p>The requested resource was not found.</p><p>Available URLs:</p><ul><li>/</li><li>/api/pokemon/list</li><li>/api/pokemon/{slot}</li><li>/api/trade/{send_slot}/{receive_slot}</li></ul>";
        printf("HTTP/1.1 404 Not Found\r\n");
        printf("Content-Type: text/html\r\n");
        printf("Content-Length: %d\r\n", strlen(not_found));
        printf("\r\n");
        printf("%s", not_found);
    }
}

void web_ui_process(void) {
    if (!web_ui_enabled) return;
    
    // This would normally check for incoming HTTP requests
    // For now, it's just a placeholder since we're using console output
}