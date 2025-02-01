/**
 * Simple Repeater Controller
 * Copyright (C) 2025, Bruce MacKinnon KC1FSZ
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * NOT FOR COMMERCIAL USE WITHOUT PERMISSION.
 */

/*
Targeting RP2040 (Pico 1)

Command used to load code onto the board: 
~/git/openocd/src/openocd -s ~/git/openocd/tcl -f interface/cmsis-dap.cfg -f target/rp040.cfg -c "adapter speed 5000" -c "rp2350.dap.core1 cortex_m reset_config sysresetreq" -c "program main.elf verify reset exit"
*/

#include <stdio.h>
#include <cstring>

#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/clocks.h"
#include "hardware/i2c.h"

#include "kc1fsz-tools/rp2040/PicoPollTimer.h"
#include "kc1fsz-tools/rp2040/PicoPerfTimer.h"

using namespace kc1fsz;

#define LED_PIN (PICO_DEFAULT_LED_PIN)
#define RADIO0_AUDIO_SEL_GPIO (4)
#define RADIO0_PTT_GPIO (5)
#define RADIO0_COS_GPIO (6)
#define RADIO1_AUDIO_SEL_GPIO (7)
#define RADIO1_PTT_GPIO (8)
#define RADIO1_COS_GPIO (9)

enum MasterState {
    RESET,
    IDLE,
    RADIO0_RX_START,
    RADIO1_RX_START,
    RADIO0_RX,
    RADIO1_RX,
    RADIO0_RX_END,
    RADIO1_RX_END,
} state = MasterState::RESET;

int main(int argc, const char** argv) {

    unsigned long fs = 48000;
    unsigned long sck_mult = 384;
    unsigned long sck_freq = sck_mult * fs;
    // Pin to be allocated to I2S SCK (output to CODEC)
    // GP10 is physical pin 14
    uint sck_pin = 10;
    // Pin to be allocated to ~RST
    uint rst_pin = 5;
    // Pin to be allocated to I2S DIN (input from)
    uint din_pin = 6;
    unsigned long system_clock_khz = 129600;
    PicoPerfTimer timer_0;

    // Adjust system clock to more evenly divide the 
    // audio sampling frequency.
    set_sys_clock_khz(system_clock_khz, true);

    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    gpio_init(RADIO0_AUDIO_SEL_GPIO);
    gpio_set_dir(RADIO0_AUDIO_SEL_GPIO, GPIO_OUT);
    gpio_init(RADIO0_PTT_GPIO);
    gpio_set_dir(RADIO0_PTT_GPIO, GPIO_OUT);
    gpio_init(RADIO0_COS_GPIO);
    gpio_set_dir(RADIO0_COS_GPIO, GPIO_IN);

    gpio_init(RADIO1_AUDIO_SEL_GPIO);
    gpio_set_dir(RADIO1_AUDIO_SEL_GPIO, GPIO_OUT);
    gpio_init(RADIO1_PTT_GPIO);
    gpio_set_dir(RADIO1_PTT_GPIO, GPIO_OUT);
    gpio_init(RADIO1_COS_GPIO);
    gpio_set_dir(RADIO1_COS_GPIO, GPIO_IN);

    // Startup ID
    sleep_ms(500);
    sleep_ms(500);

    printf("Simple Repeater Controller\nCopyright (C) 2025 Bruce MacKinnon KC1FSZ\n");

    int strobe = 0;
    
    // Display/diagnostic should happen once per second
    PicoPollTimer flashTimer;
    flashTimer.setIntervalUs(1000 * 1000);

    // ===== Main Event Loop =================================================

    while (true) { 
        
        int c = getchar_timeout_us(0);
        if (c == 's') {
            printf("Status:\n");
            printf("State %d\n", (int)state);
            printf("Radio0 COS %d\n", gpio_get(RADIO0_COS_GPIO));
            printf("Radio1 COS %d\n", gpio_get(RADIO1_COS_GPIO));
        }
        else if (c == 'r') {
            state = MasterState::RESET;
        }

        // Do periodic display/diagnostic stuff
        if (flashTimer.poll()) {
            ++strobe;
        }

        if (state == MasterState::RESET) {
            printf("Resetting ...\n");
            gpio_put(RADIO0_AUDIO_SEL_GPIO, 0);
            gpio_put(RADIO0_PTT_GPIO, 0);
            gpio_put(RADIO1_AUDIO_SEL_GPIO, 1);
            gpio_put(RADIO1_PTT_GPIO, 0);
            state = MasterState::IDLE;
        } 
        else if (state == MasterState::IDLE) {
            // Look for RADIO0 activity
            if (gpio_get(RADIO0_COS_GPIO) == 0) {
                // Both radios transmitting radio 0 input
                gpio_put(RADIO0_AUDIO_SEL_GPIO, 0);
                gpio_put(RADIO1_AUDIO_SEL_GPIO, 0);
                // Key radios
                gpio_put(RADIO0_PTT_GPIO, 1);
                gpio_put(RADIO1_PTT_GPIO, 1);
                // Indicator
                gpio_put(LED_PIN, 1);
                printf("Radio0 receiving\n");
                state = MasterState::RADIO0_RX;
            }
            // Look for RADIO1 activity
            else if (gpio_get(RADIO1_COS_GPIO) == 0) {
                // Both radios transmitting radio 1 input
                gpio_put(RADIO0_AUDIO_SEL_GPIO, 1);
                gpio_put(RADIO1_AUDIO_SEL_GPIO, 1);
                // Key radios
                gpio_put(RADIO0_PTT_GPIO, 1);
                gpio_put(RADIO1_PTT_GPIO, 1);
                // Indication
                gpio_put(LED_PIN, 1);
                printf("Radio1 receiving\n");
                state = MasterState::RADIO1_RX;
            }
        }
        else if (state == MasterState::RADIO0_RX) {
            // TODO: LOOK FOR TIMEOUT
            // Look for RADIO0 drop 
            if (gpio_get(RADIO0_COS_GPIO) == 1) {            
                // Unkey radios
                gpio_put(RADIO0_PTT_GPIO, 0);
                gpio_put(RADIO1_PTT_GPIO, 0);
                // Indication
                gpio_put(LED_PIN, 0);
                printf("Radio0 stopped receiving\n");
                state = MasterState::IDLE;
            }
        }
        else if (state == MasterState::RADIO1_RX) {
            // TODO: LOOK FOR TIMEOUT
            // Look for RADIO1 drop 
            if (gpio_get(RADIO1_COS_GPIO) == 1) {            
                // Unkey radios
                gpio_put(RADIO0_PTT_GPIO, 0);
                gpio_put(RADIO1_PTT_GPIO, 0);
                // Indication
                gpio_put(LED_PIN, 0);
                printf("Radio1 stopped receiving\n");
                state = MasterState::IDLE;
            }
        }
    }
}
