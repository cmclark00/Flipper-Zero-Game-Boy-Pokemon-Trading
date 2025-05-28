# Pokémon Trading Tool - RP2040 Version

This directory contains the firmware for running the Pokémon trading tool on an RP2040-based board (e.g., RP2040-Zero).

## Features
- Communication with Game Boy (Gen I & II) via link cable (using PIO).
- Web-based UI served directly from the RP2040 over USB (RNDIS).
- Storage for up to 6 Pokémon on the RP2040's flash memory.
- API for UI interaction (status, list Pokémon, initiate trade).

## Hardware Notes
- **Game Boy Link Cable Pins:**
    - Connect Game Boy Link Cable CLK to RP2040 GPIO0. (Note: This is currently hardcoded in the PIO program `gb_link_protocol.pio.h` via `wait 0 pin 0`).
    - Connect Game Boy Link Cable SI (Flipper's SO, GB's SO) to RP2040 GPIO1.
    - Connect Game Boy Link Cable SO (Flipper's SI, GB's SI) to RP2040 GPIO2.
    - Connect Game Boy Link Cable GND to RP2040 GND.
- **USB Connection:** The web interface is accessible via RNDIS (Ethernet over USB) when the RP2040 is connected to a PC. The default IP is usually 192.168.7.1.

## Build Instructions
1. Ensure the Pico SDK is installed and `PICO_SDK_PATH` is set correctly in your environment.
2. Navigate to the root of this repository.
3. Create a build directory: `mkdir build`
4. Navigate into the build directory: `cd build`
5. Configure CMake, pointing to the parent directory where the main `CMakeLists.txt` (if any) and the `rp2040_firmware` directory reside: `cmake ..` (If your main CMakeLists.txt is in the repo root and it adds `rp2040_firmware` as a subdirectory, this is correct. If you are building `rp2040_firmware` standalone, you might do `cmake ../rp2040_firmware` from the build dir, or `cmake .` from `rp2040_firmware/build`).
   For this project structure, assuming you are in `pokemon_trading_tool/build/`: `cmake ..`
   Then, specifically for the RP2040 firmware, you might target it if there are multiple targets, or it will be built by default.
   A more direct way for this specific firmware if it's standalone or the primary target:
   `cd rp2040_firmware`
   `mkdir build && cd build`
   `cmake ..`
6. Compile: `make` (or `make rp2040_firmware` if that's the target name from a higher-level CMake).
7. Flash the resulting `.uf2` file (e.g., `build/rp2040_firmware/rp2040_firmware.uf2`) to your RP2040.

## Current Status
- Core trading logic (data structures) from the Flipper Zero project has been ported.
- PIO-based communication for the Game Boy link cable (slave mode) is implemented.
- A web-based UI is served over RNDIS, allowing users to view link status, list (simulated) stored Pokémon, and initiate (simulated) trades.
- Pokémon data can be stored persistently on the RP2040's flash memory.
- The API endpoints for status, listing, and trade initiation are functional (trade execution is stubbed).
- Actual trade sequence with a Game Boy (full data exchange, timing validation) requires further real-hardware testing and potential refinement of the PIO program and trade state machine.
- Error handling in some parts of the communication and storage logic could be more robust.
- The web UI currently uses placeholder data for some dynamic content until full API backends are complete or real-time status updates are implemented via WebSockets or polling.
