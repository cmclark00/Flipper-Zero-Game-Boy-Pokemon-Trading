# Pokemon Trade Tool - RP2040 Zero Port

This is a port of the Pokemon Trade Tool from Flipper Zero to the Waveshare RP2040 Zero board. It allows trading Pokemon to and from Game Boy systems (Generation I & II) using a minimal hardware setup.

## Features

- **Game Boy Link Cable Communication**: Full support for Pokemon trading with Game Boy systems
- **Minimal UI**: Single button operation with LED status indicators  
- **Onboard Flash Storage**: Store up to 20 Pokemon in the RP2040's onboard flash memory
- **Automatic Trade Detection**: Automatically responds to Game Boy trade requests
- **Manual Trade Mode**: Button-initiated trades for testing and manual operation

## Hardware Requirements

### RP2040 Zero Board
- Waveshare RP2040 Zero development board
- USB-C cable for programming and power
- Breadboard or PCB for connections (optional)

### Game Boy Link Cable Connection

Connect the Game Boy Link Cable to the RP2040 Zero as follows:

| Game Boy Link Pin | RP2040 Zero GPIO | Function |
|-------------------|------------------|----------|
| Pin 2 (SO)        | GPIO 7           | Serial Out (Game Boy to RP2040) |
| Pin 3 (SI)        | GPIO 8           | Serial In (RP2040 to Game Boy) |
| Pin 5 (CLK)       | GPIO 6           | Clock |
| Pin 6 (GND)       | GND              | Ground |

### Additional Connections

| Component | RP2040 Zero GPIO | Notes |
|-----------|------------------|-------|
| User Button | GPIO 9 | Pull-up resistor recommended |
| Status LED | GPIO 25 | Onboard LED (built-in) |

## Building the Project

### Prerequisites

1. **Raspberry Pi Pico SDK**: Install the Pico SDK following the [official guide](https://github.com/raspberrypi/pico-sdk)
2. **CMake**: Version 3.13 or higher
3. **Build Tools**: GCC ARM cross-compiler

### Build Steps

1. Clone or copy the source files to your development environment
2. Set the Pico SDK path:
   ```bash
   export PICO_SDK_PATH=/path/to/pico-sdk
   ```
3. Create a build directory and compile:
   ```bash
   mkdir build
   cd build
   cmake ..
   make
   ```
4. The resulting `pokemon_trade_rp2040.uf2` file can be flashed to the RP2040 Zero

### Flashing the Firmware

1. Hold the BOOT button on the RP2040 Zero while connecting USB
2. The board will appear as a USB mass storage device
3. Copy the `pokemon_trade_rp2040.uf2` file to the mounted drive
4. The board will automatically reboot with the new firmware

## Usage

### LED Status Indicators

| LED Pattern | Meaning |
|-------------|---------|
| Slow Blink (1 sec) | Ready, waiting for Game Boy connection |
| Fast Blink (200ms) | Connected to Game Boy or processing trade |
| Solid On | Trade ready state |
| Heartbeat (double blink) | Trade in progress |
| Rapid Flash | Error condition |

### Operation Modes

#### Automatic Mode (Default)
1. Power on the RP2040 Zero (LED will start slow blinking)
2. Connect the Game Boy Link Cable
3. On the Game Boy, enter a Pokemon Center and access the Trade Center
4. The RP2040 will automatically detect the connection and be ready to trade
5. Select a Pokemon to trade on the Game Boy
6. The trade will complete automatically

#### Manual Mode
1. Press and hold the user button to initiate a trade attempt
2. The LED will fast blink while attempting to connect
3. Follow the automatic mode steps on the Game Boy side
4. Release the button once trading is complete

### Default Pokemon

The RP2040 Zero comes pre-loaded with a Level 10 Bulbasaur with the following stats:
- **Species**: Bulbasaur (#001)
- **Level**: 10
- **Moves**: Tackle, Growl, Leech Seed, Vine Whip
- **OT Name**: FLIPPER
- **OT ID**: 0x1234

## Storage System

The RP2040 Zero uses its onboard flash memory to store Pokemon data:

- **Capacity**: Up to 20 Pokemon
- **Persistence**: Data survives power cycles
- **Auto-save**: Traded Pokemon are automatically saved to slot 0
- **Format**: Compatible with Generation I & II Pokemon data structure

## Troubleshooting

### Common Issues

**LED not blinking after power-on**
- Check USB connection and power
- Verify firmware was flashed correctly
- Try reflashing the firmware

**Game Boy not connecting**
- Verify wiring connections match the pin table
- Check for loose connections
- Ensure Game Boy is in Trade Center mode
- Try different Game Boy games (Gen I/II only)

**Trades failing or corrupting**
- Check all ground connections
- Verify clock signal integrity
- Ensure stable power supply to both devices
- Try slower trade timing by modifying software delays

### Debug Output

The RP2040 Zero outputs debug information via USB serial at 115200 baud. Connect to view:
- Connection status
- Trade progress
- Error messages
- Pokemon data verification

## Compatibility

### Supported Game Boy Systems
- Game Boy (DMG)
- Game Boy Pocket
- Game Boy Color
- Game Boy Advance
- Game Boy Advance SP
- Analogue Pocket

### Supported Games
- Pokemon Red/Blue/Yellow (Generation I)
- Pokemon Gold/Silver/Crystal (Generation II)
- Non-Japanese versions only

**Note**: Japanese versions use different data structures and are not supported.

## Technical Details

### Communication Protocol
The RP2040 implements the Game Boy Link Cable protocol as a slave device:
- **Clock Mode**: External (Game Boy provides clock)
- **Data Rate**: 8192 Hz (Game Boy standard)
- **Protocol**: Pokemon-specific SPI-like communication
- **Handshake**: Automatic master/slave negotiation

### Memory Layout
- **Program Flash**: Firmware storage
- **Storage Flash**: 1MB offset for Pokemon data (configurable)
- **RAM**: Runtime data and communication buffers

### Power Consumption
- **Active**: ~50mA @ 5V (during trades)
- **Idle**: ~20mA @ 5V (waiting for connection)
- **USB Powered**: No external power required

## Development

### File Structure
```
rp2040_zero/
├── CMakeLists.txt          # Build configuration
├── main.c                  # Main application logic
├── gb_link.c/.h           # Game Boy communication protocol
├── storage.c/.h           # Flash storage management
├── ui.c/.h                # LED and button interface
├── pico_sdk_import.cmake  # Pico SDK integration
└── README.md              # This file
```

### Customization
- Modify `default_pokemon_data` in `main.c` to change the default Pokemon
- Adjust timing constants in `gb_link.c` for different Game Boy variants
- Change storage capacity in `storage.h` (limited by flash size)
- Add additional GPIO features in `ui.c`

## License

This project maintains the same license as the original Flipper Zero Pokemon Trade Tool.

## Credits

- Original Flipper Zero Pokemon Trade Tool by Kris Bahnsen and contributors
- RP2040 port implementation
- Game Boy communication protocol research by Adan Scotney
- Raspberry Pi Pico SDK by Raspberry Pi Foundation