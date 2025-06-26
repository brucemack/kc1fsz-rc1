/**
 * Software Defined Repeater Controller
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
When targeting RP2350 (Pico 2), command used to load code onto the board: 
~/git/openocd/src/openocd -s ~/git/openocd/tcl -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "adapter speed 5000" -c "rp2350.dap.core1 cortex_m reset_config sysresetreq" -c "program main.elf verify reset exit"
*/

#include <cstdio>
#include <cstring>
#include <cmath>

#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
#include "hardware/watchdog.h"

#include "kc1fsz-tools/Log.h"
#include "kc1fsz-tools/CommandShell.h"
#include "kc1fsz-tools/OutStream.h"
#include "kc1fsz-tools/rp2040/PicoPollTimer.h"
#include "kc1fsz-tools/rp2040/PicoPerfTimer.h"
#include "kc1fsz-tools/rp2040/PicoClock.h"

#include "hw/StdTx.h"
#include "hw/StdRx.h"

#include "Config.h"
#include "AudioSourceControl.h"
#include "i2s.pio.h"
#include "TxControl.h"
#include "ShellCommand.h"

using namespace kc1fsz;

// ===========================================================================
// CONFIGURATION PARAMETERS
// ===========================================================================
//
// GPIO pin to be allocated to I2S SCK (output to CODEC)
const uint sck_pin = 4;
// Pin to be allocated to ADC ~RST
const uint adc_rst_pin = 5;
// Pin to be allocated to ADC I2S DIN (input from)
const uint adc_din_pin = 6;
// Pin to be allocated to DAC I2S DOUT 
const uint dac_dout_pin = 9;

#define LED_PIN (PICO_DEFAULT_LED_PIN)
#define R0_COS_PIN (14)
#define R0_CTCSS_PIN (13)
#define R0_PTT_PIN (12)
#define R1_COS_PIN (17)
#define R1_CTCSS_PIN (16)
#define R1_PTT_PIN (15)

#define WATCHDOG_INTERVAL_MS (2000)

// Number of ADC samples in a block
#define ADC_SAMPLE_COUNT (256)
#define ADC_SAMPLE_BYTES_LOG2 (11)
#define DAC_SAMPLE_BYTES_LOG2 (11)
// Audio sample rate
// CHANGED AUDIO RATE ON 22-JUNE-2025
//#define FS_HZ (48000)
#define FS_HZ (32000)

// Controls the maximum value that can be output from the DAC
// (plus or minus) before clipping happens somewhere in the audio 
// output path.
#define MAX_DAC_VALUE (8388607)
// Size of history buffers (around a second of history)
#define IN_HISTORY_COUNT (16384)
#define OUT_HISTORY_COUNT (16384)

// How long an audio source takes to ramp up/down to avoid pops 
#define AUDIO_FADE_MS (25)

// ===========================================================================
// DIAGNOSTIC COUNTERS/FLAGS
// ===========================================================================
//
static volatile uint32_t dma_in_count = 0;
static volatile uint32_t dma_out_count = 0;

// ===========================================================================
// DMA REALTED 
// ===========================================================================
//
// Buffer used to drive the DAC via DMA. 2* for L and R
#define DAC_BUFFER_SIZE (ADC_SAMPLE_COUNT * 2)
// 4* for 32-bit integers
#define DAC_BUFFER_ALIGN (DAC_BUFFER_SIZE * 4)
// 4096-byte alignment is needed because we are using a DMA channel in ring mode
// and all buffers must be aligned to a power of two boundary.
// 256 samples * 2  words per sample * 4 bytes per word = 2048 bytes
static __attribute__((aligned(DAC_BUFFER_ALIGN))) uint32_t dac_buffer_ping[DAC_BUFFER_SIZE];
static __attribute__((aligned(DAC_BUFFER_ALIGN))) uint32_t dac_buffer_pong[DAC_BUFFER_SIZE];

// Buffer used to drive the ADC via DMA.
// When running at 48kHz, each buffer of 384 samples represents 8ms of activity
// When running at 48kHz, each buffer of 512 samples represents 10ms of activity
// The *2 accounts for left + right channels
#define ADC_BUFFER_SIZE (ADC_SAMPLE_COUNT * 2)
// Here is where the actual audio data gets written
// The *2 accounts for the fact that we are double-buffering.
static __attribute__((aligned(8))) uint32_t adc_buffer[ADC_BUFFER_SIZE * 2];
// Here is where the buffer addresses are stored to control ADC DMA
// The *2 accounts for the fact that we are double-buffering.
static __attribute__((aligned(8))) uint32_t* adc_addr_buffer[2];

// DMA channel allocations
static uint dma_ch_in_ctrl = 0;
static uint dma_ch_in_data = 0;
static uint dma_ch_out_data0 = 0;
static uint dma_ch_out_data1 = 0;

// This flag is used to manage the alternating buffers. "ping open"
// indicates that the ping buffer was just written and should be sent
// out on the next opportunity. Otherwise, it's the pong buffer that
// was just written and is waiting to be sent.
static volatile bool dac_buffer_ping_open = false;

// ===========================================================================
// HISTORY BUFFERS
// ===========================================================================
//
static float in_history_r0[IN_HISTORY_COUNT];
static float in_history_r1[IN_HISTORY_COUNT];
static uint in_history_ptr = 0;

// The output history is stored with 8-bit resolution becaue
// it is only being used for output level calculations.
static int8_t out_history_r0[OUT_HISTORY_COUNT];
static int8_t out_history_r1[OUT_HISTORY_COUNT];
static uint out_history_ptr = 0;

// ===========================================================================
// RUNTIME OBJECTS
// ===========================================================================
//
// The global configuration parameters
static Config config;
static bool configChanged = false;

static PicoClock clock;
// Objects used for tone generation (CW, courtesy, PL, etc.)
static ToneSynthesizer toneSynth0(FS_HZ, AUDIO_FADE_MS);
static ToneSynthesizer toneSynth1(FS_HZ, AUDIO_FADE_MS);
static ToneSynthesizer plSynth0(FS_HZ, AUDIO_FADE_MS);
static ToneSynthesizer plSynth1(FS_HZ, AUDIO_FADE_MS);
static AudioSourceControl audioSource0(clock, AUDIO_FADE_MS);
static AudioSourceControl audioSource1(clock, AUDIO_FADE_MS);

// A tone generator used for diagnostic and calibration
static ToneSynthesizer diagSynth0(FS_HZ, AUDIO_FADE_MS);
static ToneSynthesizer diagSynth1(FS_HZ, AUDIO_FADE_MS);

// Controls for diagnostic tone
static float diagOn = false;
static float diagScaleDb = -10.0;
static float diagScaleLinear = pow(10, (diagScaleDb / 20)) * MAX_DAC_VALUE;
static float diagFreqHz = 700.0;

// Soft gain controls

// Scale of tone vs full-scale. Per Dan, this should be around
// -10dB of full-scale.
static float toneGain_r0 = 0.33;
// PL is 25% of tone
static float plGain_r0 = 0.33 * 0.25;
static float audioGain_r0 = 1.0;
static float audioGain_t0 = 1.0;

static float toneGain_r1 = 0.33;
static float plGain_r1 = 0.33 * 0.25;
static float audioGain_r1 = 1.0;
static float audioGain_t1 = 1.0;

static void process_in_frame();

// The console can work in one of three modes:
// 
// Log    - A stream of log/diagnostic messages (default)
// Shell  - An interactive command prompt
// Status - A continuously updated live status page
//
enum UIMode { UIMODE_LOG, UIMODE_SHELL, UIMODE_STATUS };
static UIMode uiMode = UIMode::UIMODE_LOG;

// Used for integrating with the command shell
class ShellOutput : public OutStream {
public:

    /**
     * @returns Number of bytes actually written.
     */
    int write(uint8_t b) { printf("%c", (char)b); return 1; }
    bool isWritable() const { return true; }
};

// This will be called once every AUDIO_BUFFER_SIZE/2 samples.
// VERY IMPORTANT: This interrupt handler needs to be fast enough 
// to run inside of one sample block.
static void dma_adc_handler() {   

    dma_in_count++;
    process_in_frame();

    // Clear the IRQ status
    dma_hw->ints0 = 1u << dma_ch_in_data;
}

static void dma_dac0_handler() {   

    dma_out_count++;
    dac_buffer_ping_open = true;

    // Clear the IRQ status
    dma_hw->ints0 = 1u << dma_ch_out_data0;
}

static void dma_dac1_handler() {   

    dma_out_count++;
    dac_buffer_ping_open = false;

    // Clear the IRQ status
    dma_hw->ints0 = 1u << dma_ch_out_data1;
}

static void dma_irq_handler() {   
    // Figure out which interrupt fired
    if (dma_hw->ints0 & (1u << dma_ch_in_data)) {
        dma_adc_handler();
    }
    if (dma_hw->ints0 & (1u << dma_ch_out_data0)) {
        dma_dac0_handler();
    }
    if (dma_hw->ints0 & (1u << dma_ch_out_data1)) {
        dma_dac1_handler();
    }
}

// -----------------------------------------------------------------------------
// IMPORTANT FUNCTION: 
//
// This should be called when a complete frame of audio 
// data has been converted. The audio output is generated
// in this function.
//
static void process_in_frame() {

    dma_in_count++;

    // Counter used to alternate between double-buffer sides
    static uint32_t dma_count_0 = 0;

    // Figure out which part of the double-buffer we just finished
    // loading into.  Notice: the pointer is signed.
    int32_t* adc_data;
    if (dma_count_0 % 2 == 0) {
        adc_data = (int32_t*)adc_buffer;
    } else {
        adc_data = (int32_t*)&(adc_buffer[ADC_BUFFER_SIZE]);
    }
    dma_count_0++;

    // Choose the appropriate DAC buffer based on our current tracking of which 
    // is available for use.
    int32_t* dac_buffer;
    if (dac_buffer_ping_open) 
        dac_buffer = (int32_t*)dac_buffer_ping;
    else
        dac_buffer = (int32_t*)dac_buffer_pong;

    unsigned int j = 0;
    //const float toneScale = MAX_DAC_VALUE * toneTONE_SCALE;
    //const float plScale = MAX_DAC_VALUE * PL_SCALE;

    for (unsigned int i = 0; i < ADC_SAMPLE_COUNT; i++) {

        // Move from the DMA buffer to raw input buffers.  This separates the 
        // radio 0/1 streams and corrects the scaling.
        //
        // The 24-bit signed value is left-justified in the 32-bit word, 
        // so we need to shift right 8. Sign extension is automatic.
        // Range of 24 bits is -8,388,608 to 8,388,607.
        float r1_sample = (float)(adc_data[j] >> 8);
        float r0_sample = (float)(adc_data[j + 1] >> 8);     

        // Apply input soft gain immediately on receive
        r0_sample *= audioGain_r0;
        r1_sample *= audioGain_r1;

        // Hold history in circular buffer.
        in_history_r0[in_history_ptr] = r0_sample;
        in_history_r1[in_history_ptr] = r1_sample;
        in_history_ptr++;
        // Wrap
        if (in_history_ptr == IN_HISTORY_COUNT)
            in_history_ptr = 0;

        // ALL AUDIO GENERATION HAPPENS HERE

        float r0_out = 0;
        if (diagSynth0.isActive()) {
            r0_out = diagScaleLinear * diagSynth0.getSample();
        } 
        else {
            if (plSynth0.isActive()) {
                r0_out += (MAX_DAC_VALUE * plGain_r0 * plSynth0.getSample());
            }
            if (toneSynth0.isActive()) {
                r0_out += (MAX_DAC_VALUE * toneGain_r0 * toneSynth0.getSample());
            } 
            if (audioSource0.getSource() == AudioSourceControl::Source::RADIO0) {
                r0_out += (r0_sample * audioSource0.getFade());
            } else if (audioSource0.getSource() == AudioSourceControl::Source::RADIO1) {
                r0_out += (r1_sample * audioSource0.getFade());
            }
        }

        float r1_out = 0;
        if (diagSynth1.isActive()) {
            r1_out = diagScaleLinear * diagSynth1.getSample();
        } 
        else {
            if (plSynth1.isActive()) {
                r1_out += (MAX_DAC_VALUE * plGain_r1 * plSynth1.getSample());
            }
            if (toneSynth1.isActive()) {
                r1_out += (MAX_DAC_VALUE * toneGain_r1 * toneSynth1.getSample());
            } 
            if (audioSource1.getSource() == AudioSourceControl::Source::RADIO0) {
                r1_out += (r0_sample * audioSource1.getFade());
            } else if (audioSource1.getSource() == AudioSourceControl::Source::RADIO1) {
                r1_out += (r1_sample * audioSource1.getFade());
            }
        }

        // Apply soft gain just before transmitting
        r0_out *= audioGain_t0;
        r1_out *= audioGain_t1;

        // Convert to 32-bit padded with zeros on the right, per the PCM5100 datasheet.
        dac_buffer[j] = ((int32_t)r1_out) << 8;
        dac_buffer[j + 1] = ((int32_t)r0_out) << 8;

        // Hold output history in a circular buffer.  Note that the resolution
        // of the value is reduced to 8-bits because we are only using the data 
        // for power/peak calculations.
        out_history_r0[out_history_ptr] = ((int32_t)r0_out) >> 16;
        out_history_r1[out_history_ptr] = ((int32_t)r1_out) >> 16;
        // Wrap
        if (++out_history_ptr == OUT_HISTORY_COUNT)
            out_history_ptr = 0;

        // This is the pointer into the DMA buffers
        j += 2;
    }
}

static void generate_silence() {
    for (unsigned int i = 0; i < DAC_BUFFER_SIZE; i += 2) {
        // (Left)
        dac_buffer_ping[i + 1] = 0;
        dac_buffer_pong[i + 1] = 0;
        // (Right)
        dac_buffer_ping[i] = 0;
        dac_buffer_pong[i] = 0;
    }
}

static void audio_setup() {

    // TODO: REVIEW WHETHER THIS IS NEEDED
    // Reset the CODEC
    gpio_put(adc_rst_pin, 0);
    sleep_ms(100);
    gpio_put(adc_rst_pin, 1);
    sleep_ms(100);

    // ===== I2S SCK PIO Setup ===============================================

    // Allocate state machine
    uint sck_sm = pio_claim_unused_sm(pio0, true);
    uint sck_sm_mask = 1 << sck_sm;

    // Load PIO program into the PIO
    uint sck_program_offset = pio_add_program(pio0, &i2s_sck_program);
  
    // Setup the function select for a GPIO to use output from the given PIO 
    // instance.
    // 
    // PIO appears as an alternate function in the GPIO muxing, just like an 
    // SPI or UART. This function configures that multiplexing to connect a 
    // given PIO instance to a GPIO. Note that this is not necessary for a 
    // state machine to be able to read the input value from a GPIO, but only 
    // for it to set the output value or output enable.
    pio_gpio_init(pio0, sck_pin);

    // NOTE: The xxx_get_default_config() function is generated by the PIO
    // assembler and defined inside of the generated .h file.
    pio_sm_config sck_sm_config = 
        i2s_sck_program_get_default_config(sck_program_offset);
    // Associate pin with state machine. 
    // Because we are using the "SET" command in the PIO program
    // (and not OUT or side-set) we use the set_set function here.
    sm_config_set_set_pins(&sck_sm_config, sck_pin, 1);
    // Initialize setting and direction of the pin before SM is enabled
    uint sck_pin_mask = 1 << sck_pin;
    pio_sm_set_pins_with_mask(pio0, sck_sm, 0, sck_pin_mask);
    pio_sm_set_pindirs_with_mask(pio0, sck_sm, sck_pin_mask, sck_pin_mask);
    // Hook it all together.  (But this does not enable the SM!)
    pio_sm_init(pio0, sck_sm, sck_program_offset, &sck_sm_config);

    // Adjust state-machine clock divisor.  Remember that we need
    // the state machine to run at 2x SCK speed since it takes two 
    // instructions to acheive one clock transition on the pin.
    //
    // NOTE: The clock divisor is in 16:8 format
    //
    // d d d d d d d d d d d d d d d d . f f f f f f f f
    //            Integer Part         |  Fraction Part

    //unsigned int sck_sm_clock_d = 3;
    //unsigned int sck_sm_clock_f = 132;
    // Sanity check:
    // 2 * (3 + (132/256)) = 7.03125
    // 7.03125 * 48,000 * 384 = 129,600,000

    // CHANGED AUDIO RATE ON 22-JUNE-2025
    unsigned int sck_sm_clock_d = 5;
    unsigned int sck_sm_clock_f = 70;
    // Sanity check:
    // 2 * (5 + (70/256)) = 10.546875
    // 10.546875 * 32,000 * 384 = 129,600,000
    pio_sm_set_clkdiv_int_frac(pio0, sck_sm, sck_sm_clock_d, sck_sm_clock_f);

    // Final enable of the SCK state machine
    pio_enable_sm_mask_in_sync(pio0, sck_sm_mask);

    // Now issue a reset of the CODEC
    // Per datasheet page 18: "Because the system clock is used as a clock signal
    // for the reset circuit, the system clock must be supplied as soon as the 
    // power is supplied ..."
    //
    sleep_ms(100);
    gpio_put(adc_rst_pin, 0);
    sleep_ms(5);
    gpio_put(adc_rst_pin, 1);

    // Per PCM1804 datasheet page 18: 
    //
    // "The digital output is valid after the reset state is released and the 
    // time of 1116/fs has passed."
    // 
    // Assuming fs = 40,690 hz, then we must wait at least 27ms after reset!

    sleep_ms(50);

    // ===== I2S LRCK, BCK, DIN Setup (ADC) =====================================

    // Allocate state machine
    uint din_sm = pio_claim_unused_sm(pio0, true);
    uint din_sm_mask = 1 << din_sm;
    
    // Load master ADC program into the PIO
    uint din_program_offset = pio_add_program(pio0, &i2s_din_master_program);
  
    // Setup the function select for a GPIO to use from the given PIO 
    // instance.
    // DIN
    pio_gpio_init(pio0, adc_din_pin);
    gpio_set_pulls(adc_din_pin, false, false);
    gpio_set_dir(adc_din_pin, GPIO_IN);
    // BCK
    pio_gpio_init(pio0, adc_din_pin + 1);
    gpio_set_dir(adc_din_pin + 1, GPIO_OUT);
    // LRCK
    pio_gpio_init(pio0, adc_din_pin + 2);
    gpio_set_dir(adc_din_pin + 2, GPIO_OUT);

    // NOTE: The xxx_get_default_config() function is generated by the PIO
    // assembler and defined inside of the generated .h file.
    pio_sm_config din_sm_config = 
        i2s_din_master_program_get_default_config(din_program_offset);
    // Associate the input pin with state machine.  This will be 
    // relevant to the DIN pin for IN instructions.
    sm_config_set_in_pins(&din_sm_config, adc_din_pin);
    // Set the "side set pins" for the state machine. 
    // These are BCLK and LRCLK
    sm_config_set_sideset_pins(&din_sm_config, adc_din_pin + 1);
    // Configure the IN shift behavior.
    // Parameter 0: "false" means shift ISR to left on input.
    // Parameter 1: "true" means autopush is enabled.
    // Parameter 2: "32" means threshold (in bits) before auto/conditional 
    //              push to the ISR.
    sm_config_set_in_shift(&din_sm_config, false, true, 32);
    // Merge the FIFOs since we are only doing RX.  This gives us 
    // 8 words of buffer instead of the usual 4.
    sm_config_set_fifo_join(&din_sm_config, PIO_FIFO_JOIN_RX);

    // Initialize the direction of the pins before SM is enabled
    // There are three pins in the mask here. 
    uint din_pins_mask = 0b111 << adc_din_pin;
    // DIN: The "0" means input, "1" means output
    uint din_pindirs   = 0b110 << adc_din_pin;
    pio_sm_set_pindirs_with_mask(pio0, din_sm, din_pindirs, din_pins_mask);
    // Start with the two clocks in 1 state.
    uint din_pinvals   = 0b110 << adc_din_pin;
    pio_sm_set_pins_with_mask(pio0, din_sm, din_pinvals, din_pins_mask);

    // Hook it all together.  (But this does not enable the SM!)
    pio_sm_init(pio0, din_sm, din_program_offset, &din_sm_config);
          
    // Adjust state-machine clock divisor.  
    // NOTE: The clock divisor is in 16:8 format
    //
    // d d d d d d d d d d d d d d d d . f f f f f f f f
    //            Integer Part         |  Fraction Part
    // 
    // CHANGED AUDIO RATE ON 22-JUNE-2025
    //pio_sm_set_clkdiv_int_frac(pio0, din_sm, 21, 24);
    pio_sm_set_clkdiv_int_frac(pio0, din_sm, 31, 164);
    
    // ----- ADC DMA setup ---------------------------------------

    // The control channel will read between these two addresses,
    // telling the data channel to write to them alternately (i.e.
    // double-buffer).
    adc_addr_buffer[0] = adc_buffer;
    adc_addr_buffer[1] = &(adc_buffer[ADC_BUFFER_SIZE]);
    
    dma_ch_in_ctrl = dma_claim_unused_channel(true);
    dma_ch_in_data = dma_claim_unused_channel(true);

    // Setup the control channel. This channel is only needed to 
    // support the double-buffering behavior. A write by the control
    // channel will trigger the data channel to wake up and 
    // start to move data out of the PIO RX FIFO.
    dma_channel_config cfg = dma_channel_get_default_config(dma_ch_in_ctrl);
    // The control channel needs to step across the addresses of 
    // the various buffers.
    channel_config_set_read_increment(&cfg, true);
    // But always writing into the same location
    channel_config_set_write_increment(&cfg, false);
    // Sanity check before we start making assumptions about the
    // transfer size.
    assert(sizeof(uint32_t*) == 4);
    // Configure how many bits are involved in the address rotation.
    // 3 bits are used because we are wrapping through a total of 8 
    // bytes (two 4-byte addresses).  
    // The "false" means the read side.
    channel_config_set_ring(&cfg, false, 3);
    // Each address is 32-bits, so that's what we need to transfer 
    // each time.
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    // Program the DMA channel
    dma_channel_configure(dma_ch_in_ctrl, &cfg, 
        // Here is where we write to (the data channel)
        // NOTE: dma_hw is a global variable from the PICO SDK
        // Since we are writing to write_addr_trig, the result of 
        // the control channel write will be to start the data
        // channel.
        &dma_hw->ch[dma_ch_in_data].al2_write_addr_trig,
        // Here is where we start to read from (the address 
        // buffer area).
        adc_addr_buffer, 
        // TRANS_COUNT: Number of transfers to perform before stopping.
        // This count will be reset to the original value (1) every 
        // time the channel is started.
        1, 
        // false means don't start yet
        false);

    // Setup the data channel.
    cfg = dma_channel_get_default_config(dma_ch_in_data);
    // No increment required because we are always reading from the 
    // PIO RX FIFO every time.
    channel_config_set_read_increment(&cfg, false);
    // We need to increment the write to move across the buffer
    channel_config_set_write_increment(&cfg, true);
    // Set size of each transfer (one audio word)
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    // We trigger the control channel once the data transfer is done
    channel_config_set_chain_to(&cfg, dma_ch_in_ctrl);
    // Attach the DMA channel to the RX DREQ of the PIO state machine. 
    // The "false" below indicates RX.
    // This is the "magic" that connects the PIO SM to the DMA.
    channel_config_set_dreq(&cfg, pio_get_dreq(pio0, din_sm, false));
    // Program the DMA channel
    dma_channel_configure(dma_ch_in_data, &cfg,
        // Initial write address
        // 0 means that the target will be set by the control channel
        0, 
        // Initial Read address
        // The memory-mapped location of the RX FIFO of the PIO state
        // machine used for receiving data
        // This is the "magic" that connects the PIO SM to the DMA.
        &(pio0->rxf[din_sm]),
        // Number of transfers (each is 32 bits)
        ADC_BUFFER_SIZE,
        // Don't start yet
        false);

    // Enable interrupt when DMA data transfer completes via
    // the DMA_IRQ0.
    dma_channel_set_irq0_enabled(dma_ch_in_data, true);

    // ===== I2S DOUT/BCK/LRCK PIO Setup (To DAC) ==============================

    // Allocate state machine
    uint dout_sm = pio_claim_unused_sm(pio0, true);
    uint dout_sm_mask = 1 << dout_sm;

    // Load master ADC program into the PIO
    uint dout_program_offset = pio_add_program(pio0, &i2s_dout_master_program);
  
    // Setup the function select for a GPIO to use from the given PIO 
    // instance: DOUT
    pio_gpio_init(pio0, dac_dout_pin);
    gpio_set_dir(dac_dout_pin, GPIO_OUT);
    // BCK
    pio_gpio_init(pio0, dac_dout_pin + 1);
    gpio_set_dir(dac_dout_pin + 1, GPIO_OUT);
    // LRCK
    pio_gpio_init(pio0, dac_dout_pin + 2);
    gpio_set_dir(dac_dout_pin + 2, GPIO_OUT);

    // NOTE: The xxx_get_default_config() function is generated by the PIO
    // assembler and defined inside of the generated .h file.
    pio_sm_config dout_sm_config = 
        i2s_dout_master_program_get_default_config(dout_program_offset);
    // Associate the input pin with state machine.  This will be 
    // relevant to the DOUT pin for OUT instructions.
    sm_config_set_out_pins(&dout_sm_config, dac_dout_pin, 1);
    // Set the "side set pins" for the state machine. 
    // These are BCLK and LRCLK
    sm_config_set_sideset_pins(&dout_sm_config, dac_dout_pin + 1);
    // Configure the OUT shift behavior.
    // Parameter 0: "false" means shift OSR to left on input.
    // Parameter 1: "true" means autopull is enabled.
    // Parameter 2: "32" means threshold (in bits) before auto/conditional 
    //              pull to the OSR.
    sm_config_set_out_shift(&dout_sm_config, false, true, 32);
    // Merge the FIFOs since we are only doing TX.  This gives us 
    // 8 words of buffer instead of the usual 4.
    sm_config_set_fifo_join(&dout_sm_config, PIO_FIFO_JOIN_TX);

    // Initialize the direction of the pins before SM is enabled
    uint dout_pins_mask = 0b111 << dac_dout_pin;
    // DIN: The "0" means input, "1" means output
    uint dout_pindirs   = 0b111 << dac_dout_pin;
    pio_sm_set_pindirs_with_mask(pio0, dout_sm, dout_pindirs, dout_pins_mask);
    // Start with the two clocks in 1 state.
    uint dout_pinvals   = 0b110 << dac_dout_pin;
    pio_sm_set_pins_with_mask(pio0, dout_sm, dout_pinvals, dout_pins_mask);

    // Hook it all together.  (But this does not enable the SM!)
    pio_sm_init(pio0, dout_sm, dout_program_offset, &dout_sm_config);
          
    // Adjust state-machine clock divisor.  
    // NOTE: The clock divisor is in 16:8 format
    //
    // d d d d d d d d d d d d d d d d . f f f f f f f f
    //            Integer Part         |  Fraction Part
    // 
    // TODO: USE VARIABLES SHARED WITH DIN
    // CHANGED AUDIO RATE ON 22-JUNE-2025
    //pio_sm_set_clkdiv_int_frac(pio0, dout_sm, 21, 24);
    pio_sm_set_clkdiv_int_frac(pio0, dout_sm, 31, 164);
    
    dma_ch_out_data0 = dma_claim_unused_channel(true);
    dma_ch_out_data1 = dma_claim_unused_channel(true);

    // ----- DAC DMA channel 0 setup -------------------------------------

    cfg = dma_channel_get_default_config(dma_ch_out_data0);
    // We need to increment the read to move across the buffer
    channel_config_set_read_increment(&cfg, true);
    // Define wrap-around ring (read). Per Pico SDK docs:
    //
    // "For values n > 0, only the lower n bits of the address will change. This 
    // wraps the address on a (1 << n) byte boundary, facilitating access to 
    // naturally-aligned ring buffers. Ring sizes between 2 and 32768 bytes are 
    // possible (size_bits from 1 - 15)."
    //
    // WARNING: Make sure the buffer is sufficiently aligned for this to work!
    channel_config_set_ring(&cfg, false, DAC_SAMPLE_BYTES_LOG2);
    // No increment required because we are always writing to the 
    // PIO TX FIFO every time.
    channel_config_set_write_increment(&cfg, false);
    // Set size of each transfer (one audio word)
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    // Attach the DMA channel to the TX DREQ of the PIO state machine. 
    // The "true" below indicates TX.
    // This is the "magic" that connects the PIO SM to the DMA.
    channel_config_set_dreq(&cfg, pio_get_dreq(pio0, dout_sm, true));
    // We trigger the other data channel once the data transfer is done
    // in order to achieve the ping-pong effect.
    channel_config_set_chain_to(&cfg, dma_ch_out_data1);
    // Program the DMA channel
    dma_channel_configure(dma_ch_out_data0, &cfg,
        // Initial write address
        // The memory-mapped location of the RX FIFO of the PIO state
        // machine used for receiving data
        // This is the "magic" that connects the PIO SM to the DMA.
        &(pio0->txf[dout_sm]),
        // Initial Read address
        dac_buffer_ping, 
        // Number of transfers (each is 32 bits)
        DAC_BUFFER_SIZE,
        // Don't start yet
        false);
    // Enable interrupt when DMA data transfer completes
    dma_channel_set_irq0_enabled(dma_ch_out_data0, true);

    // ----- DAC DMA channel 1 setup -------------------------------------

    cfg = dma_channel_get_default_config(dma_ch_out_data1);
    // We need to increment the read to move across the buffer
    channel_config_set_read_increment(&cfg, true);
    // Define wrap-around ring (read)
    channel_config_set_ring(&cfg, false, DAC_SAMPLE_BYTES_LOG2);
    // No increment required because we are always writing to the 
    // PIO TX FIFO every time.
    channel_config_set_write_increment(&cfg, false);
    // Set size of each transfer (one audio word)
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    // Attach the DMA channel to the TX DREQ of the PIO state machine. 
    // The "true" below indicates TX.
    // This is the "magic" that connects the PIO SM to the DMA.
    channel_config_set_dreq(&cfg, pio_get_dreq(pio0, dout_sm, true));
    // We trigger the other data channel once the data transfer is done
    // in order to achieve the ping-pong effect.
    channel_config_set_chain_to(&cfg, dma_ch_out_data0);
    // Program the DMA channel
    dma_channel_configure(dma_ch_out_data1, &cfg,
        // Initial write address
        // The memory-mapped location of the RX FIFO of the PIO state
        // machine used for receiving data
        // This is the "magic" that connects the PIO SM to the DMA.
        &(pio0->txf[dout_sm]),
        // Initial Read address
        dac_buffer_pong, 
        // Number of transfers (each is 32 bits)
        DAC_BUFFER_SIZE,
        // Don't start yet
        false);
    // Enable interrupt when DMA data transfer completes
    dma_channel_set_irq0_enabled(dma_ch_out_data1, true);

    // ----- Final Enables ----------------------------------------------------

    // Bind to the interrupt handler 
    irq_set_exclusive_handler(DMA_IRQ_0, dma_irq_handler);
    // Enable DMA interrupts
    irq_set_enabled(DMA_IRQ_0, true);

    // Start ADC DMA action on the control side.  This will trigger
    // the ADC data DMA channel in turn.
    dma_channel_start(dma_ch_in_ctrl);
    // Start DAC DMA action immediately so the DAC FIFO is full
    // from the beginning.
    dma_channel_start(dma_ch_out_data0);

    // Stuff the TX FIFO to get going.  If the DAC state machine
    // gets started before there is anything in the FIFO we will
    // get stalled forever. 
    // TODO: EXPLAIN WHY
    uint fill_count = 0;
    while (pio0->fstat & 0x00040000 == 0) {
        fill_count++;
    }

    // Final enable of the three SMs to keep them in sync.
    pio_enable_sm_mask_in_sync(pio0, sck_sm_mask | din_sm_mask | dout_sm_mask);

    // Now issue a reset of the ADC
    //
    // Per datasheet page 18: "In slave mode, the system clock rate is automatically
    // detected."
    //
    // Per datasheet page 18: "The PCM1804 needs -RST=low when control pins
    // are changed or in slave mode when SCKI, LRCK, and BCK are changed."
    //
    // I believe these two statements imply that a -RST is needed after *all* of the
    // clocks are being driven at the target frequency.

    sleep_ms(100);
    gpio_put(adc_rst_pin, 0);
    sleep_ms(100);
    gpio_put(adc_rst_pin, 1);
}

static int calc_rms_db(float* data, unsigned int dataSize, float maxValue) {
    float rms = 0;
    for (unsigned int i = 0; i < dataSize; i++)
        rms += data[i] * data[i];
    rms = sqrt(rms / (float)dataSize);
    if (rms <= 0)
        return -99;
    return rms = 20.0 * log10(rms / maxValue);
}

static int calc_peak_db(float* data, unsigned int dataSize, float maxValue) {
    float peak = 0;
    for (unsigned int i = 0; i < dataSize; i++)
        if (data[i] > peak)
            peak = data[i];
    if (peak <= 0)
        return -99;
    return 20.0 * log10(peak / maxValue);
}

static int calc_rms_db(int8_t* data, unsigned int dataSize, float maxValue) {
    float rms = 0;
    for (unsigned int i = 0; i < dataSize; i++)
        rms += ((float)data[i] * (float)data[i]);
    rms = sqrt(rms / (float)dataSize);
    if (rms <= 0)
        return -99;
    return rms = 20.0 * log10(rms / maxValue);
}

static int calc_peak_db(int8_t* data, unsigned int dataSize, float maxValue) {
    float peak = 0;
    for (unsigned int i = 0; i < dataSize; i++)
        if ((float)data[i] > peak)
            peak = (float)data[i];
    if (peak <= 0)
        return -99;
    return 20.0 * log10(peak / maxValue);
}

static void print_vu_bar(int rms_db, int peak_db) {
    if (rms_db > -1 || peak_db > -1)
        printf("\033[31m");
    printf("[");
    int max_db_scale = 33;
    if (rms_db < -max_db_scale) 
        rms_db = -99;
    if (peak_db < -max_db_scale) 
        peak_db = -99;
    int rms_len = max_db_scale + (int)rms_db;
    if (rms_len < 0)
        rms_len = 0;
    int peak_len = max_db_scale + (int)peak_db;
    if (peak_len < 0)
        peak_len = 0;
    for (int i = 0; i < max_db_scale; i++) {
        if (i == peak_len) 
            printf("|");
        else if (i < rms_len)
            printf("=");
        else 
            printf(" ");
    }
    printf("] RMS %3ddB Peak %3ddB", (int)rms_db, (int)peak_db);
    printf("\033[0m");
}

static void render_status(const Rx& rx0, const Rx& rx1, const Tx& tx0, const Tx& tx1) {

    printf("\033[H");
    printf("W1TKZ Software Defined Repater Controller\n");
    printf("\n");

    printf("\033[30;47m");
    printf(" Radio 0 \n");
    printf("\033[0m");

    printf("RX0 COS  : ");
    if (rx0.isCOS()) {
        printf("\033[30;42m");
        printf("ACTIVE  ");
    } else {
        printf("\033[2m");
        printf("INACTIVE");
    }
    printf("\n");
    printf("\033[0m");

    printf("RX0 CTCSS: ");
    if (rx0.isCTCSS()) {
        printf("\033[30;42m");
        printf("ACTIVE  ");
    } else {
        printf("\033[2m");
        printf("INACTIVE");
    }
    printf("\n");
    printf("\033[0m");

    printf("TX0 PTT  : ");
    if (tx0.getPtt()) {
        printf("\033[30;42m");
        printf("ACTIVE  ");
    } else {
        printf("\033[2m");
        printf("INACTIVE");
    }
    printf("\n");
    printf("\033[0m");

    int rx_rms_r0_db = calc_rms_db(in_history_r0, IN_HISTORY_COUNT, MAX_DAC_VALUE);
    int rx_peak_r0_db = calc_peak_db(in_history_r0, IN_HISTORY_COUNT, MAX_DAC_VALUE);
    printf("RX0 LVL  : ");
    print_vu_bar(rx_rms_r0_db, rx_peak_r0_db);
    printf("\n");

    int tx_rms_r0_db = calc_rms_db(out_history_r0, OUT_HISTORY_COUNT, 128);
    int tx_peak_r0_db = calc_peak_db(out_history_r0, OUT_HISTORY_COUNT, 128);
    printf("TX0 LVL  : ");
    print_vu_bar(tx_rms_r0_db, tx_peak_r0_db);
    printf("\n");

    printf("\n");
                
    printf("\033[30;47m");
    printf(" Radio 1 \n");
    printf("\033[0m");

    printf("RX1 COS  : ");
    if (rx1.isCOS()) {
        printf("\033[30;42m");
        printf("ACTIVE  ");
    } else {
        printf("\033[2m");
        printf("INACTIVE");
    }
    printf("\n");
    printf("\033[0m");

    printf("RX1 CTCSS: ");
    if (rx1.isCTCSS()) {
        printf("\033[30;42m");
        printf("ACTIVE  ");
    } else {
        printf("\033[2m");
        printf("INACTIVE");
    }
    printf("\n");
    printf("\033[0m");

    printf("TX1 PTT  : ");
    if (tx1.getPtt()) {
        printf("\033[30;42m");
        printf("ACTIVE  ");
    } else {
        printf("\033[2m");
        printf("INACTIVE");
    }
    printf("\n");
    printf("\033[0m");

    int rx_rms_r1_db = calc_rms_db(in_history_r1, IN_HISTORY_COUNT, MAX_DAC_VALUE);
    int rx_peak_r1_db = calc_peak_db(in_history_r1, IN_HISTORY_COUNT, MAX_DAC_VALUE);
    printf("RX1 LVL  : ");
    print_vu_bar(rx_rms_r1_db, rx_peak_r1_db);
    printf("\n");

    int tx_rms_r1_db = calc_rms_db(out_history_r1, OUT_HISTORY_COUNT, 128);
    int tx_peak_r1_db = calc_peak_db(out_history_r1, OUT_HISTORY_COUNT, 128);
    printf("TX1 LVL  : ");
    print_vu_bar(tx_rms_r1_db, tx_peak_r1_db);
    printf("\n");

    printf("\n");
}

/**
 * @brief Transfers configuration parameters from the 
 * Config structure into the actual repeater controller.
 * This needs to happen once at started and then any 
 * time that the configuration is changed.
 */
static void transferConfig(const Config& config,
    Rx& rx0, Rx& rx1,
    Tx& tx0, Tx& tx1,
    TxControl& txc0, TxControl& txc1) 
{
    // General configuration
    txc0.setCall(config.general.callSign);
    txc1.setCall(config.general.callSign);
    txc0.setPass(config.general.pass);
    txc1.setPass(config.general.pass);
    txc0.setRepeatMode((TxControl::RepeatMode)config.general.repeatMode);
    txc1.setRepeatMode((TxControl::RepeatMode)config.general.repeatMode);

    // Receiver configuration
    rx0.setCosMode((Rx::CosMode)config.rx0.cosMode);
    rx0.setCosActiveTime(config.rx0.cosActiveTime);
    rx0.setCosInactiveTime(config.rx0.cosInactiveTime);
    rx0.setCosLevel(config.rx0.cosLevel);
    rx0.setToneMode((Rx::ToneMode)config.rx0.toneMode);
    rx0.setToneActiveTime(config.rx0.toneActiveTime);
    rx0.setToneInactiveTime(config.rx0.toneInactiveTime);
    rx0.setToneLevel(config.rx0.toneLevel);
    rx0.setToneFreq(config.rx0.toneFreq);
    rx0.setGain(config.rx0.gain);

    rx1.setCosMode((Rx::CosMode)config.rx1.cosMode);
    rx1.setCosActiveTime(config.rx1.cosActiveTime);
    rx1.setCosInactiveTime(config.rx1.cosInactiveTime);
    rx0.setCosLevel(config.rx1.cosLevel);
    rx1.setToneMode((Rx::ToneMode)config.rx1.toneMode);
    rx1.setToneActiveTime(config.rx1.toneActiveTime);
    rx1.setToneInactiveTime(config.rx1.toneInactiveTime);
    rx1.setToneLevel(config.rx1.toneLevel);
    rx1.setToneFreq(config.rx1.toneFreq);
    rx1.setGain(config.rx1.gain);

    // Transmitter configuration
    tx0.setToneMode((Tx::ToneMode)config.tx0.toneMode);
    tx0.setToneLevel(config.tx0.toneLevel);
    tx0.setToneFreq(config.tx0.toneFreq);

    tx1.setToneMode((Tx::ToneMode)config.tx1.toneMode);
    tx1.setToneLevel(config.tx1.toneLevel);
    tx1.setToneFreq(config.tx1.toneFreq);

    // Controller configuration
    txc0.setTimeoutTime(config.txc0.timeoutTime);
    txc0.setLockoutTime(config.txc0.lockoutTime);
    txc0.setHangTime(config.txc0.hangTime);
    txc0.setCtMode((TxControl::CtMode)config.txc0.ctMode);
    txc0.setCtLevel(config.txc0.ctLevel);
    txc0.setIdLevel(config.txc0.idLevel);

    txc1.setTimeoutTime(config.txc1.timeoutTime);
    txc1.setLockoutTime(config.txc1.lockoutTime);
    txc1.setHangTime(config.txc1.hangTime);
    txc1.setCtMode((TxControl::CtMode)config.txc1.ctMode);
    txc1.setCtLevel(config.txc1.ctLevel);
    txc1.setIdLevel(config.txc1.idLevel);
}

int main(int argc, const char** argv) {

    // Adjust system clock to more evenly divide the 
    // audio sampling frequency.
    unsigned long system_clock_khz = 129600;
    set_sys_clock_khz(system_clock_khz, true);

    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    
    gpio_init(R0_COS_PIN);
    gpio_set_dir(R0_COS_PIN, GPIO_IN);
    gpio_init(R0_CTCSS_PIN);
    gpio_set_dir(R0_CTCSS_PIN, GPIO_IN);
    gpio_init(R0_PTT_PIN);
    gpio_set_dir(R0_PTT_PIN, GPIO_OUT);
    gpio_put(R0_PTT_PIN, 0);
    gpio_init(R1_COS_PIN);
    gpio_set_dir(R1_COS_PIN, GPIO_IN);
    gpio_init(R1_CTCSS_PIN);
    gpio_set_dir(R1_CTCSS_PIN, GPIO_IN);
    gpio_init(R1_PTT_PIN);
    gpio_set_dir(R1_PTT_PIN, GPIO_OUT);
    gpio_put(R1_PTT_PIN, 0);

    gpio_init(adc_rst_pin);
    gpio_set_dir(adc_rst_pin, GPIO_OUT);
    gpio_put(adc_rst_pin, 1);

    // Startup ID
    sleep_ms(500);
    gpio_put(LED_PIN, 1);
    sleep_ms(500);
    gpio_put(LED_PIN, 0);

    Log log(&clock);
    uiMode = UIMode::UIMODE_LOG;
    log.setEnabled(true);

    log.info("W1TKZ Software Defined Repeater Controller");
    log.info("Copyright (C) 2025 Bruce MacKinnon KC1FSZ");
    log.info("Firmware R00236 2025-06-22");

    if (watchdog_enable_caused_reboot()) {
        log.info("Rebooted by watchdog timer");
    } else {
        log.info("Clean boot");
    }
    
    // ----- READ CONFIGURATION FROM FLASH ------------------------------------
    Config::loadConfig(&config);
    if (!config.isValid()) {
        log.info("Invalid config, setting factory default");
        Config::setFactoryDefaults(&config);
    }
    // This will force a load of the configuration data 
    // into the controller.
    configChanged = true;

    // Enable the watchdog, requiring the watchdog to be updated or the chip 
    // will reboot. The second arg is "pause on debug" which means 
    // the watchdog will pause when stepping through code
    watchdog_enable(WATCHDOG_INTERVAL_MS, 1);

    audio_setup();

    int strobe = 0;
    bool liveDisplay = false;
    
    clock.reset();
    //clock.setScale(10);

    // Display/diagnostic should happen once per second
    PicoPollTimer flashTimer;
    flashTimer.setIntervalUs(500 * 1000);

    StdTx tx0(clock, log, 0, R0_PTT_PIN, plSynth0);

    StdTx tx1(clock, log, 1, R1_PTT_PIN, plSynth1);

    StdRx rx0(clock, log, 0, R0_COS_PIN, R0_CTCSS_PIN, CourtesyToneGenerator::Type::FAST_UPCHIRP);

    StdRx rx1(clock, log, 1, R1_COS_PIN, R1_CTCSS_PIN, CourtesyToneGenerator::Type::FAST_DOWNCHIRP);

    TxControl txCtl0(clock, log, tx0, toneSynth0, audioSource0);
    TxControl txCtl1(clock, log, tx1, toneSynth1, audioSource1);

    txCtl0.setRx(0, &rx0);
    txCtl0.setRx(1, &rx1);
    txCtl1.setRx(0, &rx0);
    txCtl1.setRx(1, &rx1);  

    int i = 0;

    ShellOutput shellOutput;
    ShellCommand shellCommand(config);
    CommandShell shell;
    shell.setOutput(&shellOutput);
    shell.setSink(&shellCommand);

    // ===== Main Event Loop =================================================

    while (true) { 

        watchdog_update();

        // If anything in the configuration structure is 
        // changed then we force a transfer of all config
        // parameters from the config structure and into 
        // the controller objects.
        if (configChanged) {
            transferConfig(config, rx0, rx1, tx0, tx1, txCtl0, txCtl1);
            configChanged = false;
        }
      
        int c = getchar_timeout_us(0);
        bool flash = flashTimer.poll();

        if (uiMode == UIMode::UIMODE_LOG) {
            if (c == 's') {
                uiMode = UIMode::UIMODE_SHELL;
                log.setEnabled(false);
                shell.reset();
            } else if (c == 't') {
                // Clear off the status screen
                printf("\033[2J");
                // Hide cursor
                printf("\033[?25l");
                uiMode = UIMode::UIMODE_STATUS;
                log.setEnabled(false);

            } else if (c == 'i') {
                txCtl0.forceId();
                txCtl1.forceId();
            }
        }
        else if (uiMode == UIMode::UIMODE_SHELL) {
            if (c != 0) {
                shell.process(c);
            }
        }
        else if (uiMode == UIMode::UIMODE_STATUS) {
            // Do periodic display/diagnostic stuff
            if (flash)
                render_status(rx0, rx1, tx0, tx1);
            if (c == 'l') {
                // Clear off the status screen
                printf("\033[2J");
                // Show cursor
                printf("\033[?25h");
                uiMode = UIMode::UIMODE_LOG;
                log.setEnabled(true);
                log.info("Entered log mode");
            } 
            else if (c == 's') {
                // Clear off the status screen
                printf("\033[2J");
                // Show cursor
                printf("\033[?25h");
                uiMode = UIMode::UIMODE_SHELL;
                log.setEnabled(false);
                shell.reset();
            } 
            else if (c == 'i') {
                txCtl0.forceId();
                txCtl1.forceId();
            }
        }
        /*
        else if (c == 'd') {
            if (!diagOn) {
                diagOn = true;
                diagSynth0.setFreq(diagFreqHz);
                diagSynth1.setFreq(diagFreqHz);
                diagSynth0.setEnabled(true);
                diagSynth1.setEnabled(true);
                log.info("Diag tone enabled");
            }
            else {
                diagOn = false;
                diagSynth0.setEnabled(false);
                diagSynth1.setEnabled(false);
                log.info("Diag tone disabled");
            }
        }
        else if (c == '8') {
            diagFreqHz += 50;
            diagSynth0.setFreq(diagFreqHz);
            diagSynth1.setFreq(diagFreqHz);
            log.info("Diag tone f=%f", diagFreqHz);
        } 
        else if (c == '2') {
            if (diagFreqHz > 100)
                diagFreqHz -= 50;
            diagSynth0.setFreq(diagFreqHz);
            diagSynth1.setFreq(diagFreqHz);
            log.info("Diag tone f=%f", diagFreqHz);
        }
        else if (c == '9') {
            if (diagScaleDb < 0)
                diagScaleDb += 1.0;
            // Convert from dB to linear
            diagScaleLinear = pow(10, (diagScaleDb / 20)) * MAX_DAC_VALUE;
            log.info("Diag s=%f", diagScaleDb);
        }
        else if (c == '3') {
            diagScaleDb -= 1.0;
            // Convert from dB to linear
            diagScaleLinear = pow(10, (diagScaleDb / 20)) * MAX_DAC_VALUE;
            log.info("Diag s=%f", diagScaleDb);
        }
        else if (c == 'l') {
            if (liveDisplay) {
                liveDisplay = false;
                log.setEnabled(true);
                puts("\033[2J\033[?25h");
            }
            else {
                liveDisplay = true;
                log.setEnabled(false);
                puts("\033[2J\033[?25l");
            }
        }
        else if (c == 'r') {
            log.info("Reboot requested");
            // The watchdog will take over from here
            while (true);            
        }
        */

        // Run all components
        tx0.run();
        tx1.run();
        rx0.run();
        rx1.run();
        txCtl0.run();
        txCtl1.run();
        audioSource0.run();        
        audioSource1.run();
    }
}
