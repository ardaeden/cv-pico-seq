#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "pico/multicore.h"
#include <cstdio>

volatile uint64_t us_counter = 0;
volatile uint32_t clock_interval_us = 5000; //50000us adds .01 presicion
volatile uint64_t last_tick = 0;
const uint LED_PIN = 25;
volatile bool led_state = false;

bool timer_callback(struct repeating_timer *t) {
    us_counter++;

    if (us_counter >= clock_interval_us) {
        //last_tick = us_counter;

        us_counter = 0;
        led_state = !led_state;
        gpio_put(LED_PIN, led_state);  // LED aç
    }
    return true;
}

void core1_main() {

    struct repeating_timer timer;
    add_repeating_timer_us(-100, timer_callback, NULL, &timer);
    
    while(true) {
        tight_loop_contents();
    }
}

int main() {


    stdio_init_all();
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, true);


    multicore_launch_core1(core1_main);

    while (true) {
        tight_loop_contents();
    }
}
