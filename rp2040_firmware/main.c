#include "pico/stdlib.h"
#include <stdio.h>
#include "pico/cyw43_arch.h" // Required for pico_lwip_sys_threadsafe_background
#include "include/web_server.h"
#include "include/gb_link_protocol.h" // For gb_link_init()
#include "include/trade_logic.h"    // For trade_logic_init()
#include "include/pokemon_storage.h" // For pokemon_storage_init()
#include "tusb.h" // For tud_init

// For pico_usb_net, we need to define the network interface.
// This is usually provided by the pico_usb_net library itself,
// but we might need to declare it extern if it's not in a common header.
// struct netif netif_data; // This will be defined in pico_usb_net internals.

int main() {
  stdio_init_all();
  printf("Hello RP2040! Initializing...\n");

  // Initialize networking stack for RNDIS
  // This typically initializes TinyUSB, lwIP, and the RNDIS interface.
  // For pico_usb_net, this might be a single call or handled implicitly
  // when tud_init and lwip_init are called.
  // pico_lwip_sys_threadsafe_background and pico_usb_net handle much of this.
  
  // Initialize TinyUSB stack for RNDIS. BOARD_TUD_RHPORT is usually 0 for RP2040.
  tud_init(0); 

  // Initialize storage, link protocol, and trade logic first
  pokemon_storage_init();
  gb_link_init();       // Initialize Game Boy link PIO
  trade_logic_init();   // Initialize trade logic

  // Initialize the web server (which now also initializes CGI)
  web_server_init();

  printf("Entering main loop...\n");
  while (true) {
    // TinyUSB device task + RNDIS related tasks
    web_server_task(); 
    
    // Other tasks can go here
    // For example, blinking an LED
    static uint led_pin = PICO_DEFAULT_LED_PIN; // Or CYW43_WL_GPIO_LED_PIN if using CYW43
    static bool first_run = true;
    if (first_run) {
        gpio_init(led_pin);
        gpio_set_dir(led_pin, GPIO_OUT);
        first_run = false;
    }
    static int counter = 0;
    if (counter++ % 100000 == 0) { // Blink roughly
        gpio_put(led_pin, !gpio_get(led_pin));
    }
    // Tight_loop_contents(); // Or sleep_ms(1) if nothing else to do
  }

  return 0; // Should not reach here
}
