#include "include/web_server.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h" // For lwIP init and RNDIS task

#include "lwip/opt.h"
#include "lwip/apps/httpd.h"
#include "lwip/netif.h" // For netif_list and netif_default

// For RNDIS
#include "tusb.h" // TinyUSB for RNDIS

// --- Custom fsdata for web files ---

// Content of fs/index.html
const char index_html_content[] =
"<!DOCTYPE html>\n"
"<html>\n"
"<head>\n"
"    <title>RP2040 Pokemon Link</title>\n"
"    <link rel=\"stylesheet\" href=\"style.css\">\n"
"</head>\n"
"<body>\n"
"    <h1>RP2040 Pokemon Link</h1>\n"
"\n"
"    <div id=\"status-section\">\n"
"        GameBoy Status: <span id=\"gb-status\">Unknown</span>\n"
"    </div>\n"
"\n"
"    <div id=\"stored-pokemon-section\">\n"
"        <h2>Stored Pokemon:</h2>\n"
"        <ul id=\"pokemon-list\">\n"
"            <!-- List items will be populated by script.js -->\n"
"        </ul>\n"
"    </div>\n"
"\n"
"    <div id=\"trade-controls-section\">\n"
"        <h2>Trade Controls:</h2>\n"
"        <p>Selected Pokemon for Trade: <span id=\"selected-pokemon-trade\">None</span></p>\n"
"        <button id=\"initiate-trade-btn\">Initiate Trade with GameBoy</button>\n"
"    </div>\n"
"\n"
"    <div id=\"log-section\">\n"
"        <h2>Log:</h2>\n"
"        <pre id=\"log-output\"></pre>\n"
"    </div>\n"
"\n"
"    <script src=\"script.js\" defer></script>\n"
"</body>\n"
"</html>";

// Content of fs/style.css
const char style_css_content[] =
"body {\n"
"    font-family: Arial, sans-serif;\n"
"    margin: 20px;\n"
"    background-color: #f4f4f4;\n"
"    color: #333;\n"
"}\n"
"\n"
"h1, h2 {\n"
"    color: #333;\n"
"}\n"
"\n"
"div {\n"
"    background-color: #fff;\n"
"    margin-bottom: 15px;\n"
"    padding: 15px;\n"
"    border-radius: 5px;\n"
"    box-shadow: 0 2px 4px rgba(0,0,0,0.1);\n"
"}\n"
"\n"
"#status-section span,\n"
"#trade-controls-section span {\n"
"    font-weight: bold;\n"
"    color: #555;\n"
"}\n"
"\n"
"ul#pokemon-list {\n"
"    list-style-type: none;\n"
"    padding: 0;\n"
"}\n"
"\n"
"ul#pokemon-list li {\n"
"    padding: 8px;\n"
"    border-bottom: 1px solid #eee;\n"
"}\n"
"\n"
"ul#pokemon-list li:last-child {\n"
"    border-bottom: none;\n"
"}\n"
"\n"
"button {\n"
"    background-color: #007bff;\n"
"    color: white;\n"
"    padding: 10px 15px;\n"
"    border: none;\n"
"    border-radius: 4px;\n"
"    cursor: pointer;\n"
"    font-size: 16px;\n"
"}\n"
"\n"
"button:hover {\n"
"    background-color: #0056b3;\n"
"}\n"
"\n"
"#log-output {\n"
"    background-color: #e9e9e9;\n"
"    border: 1px solid #ddd;\n"
"    padding: 10px;\n"
"    height: 150px;\n"
"    overflow-y: scroll;\n"
"    font-family: monospace;\n"
"    white-space: pre-wrap;\n"
"}";

// Content of fs/script.js
const char script_js_content[] =
"document.addEventListener('DOMContentLoaded', () => {\n"
"    const gbStatusSpan = document.getElementById('gb-status');\n"
"    const pokemonListUl = document.getElementById('pokemon-list');\n"
"    const selectedPokemonSpan = document.getElementById('selected-pokemon-trade');\n"
"    const initiateTradeBtn = document.getElementById('initiate-trade-btn');\n"
"    const logOutputPre = document.getElementById('log-output');\n"
"\n"
"    function logMessage(message) {\n"
"        logOutputPre.innerText += message + '\\n';\n"
"        logOutputPre.scrollTop = logOutputPre.scrollHeight;\n"
"    }\n"
"\n"
"    function fetchStatus() {\n"
"        logMessage('Fetching GameBoy status...');\n"
"        // Placeholder:\n"
"        gbStatusSpan.textContent = 'Simulated Connected';\n"
"        logMessage('Status updated (simulated).');\n"
"    }\n"
"\n"
"    function fetchStoredPokemon() {\n"
"        logMessage('Fetching stored Pokemon list...');\n"
"        // Placeholder:\n"
"        pokemonListUl.innerHTML = '';\n"
"        for (let i = 0; i < 6; i++) {\n"
"            const li = document.createElement('li');\n"
"            li.textContent = `Slot ${i + 1}: Empty (Simulated)`;\n"
"            li.addEventListener('click', () => selectPokemonForTrade({ name: `Sim Pkmn ${i+1}`, id: i }, i + 1));\n"
"            pokemonListUl.appendChild(li);\n"
"        }\n"
"        logMessage('Pokemon list updated (simulated).');\n"
"    }\n"
"    \n"
"    let currentSelectedPokemon = null;\n"
"\n"
"    function selectPokemonForTrade(pokemon, slotNumber) {\n"
"        currentSelectedPokemon = { ...pokemon, slot: slotNumber };\n"
"        selectedPokemonSpan.textContent = `${pokemon.name} (from Slot ${slotNumber})`;\n"
"        logMessage(`Selected for trade: ${pokemon.name} from Slot ${slotNumber}`);\n"
"    }\n"
"\n"
"    function initiateTrade() {\n"
"        if (!currentSelectedPokemon) {\n"
"            logMessage('No Pokemon selected for trade.');\n"
"            alert('Please select a Pokemon to trade first.');\n"
"            return;\n"
"        }\n"
"        logMessage(`Initiating trade with GameBoy for ${currentSelectedPokemon.name}...`);\n"
"        // Placeholder:\n"
"        logMessage('Trade initiated (simulated). Result: Success!');\n"
"    }\n"
"\n"
"    if (initiateTradeBtn) {\n"
"        initiateTradeBtn.addEventListener('click', initiateTrade);\n"
"    }\n"
"\n"
"    fetchStatus();\n"
"    fetchStoredPokemon();\n"
"\n"
"    logMessage('Pokemon Link UI Initialized.');\n"
"});";


// fs_file structure for our custom files
// We need to associate content types with these files.
// One way is to add a content_type field to a custom fs_file struct,
// or handle it in fs_open_custom and set it via a global/static variable
// that httpd_send_headers can access.
// For simplicity with the existing httpd API, we'll rely on the file extension
// hint in the httpd internals if possible, or just serve as binary/octet-stream
// if it doesn't pick up the type correctly without more complex SSI/CGI header injection.
// The `flags` field can indicate persistence.

// Custom struct to hold content type, as LWIP's default fs_file doesn't have it directly.
typedef struct {
    const char *name;
    const char *data;
    int len;
    u16_t flags;
    const char *content_type;
} custom_fs_file_t;

const custom_fs_file_t custom_fs_data_files[] = {
    {"/index.html", index_html_content, sizeof(index_html_content) - 1, FS_FILE_FLAGS_HEADER_PERSISTENT | FS_FILE_FLAGS_HEADER_HTTPVER_1_1, "text/html"},
    {"/style.css", style_css_content, sizeof(style_css_content) - 1, FS_FILE_FLAGS_HEADER_PERSISTENT | FS_FILE_FLAGS_HEADER_HTTPVER_1_1, "text/css"},
    {"/script.js", script_js_content, sizeof(script_js_content) - 1, FS_FILE_FLAGS_HEADER_PERSISTENT | FS_FILE_FLAGS_HEADER_HTTPVER_1_1, "application/javascript"},
};

#define NUM_CUSTOM_FILES (sizeof(custom_fs_data_files) / sizeof(custom_fs_data_files[0]))

// This is a bit of a hack to pass content type. LWIP's httpd doesn't have a clean way
// to set content type for custom fsdata files without SSI, modifying httpd source,
// or using LWIP_HTTPD_DYNAMIC_HEADERS with a custom header generation function.
// We'll store the last opened file's content type here.
// This is NOT thread-safe if httpd handles multiple requests concurrently without separate contexts.
// However, for pico-lwip with NO_SYS=0 and single-threaded httpd access (common for these devices), it works.
static const char* s_last_content_type = "text/plain";


// Required by LWIP_HTTPD_CUSTOM_FSDATA
int fs_open_custom(struct fs_file *file, const char *name) {
    for (size_t i = 0; i < NUM_CUSTOM_FILES; ++i) {
        if (strcmp(name, custom_fs_data_files[i].name) == 0 || (strcmp(name, "/") == 0 && strcmp(custom_fs_data_files[i].name, "/index.html") == 0) ) {
            file->data = custom_fs_data_files[i].data;
            file->len = custom_fs_data_files[i].len;
            file->index = custom_fs_data_files[i].len; // For range requests, not fully supported here
            file->flags = custom_fs_data_files[i].flags;
            file->pextension = NULL; // Not using custom pextension for SSI tags
            
            // Store content type for httpd_send_response_headers_for_file (if it uses it)
            // This is a workaround. A better way is to modify httpd to accept content type from fs_file.
            s_last_content_type = custom_fs_data_files[i].content_type; 
            
            // If httpd has a field in fs_file for content_type, set it here.
            // Example: file->content_type = custom_fs_data_files[i].content_type;
            // Since it doesn't, this is the best we can do without modifying httpd.c

            return 1; // Success
        }
    }
    return 0; // File not found
}

void fs_close_custom(struct fs_file *file) {
    (void)file; // Nothing to do for in-memory files
}

// This function might be needed if LWIP_HTTPD_DYNAMIC_HEADERS is enabled
// and we want to inject the Content-Type dynamically.
// For now, we rely on the server hopefully picking a default or the s_last_content_type hack.
/*
int httpd_send_response_headers_for_file_custom(struct http_state *hs, struct fs_file *file) {
    (void)file; // hs is the http connection state
    char content_type_hdr[64];
    snprintf(content_type_hdr, sizeof(content_type_hdr), "Content-Type: %s\r\n", s_last_content_type);
    httpd_send_headers(hs, content_type_hdr);
    return 0; // 0 for OK, -1 for error
}
*/


// Network interface (will be RNDIS)
extern struct netif netif_data; // Defined by pico_usb_net

// Include CGI handlers
#include "include/cgi_handlers.h"

void web_server_init(void) {
    httpd_init();
    cgi_init(); // Initialize CGI handlers
    printf("HTTPD and CGI Initialized.\n");
    printf("Web server init complete. Connect to http://192.168.7.1/\n");
}

void web_server_task(void) {
    tud_task(); // TinyUSB device task

    // Check if data is available for RNDIS
    if (tud_network_packet_rx_available()) {
        struct pbuf *p = tud_network_recv_renew();
        if (p != NULL) {
            if (netif_data.input(p, &netif_data) != ERR_OK) {
                pbuf_free(p);
            }
        }
    }
    // Let LwIP process timers
    sys_check_timeouts();
}
