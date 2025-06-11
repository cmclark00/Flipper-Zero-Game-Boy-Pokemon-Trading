# RP2040 Zero Port: Game Boy Pokémon Trading/Storage

This directory contains a port of the Flipper Zero Game Boy Pokémon Trading/Storage application for the Waveshare RP2040 Zero board, using the custom PCB described at [this repository](https://github.com/Raphael-Boichot/Collection-of-PCB-for-Game-Boy-Printer-Emulators#game-boy-printer-emulator-pcb-for-the-waveshare-rp2040-zero).

## Features
- Game Boy Link Cable communication (level shifter required)
- Minimal UI: single button and status LED
- SD card storage for Pokémon data

## Wiring
- **Game Boy Link Cable**: Connect via the PCB, which uses a 4-gate bidirectional level shifter.
- **GPIO Mapping:**
  - GBP_SO_PIN: GPIO 0
  - GBP_SI_PIN: GPIO 1
  - GBP_SC_PIN: GPIO 2
  - GBP_SD_PIN: GPIO 3
  - STATUS_LED_PIN: GPIO 8
  - BUTTON_PIN: GPIO 9 (example, check your PCB)
- **SD Card**: Connect via SPI (see Pico SDK docs for wiring details)

## Build Instructions
1. Install the [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk) and CMake.
2. Clone this repository and initialize submodules if needed.
3. From this directory, run:
   ```sh
   mkdir build
   cd build
   cmake ..
   make
   ```
4. Flash the resulting UF2 file to your RP2040 Zero.

## Usage
- Press the button to start/stop trading/storage operations.
- The LED indicates status (idle, working, error).
- Pokémon data is stored on the SD card.

## License
GPL-3.0, see main project. 