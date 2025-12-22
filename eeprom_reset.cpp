#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"

// EEPROM magic byte'ı sıfırla
int main() {
    stdio_init_all();
    
    // I2C init
    i2c_init(i2c1, 400000);
    gpio_set_function(26, GPIO_FUNC_I2C);  // SDA
    gpio_set_function(27, GPIO_FUNC_I2C);  // SCL
    gpio_pull_up(26);
    gpio_pull_up(27);
    
    sleep_ms(100);
    
    // EEPROM adresi 0x50, magic byte address 1900
    uint8_t eeprom_addr = 0x50 | ((1900 >> 8) & 0x07);
    uint8_t local_addr = 1900 & 0xFF;
    
    // Magic byte'ı 0x00 yap
    uint8_t buf[2] = {local_addr, 0x00};
    int result = i2c_write_blocking(i2c1, eeprom_addr, buf, 2, false);
    
    sleep_ms(20);
    
    // LED ile geri bildirim (GP3)
    gpio_init(3);
    gpio_set_dir(3, GPIO_OUT);
    
    if (result > 0) {
        // Başarılı - 3 kere yanıp sön
        for (int i = 0; i < 3; i++) {
            gpio_put(3, 1);
            sleep_ms(200);
            gpio_put(3, 0);
            sleep_ms(200);
        }
    } else {
        // Hata - sürekli yanık kal
        gpio_put(3, 1);
    }
    
    while (1) {
        tight_loop_contents();
    }
    
    return 0;
}
