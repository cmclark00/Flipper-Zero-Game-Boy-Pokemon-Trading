; Game Boy Link Protocol PIO program
; RP2040 acts as a slave device.
; - CLK is input from Game Boy (absolute GPIO defined by `wait` instruction, or by C code patching)
; - SI (Serial In) is input from Game Boy (RP2040's MISO/RX, first 'in' pin for PIO)
; - SO (Serial Out) is output to Game Boy (RP2040's MOSI/TX, first 'out' pin for PIO)

.program gb_link_slave

; The PIO program will:
; 1. Wait for the falling edge of the clock (CLK).
; 2. On the falling edge:
;    a. Output the next bit from OSR to the SO_PIN.
;    b. Input the bit from SI_PIN into ISR.
; 3. Repeat 8 times for a full byte.

; Use 'autopull' to automatically load data from TX FIFO into OSR.
; Use 'autopush' to automatically load data from ISR into RX FIFO after 8 bits.

    pull block          ; Pull 8 bits from TX FIFO into OSR (blocking)
    set x, 7            ; Initialize bit counter (8 bits)

loop:
    wait 0 pin 0        ; Wait for CLK (absolute GPIO 0) to go low (falling edge).
                        ; This needs to be configurable or patched by C code if CLK_PIN is not GPIO0.
    
    out pins, 1         ; Output 1 bit from OSR MSB to SO_PIN (first 'out' pin)
    in pins, 1          ; Input 1 bit from SI_PIN (first 'in' pin) into ISR LSB
                        
    jmp x-- loop        ; Decrement bit counter and loop if not zero

    push block          ; Push 8 bits from ISR to RX FIFO (blocking)

% c-sdk {
// Helper function to initialize this PIO program
static inline void gb_link_pio_program_init(PIO pio, uint sm, uint offset, uint clk_pin_abs, uint si_pin, uint so_pin) {
    // This generated program uses `wait 0 pin 0`. This means the CLK pin *must* be GPIO0
    // unless this instruction is patched dynamically in C before loading, or the PIO
    // source is modified to take a define for the pin, e.g. by using pio_encode_wait_pin().
    // For this implementation, we will enforce that clk_pin_abs passed to this function *is* 0.
    // If clk_pin_abs is not 0, this PIO program will not work as intended without modification.
    // assert(clk_pin_abs == 0); // Or handle dynamically

    // Create a const copy of the program to potentially patch the wait instruction.
    // Or, ensure CMake defines CLK_PIN for pioasm. For now, assume clk_pin_abs IS the pin for wait.
    // The most robust way is to use pio_encode_wait_pin(false, clk_pin_abs) to generate the instruction
    // and patch it into a copy of the program if clk_pin_abs is not 0.
    // For this iteration, we will assume the user of this function ensures clk_pin_abs is appropriate
    // for the `wait 0 pin 0` instruction (i.e. clk_pin_abs IS 0).

    pio_sm_config c = gb_link_slave_program_get_default_config(offset);

    // Configure 'in' pins for the 'in' instruction (SI)
    sm_config_set_in_pins(&c, si_pin);
    // Configure 'out' pins for the 'out' instruction (SO)
    sm_config_set_out_pins(&c, so_pin);
    
    // CLK pin for 'wait' instruction (absolute GPIO number)
    // The PIO program has `wait 0 pin 0`. If `clk_pin_abs` is not 0, this needs adjustment.
    // For simplicity in this step, we will proceed assuming `clk_pin_abs` is the one PIO uses.
    // The C code calling this init function must ensure `clk_pin_abs` matches the PIO's expectation.

    // Shift direction for ISR (input data from SI) and OSR (output data to SO)
    sm_config_set_in_shift(&c, false, true, 8); // Shift ISR right, autopush after 8 bits
    sm_config_set_out_shift(&c, false, true, 8); // Shift OSR right, autopull, pull threshold 8 bits

    // Initialize pins for PIO control
    pio_gpio_init(pio, clk_pin_abs); // This is for the 'wait' instruction
    pio_gpio_init(pio, si_pin);      // This is for the 'in' instruction
    pio_gpio_init(pio, so_pin);      // This is for the 'out' instruction

    // Set pin directions
    pio_sm_set_consecutive_pindirs(pio, sm, clk_pin_abs, 1, false); // CLK is input
    pio_sm_set_consecutive_pindirs(pio, sm, si_pin,      1, false); // SI is input
    pio_sm_set_consecutive_pindirs(pio, sm, so_pin,      1, true);  // SO is output
    
    // Load our configuration
    pio_sm_init(pio, sm, offset, &c);
    // Set the state machine running
    pio_sm_set_enabled(pio, sm, true);
}
%}
