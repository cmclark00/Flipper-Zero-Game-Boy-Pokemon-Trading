#include "gb_link.h"
#include "storage.h"
#include "ui.h"
#include <stdio.h>
#include "pico/stdlib.h"

int main() {
    stdio_init_all();
    
    // Give extra time for USB to initialize
    sleep_ms(5000);
    
    // Test serial output with a simple loop
    int counter = 0;
    while (counter < 10) {
        printf("=== RP2040 Test Message %d ===\n", counter);
        printf("If you can see this, USB serial is working!\n");
        sleep_ms(1000);
        counter++;
    }
    
    printf("=== RP2040 Game Boy Pokemon Trading Device ===\n");
    printf("Firmware starting up...\n");
    
    ui_init();
    printf("UI initialized\n");
    
    storage_init();
    printf("Storage initialized\n");
    
    gb_link_init();
    printf("Game Boy link interface initialized\n");
    
    printf("=== Device Ready! ===\n");
    printf("Connect Game Boy link cable and start Pokemon Red/Blue/Yellow\n");
    printf("Go to: Trade Center -> Trade with friend\n");
    printf("The device will be PASSIVE and wait for Game Boy to initiate communication\n");

    while (1) {
        printf("=== Waiting passively for Game Boy communication ===\n");
        ui_set_status(UI_STATUS_IDLE);
        
        gb_link_trade_or_store();
        
        printf("=== Communication session ended, resuming passive wait ===\n");
        
        // Wait longer between attempts to avoid interference
        sleep_ms(10000);  // 10 seconds
    }
    return 0;
} 