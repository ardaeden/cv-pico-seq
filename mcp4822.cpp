#include "mcp4822.h"

#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

static uint CS_PIN = 17; // default CS = GP17

void mcp4822_init(uint cs_pin) {
    CS_PIN = cs_pin;
    // init SPI0 at 8 MHz, mode 0
    spi_init(spi0, 8000000);
    // SCK = GP18, MOSI = GP19
    gpio_set_function(18, GPIO_FUNC_SPI);
    gpio_set_function(19, GPIO_FUNC_SPI);
    // CS pin as output high
    gpio_init(CS_PIN);
    gpio_set_dir(CS_PIN, GPIO_OUT);
    gpio_put(CS_PIN, true);
}

void mcp4822_set_voltage(uint8_t channel, uint8_t gain, uint16_t value) {
    // sanitize
    if (value > 0x0FFF) value = 0x0FFF;
    uint16_t command = 0;
    // Build command: [C1 C0 | BUF | GA | SHDN | D11..D0]
    // Use similar mapping to sample: channel selects bit, gain as in sample.
    // For MCP4822: command high nibble for control bits. We'll follow sample's bits.
    if (channel) command |= 0x8000; // select channel B (bit 15)
    else command |= 0x0000; // channel A

    // gain bit: sample used 1 -> 2X, set Gx = 0 for 2x per MCP4822 datasheet
    if (gain == 0) {
        // 1x: set GA = 1 per datasheet (in our mapping set the bit)
        command |= 0x2000; // set bit so that gain = 1X
    } else {
        // 2x: GA = 0
        // nothing to OR
    }

    // shutdown bit (active) = 1 for active
    command |= 0x1000;

    // data bits
    command |= (value & 0x0FFF);

    // Send MSB first
    uint8_t outbuf[2];
    outbuf[0] = (command >> 8) & 0xFF;
    outbuf[1] = command & 0xFF;

    // Toggle CS and send two bytes
    gpio_put(CS_PIN, false);
    spi_write_blocking(spi0, outbuf, 2);
    gpio_put(CS_PIN, true);
}
