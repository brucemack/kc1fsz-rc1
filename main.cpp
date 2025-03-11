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
#include <cmath>

#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/clocks.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "hardware/timer.h"
#include "hardware/irq.h"

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
#define TONE_GPIO (10)

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

// Tone synthesis stuff
static volatile bool synth_enabled = false;
static volatile uint32_t synth_ptr = 0;
static const uint32_t synth_size = 256;
static uint8_t synth_table[synth_size];
static struct repeating_timer synth_timer;

// An 8kHz timer is setup to feed new levels into the PWM generator.
// Each time the timer fires we advance forward to the next entry
// in the synth_table.
static bool synth_timer_isr(struct repeating_timer*) {
    uint pwm_slice = pwm_gpio_to_slice_num(TONE_GPIO);
    if (synth_enabled) {
        uint8_t v = synth_table[synth_ptr++];
        if (synth_ptr == synth_size)
            synth_ptr = 0;
        pwm_set_chan_level(pwm_slice, PWM_CHAN_A, v);
    } else {
        pwm_set_chan_level(pwm_slice, PWM_CHAN_A, 0);
    }
    return true;
}

int main(int argc, const char** argv) {

    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    gpio_init(RADIO0_AUDIO_SEL_GPIO);
    gpio_set_dir(RADIO0_AUDIO_SEL_GPIO, GPIO_OUT);
    gpio_put(RADIO0_AUDIO_SEL_GPIO, 0);

    gpio_init(RADIO0_PTT_GPIO);
    gpio_set_dir(RADIO0_PTT_GPIO, GPIO_OUT);
    gpio_put(RADIO0_PTT_GPIO, 0);

    gpio_init(RADIO0_COS_GPIO);
    gpio_set_dir(RADIO0_COS_GPIO, GPIO_IN);

    gpio_init(RADIO1_AUDIO_SEL_GPIO);
    gpio_set_dir(RADIO1_AUDIO_SEL_GPIO, GPIO_OUT);
    gpio_put(RADIO1_AUDIO_SEL_GPIO, 0);

    gpio_init(RADIO1_PTT_GPIO);
    gpio_set_dir(RADIO1_PTT_GPIO, GPIO_OUT);
    gpio_put(RADIO1_PTT_GPIO, 0);

    gpio_init(RADIO1_COS_GPIO);
    gpio_set_dir(RADIO1_COS_GPIO, GPIO_IN);

    // Startup ID
    sleep_ms(500);
    gpio_put(LED_PIN, 1);
    sleep_ms(500);
    gpio_put(LED_PIN, 0);

    printf("Simple Repeater Controller\nCopyright (C) 2025 Bruce MacKinnon KC1FSZ\n");

    int strobe = 0;
    
    // Display/diagnostic should happen once per second
    PicoPollTimer flashTimer;
    flashTimer.setIntervalUs(1000 * 1000);

    PicoPollTimer cosDebounceTimer;
    cosDebounceTimer.setIntervalUs(25 * 1000);

    // Setup PWM test
    gpio_set_function(TONE_GPIO, GPIO_FUNC_PWM);
    uint pwm_slice = pwm_gpio_to_slice_num(TONE_GPIO);
    // Control the size of the loop (inclusive)
    pwm_set_wrap(pwm_slice, 255);
    // Running with no clock divisor, so the pulse frequency is 125,000,000 / 256 
    // or acound 488 kHz.
    // Initiial 50% duty
    pwm_set_chan_level(pwm_slice, PWM_CHAN_A, 128);
    // Run the PWM 
    pwm_set_enabled(pwm_slice, true);

    // Setup timer to drive audio
    add_repeating_timer_us(-125, synth_timer_isr, 0, &synth_timer);

    // Fill the synth table with a 1 kHz tone
    unsigned int fs = 8000;
    unsigned int tone_freq = 1000;
    for (unsigned int i = 0; i < synth_size; i++) {
        float r = 2.0 * 3.1415926 * (float)tone_freq * (float)i / (float)fs;
        float a = ((std::cos(r) + 1.0) / 2.0) * 255.0;
        synth_table[i] = (unsigned char)a;
    }

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
            if (strobe % 2 == 0)
                synth_enabled = true;
            else
                synth_enabled = false;
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
            // Look for RADIO0 RX activity
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
                cosDebounceTimer.reset();
                state = MasterState::RADIO0_RX;
            }
            // Look for RADIO1 RX activity
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
                cosDebounceTimer.reset();
                state = MasterState::RADIO1_RX;
            }
        }
        else if (state == MasterState::RADIO0_RX) {
            // TODO: LOOK FOR TIMEOUT
            // Look for RADIO0 COS drop 
            if (gpio_get(RADIO0_COS_GPIO) == 1) {            
                if (cosDebounceTimer.poll()) {
                    // Unkey radios
                    gpio_put(RADIO0_PTT_GPIO, 0);
                    gpio_put(RADIO1_PTT_GPIO, 0);
                    // Indication
                    gpio_put(LED_PIN, 0);
                    printf("Radio0 stopped receiving\n");
                    state = MasterState::IDLE;
                }
                else {
                    printf("COS0 debounce\n");
                }
            }
        }
        else if (state == MasterState::RADIO1_RX) {
            // TODO: LOOK FOR TIMEOUT
            // Look for RADIO1 drop 
            if (gpio_get(RADIO1_COS_GPIO) == 1) {            
                if (cosDebounceTimer.poll()) {
                    // Unkey radios
                    gpio_put(RADIO0_PTT_GPIO, 0);
                    gpio_put(RADIO1_PTT_GPIO, 0);
                    // Indication
                    gpio_put(LED_PIN, 0);
                    printf("Radio1 stopped receiving\n");
                    state = MasterState::IDLE;
                }
                else {
                    printf("COS1 debounce\n");
                }
            }
        }
    }
}
