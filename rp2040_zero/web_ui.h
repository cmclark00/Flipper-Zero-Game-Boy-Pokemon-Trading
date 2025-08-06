#ifndef WEB_UI_H
#define WEB_UI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Web UI function prototypes
bool web_ui_init(void);
void web_ui_deinit(void);
void web_ui_process(void);

// HTTP request handling
void web_ui_handle_request(const char* request);
void web_ui_send_response(const char* content_type, const char* content);
void web_ui_send_json_pokemon(uint8_t slot);
void web_ui_send_pokemon_list(void);
void web_ui_handle_bidirectional_trade(uint8_t send_slot, uint8_t receive_slot);

// Utility functions
const char* web_ui_get_pokemon_name(uint8_t species_id);
const char* web_ui_get_type_name(uint8_t type_id);
const char* web_ui_get_move_name(uint8_t move_id);

#endif // WEB_UI_H