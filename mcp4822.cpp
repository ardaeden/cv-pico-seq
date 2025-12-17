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

void mcp4822_set_voltage(uint8_t channel, uint16_t value) {
    if (value > 0x0FFF) value = 0x0FFF;
    uint16_t command = 0;
    
    // Channel select
    if (channel) command |= 0x8000;
    
    // 2X gain: GA = 0 (bit 13 cleared)
    // 1X gain would be: command |= 0x2000;
    
    // Shutdown bit (active)
    command |= 0x1000;
    
    // Data bits
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
