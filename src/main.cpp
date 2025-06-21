/**
 * Digital Repeater Controller
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
When targeting RP2040 (Pico 1):

Command used to load code onto the board: 
~/git/openocd/src/openocd -s ~/git/openocd/tcl -f interface/cmsis-dap.cfg -f target/rp040.cfg -c "adapter speed 5000" -c "rp2350.dap.core1 cortex_m reset_config sysresetreq" -c "program main.elf verify reset exit"
*/

#include <stdio.h>
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

#include "kc1fsz-tools/Log.h"
#include "kc1fsz-tools/rp2040/PicoPollTimer.h"
#include "kc1fsz-tools/rp2040/PicoPerfTimer.h"
#include "kc1fsz-tools/rp2040/PicoClock.h"

#include "test/TestTx.h"
#include "test/TestRx.h"
#include "hw/StdTx.h"
#include "hw/StdRx.h"
#include "AudioSourceControl.h"

#include "i2s.pio.h"
#include "TxControl.h"

using namespace kc1fsz;

// ===========================================================================
// AUDIO RELATED 
// ===========================================================================

// Pin to be allocated to I2S SCK (output to CODEC)
// GP10 is physical pin 14
uint sck_pin = 4;
// ADC pins
// Pin to be allocated to ~RST
uint adc_rst_pin = 5;
// Pin to be allocated to I2S DIN (input from)
uint adc_din_pin = 6;
// DAC pins
// Pin to be allocated to I2S DOUT 
uint dac_dout_pin = 9;

#define PI (3.1415926)
#define ADC_SAMPLE_COUNT (256)
#define ADC_SAMPLE_BYTES_LOG2 (11)
#define DAC_SAMPLE_BYTES_LOG2 (11)
#define FS_HZ (48000)
// Controls the maximum value that can be output from the DAC
// (plus or minus) before clipping happens somewhere in the audio 
// output path.
#define MAX_DAC_SCALE (8388608)

// Buffer used to drive the DAC via DMA.
// 2* for L and R
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

// Diagnostic counters
static volatile uint32_t dma_in_count = 0;
static volatile uint32_t dma_out_count = 0;

static uint dma_ch_in_ctrl = 0;
static uint dma_ch_in_data = 0;
static uint dma_ch_out_data0 = 0;
static uint dma_ch_out_data1 = 0;

// This flag is used to manage the alternating buffers. "ping open"
// indicates that the ping buffer was just written and should be sent
// out on the next opportunity. Otherwise, it's the pong buffer that
// was just written and is waiting to be sent.
static volatile bool dac_buffer_ping_open = false;

static void process_in_frame();

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

// Audio input processing buffers
// (Pulled into global space to enable introspection)
static float raw_in_r0[ADC_SAMPLE_COUNT];
static float raw_in_r1[ADC_SAMPLE_COUNT];

// Used for holding a down-sampled version of the audio for level analysis
static const unsigned int downSampleSize = 48;
static unsigned int downSamplePtr = 0;
static const unsigned int downSampleBufSize = 512;
static unsigned int downSampleBufPtr = 0;

static float adcSampleRmsAcc_r0 = 0;
static float adcSampleRmsAcc_r1 = 0;
static float adcSamplePeakAcc_r0 = 0;
static float adcSamplePeakAcc_r1 = 0;
static float adcSampleRmsBuf_r0[downSampleBufSize];
static float adcSampleRmsBuf_r1[downSampleBufSize];
static float adcSamplePeakBuf_r0[downSampleBufSize];
static float adcSamplePeakBuf_r1[downSampleBufSize];

static float dacSampleRmsAcc_r0 = 0;
static float dacSampleRmsAcc_r1 = 0;
static float dacSamplePeakAcc_r0 = 0;
static float dacSamplePeakAcc_r1 = 0;
static float dacSampleRmsBuf_r0[downSampleBufSize];
static float dacSampleRmsBuf_r1[downSampleBufSize];
static float dacSamplePeakBuf_r0[downSampleBufSize];
static float dacSamplePeakBuf_r1[downSampleBufSize];

// Objects used for tone generation (CW, courtesy, PL, etc.)
static ToneSynthesizer toneSynth0(FS_HZ, 5);
static ToneSynthesizer toneSynth1(FS_HZ, 5);
static ToneSynthesizer plSynth0(FS_HZ, 5);
static ToneSynthesizer plSynth1(FS_HZ, 5);
static AudioSourceControl audioSource0;
static AudioSourceControl audioSource1;

// A tone generator used for diagnostic and calibration
static ToneSynthesizer diagSynth0(FS_HZ, 5);
static ToneSynthesizer diagSynth1(FS_HZ, 5);
static float diagScaleDb = 0;
static float diagScaleLinear = 1.0;
static float diagFreqHz = 1000.0;

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

    // Write to the appropriate DAC buffer based on our current tracking of which 
    // is available for use.
    int32_t* dac_buffer;
    if (dac_buffer_ping_open) 
        dac_buffer = (int32_t*)dac_buffer_ping;
    else
        dac_buffer = (int32_t*)dac_buffer_pong;

    // Move from the DMA buffer to raw input buffers.  This separates the 
    // radio 0/1 streams and corrects the scaling.
    unsigned int j = 0;
    // Down -10dB
    const float toneScale = MAX_DAC_SCALE * 0.32;
    // Down another -12dB
    const float plScale = toneScale * 0.25;
    // Audio is allowed to pass unscaled
    const float audioScale = 1.0;

    for (unsigned int i = 0; i < ADC_SAMPLE_COUNT; i++) {

        // The 24-bit signed value is left-justified in the 32-bit word, 
        // so we need to shift right 8. Sign extension is automatic.
        // Range of 24 bits is -8,388,608 to 8,388,607.
        float r1_sample = (float)(adc_data[j] >> 8);
        float r0_sample = (float)(adc_data[j + 1] >> 8);     

        raw_in_r0[i] = r0_sample;
        raw_in_r1[i] = r1_sample;

        // Create down-sampled history 
        adcSampleRmsAcc_r0 += r0_sample * r0_sample;
        adcSampleRmsAcc_r1 += r1_sample * r1_sample;
        if (r0_sample > adcSamplePeakAcc_r0) 
            adcSamplePeakAcc_r0 = r0_sample;
        if (r1_sample > adcSamplePeakAcc_r1) 
            adcSamplePeakAcc_r1 = r1_sample;

        // -----------------------------------------------------------------------
        // AUDIO PROCESSING HAPPENS HERE

        float r0_out = 0;
        if (diagSynth0.isActive()) {
            r0_out = diagScaleLinear * diagSynth0.getSample();
        } 
        else {
            // Blend the various audio sources
            r0_out = (plScale * plSynth0.getSample());
            // Bring in tone if it is running
            if (toneSynth0.isActive()) {
                r0_out += toneScale * toneSynth0.getSample();
            } 
            // If there is no tone then bring in the audio
            else {
                if (audioSource0.getSource() == AudioSourceControl::Source::RADIO0) {
                    r0_out += audioScale * r0_sample;                
                } else if (audioSource0.getSource() == AudioSourceControl::Source::RADIO1) {
                    r0_out += audioScale * r1_sample;
                }
            }
        }

        float r1_out = 0;
        if (diagSynth1.isActive()) {
            r1_out = diagScaleLinear * diagSynth1.getSample();
        } 
        else {
            // Blend the various audio sources
            r1_out = (plScale * plSynth1.getSample());
            // Bring in tone if it is running
            if (toneSynth1.isActive()) {
                r1_out += toneScale * toneSynth1.getSample();
            } 
            // If there is no tone then bring in the audio
            else {
                if (audioSource1.getSource() == AudioSourceControl::Source::RADIO0) {
                    r1_out += audioScale * r0_sample;
                } else if (audioSource1.getSource() == AudioSourceControl::Source::RADIO1) {
                    r1_out += audioScale * r1_sample;
                }
            }
        }
        // -----------------------------------------------------------------------
        
        // Convert to 32-bit padded with zeros on the left
        // Radio 1
        dac_buffer[j] = ((int32_t)r1_out) << 8;
        // Radio 0
        dac_buffer[j + 1] = ((int32_t)r0_out) << 8;

        // Create down-sampled history 
        dacSampleRmsAcc_r0 += r0_out * r0_out;
        dacSampleRmsAcc_r1 += r1_out * r1_out;
        //r0_out = max(r0_out, dacSamplePeakAcc_r0);
        //r1_out = max(r1_out, dacSamplePeakAcc_r1);
        if (r0_out > dacSamplePeakAcc_r0) 
            dacSamplePeakAcc_r0 = r0_out;
        if (r1_out > dacSamplePeakAcc_r1) 
            dacSamplePeakAcc_r1 = r1_out;

        // Deal with down-sampled power data
        downSamplePtr++;
        if (downSamplePtr >= downSampleSize) {

            downSamplePtr = 0;

            adcSampleRmsBuf_r0[downSampleBufPtr] = adcSampleRmsAcc_r0;
            adcSampleRmsBuf_r1[downSampleBufPtr] = adcSampleRmsAcc_r1;
            adcSamplePeakBuf_r0[downSampleBufPtr] = adcSamplePeakAcc_r0;
            adcSamplePeakBuf_r1[downSampleBufPtr] = adcSamplePeakAcc_r1;
            adcSampleRmsAcc_r0 = 0;
            adcSampleRmsAcc_r1 = 0;
            adcSamplePeakAcc_r0 = 0;
            adcSamplePeakAcc_r1 = 0;

            dacSampleRmsBuf_r0[downSampleBufPtr] = dacSampleRmsAcc_r0;
            dacSampleRmsBuf_r1[downSampleBufPtr] = dacSampleRmsAcc_r1;
            dacSamplePeakBuf_r0[downSampleBufPtr] = dacSamplePeakAcc_r0;
            dacSamplePeakBuf_r1[downSampleBufPtr] = dacSamplePeakAcc_r1;
            dacSampleRmsAcc_r0 = 0;
            dacSampleRmsAcc_r1 = 0;
            dacSamplePeakAcc_r0 = 0;
            dacSamplePeakAcc_r1 = 0;

            // Increment ptr and wrap
            downSampleBufPtr++;
            if (downSampleBufPtr >= downSampleBufSize)
                downSampleBufPtr = 0;
        }

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
    unsigned int sck_sm_clock_d = 3;
    unsigned int sck_sm_clock_f = 132;
    // Sanity check:
    // 2 * (3 + (132/256)) = 7.03125
    // 7.03125 * 48,000 * 384 = 129,600,000
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
    pio_sm_set_clkdiv_int_frac(pio0, din_sm, 21, 24);
    
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
    pio_sm_set_clkdiv_int_frac(pio0, dout_sm, 21, 24);
    
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

#define LED_PIN (PICO_DEFAULT_LED_PIN)
#define R0_COS_PIN (14)
#define R0_CTCSS_PIN (13)
#define R0_PTT_PIN (12)
#define R1_COS_PIN (17)
#define R1_CTCSS_PIN (16)
#define R1_PTT_PIN (15)

static int calc_rms(float* data, unsigned int dataSize, unsigned int sampleSize) {
    float rms = 0;
    for (unsigned int i = 0; i < dataSize; i++) {
        float p = sqrt(data[i] / (float)sampleSize);
        rms += p * p;
    }
    rms = sqrt(rms / (float)dataSize);
    float max = 8000000;
    return rms = 10.0 * log10(rms / max);
}

static int calc_peak(float* data, unsigned int dataSize) {
    float peak = 0;
    for (unsigned int i = 0; i < dataSize; i++) {
        if (data[i] > peak)
            peak = data[i];
    }
    float max = 7500000;
    return 20.0 * log10(peak / max);
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

    //printf("\033[2J");
    printf("Software Defined Repeater Controller\nCopyright (C) 2025 Bruce MacKinnon KC1FSZ\n");
    printf("Firmware R00234 2025-06-20\n");

    // ===== AUDIO SETUP 

    audio_setup();

    int strobe = 0;
    bool liveDisplay = false;
    
    PicoClock clock;
    clock.reset();
    //clock.setScale(10);

    Log log(&clock);

    // Display/diagnostic should happen once per second
    PicoPollTimer flashTimer;
    flashTimer.setIntervalUs(500 * 1000);

    StdTx tx0(clock, log, 0, R0_PTT_PIN, plSynth0);
    //tx0.setToneMode(StdTx::ToneMode::SOFT);
    tx0.setTone(1230);

    StdTx tx1(clock, log, 1, R1_PTT_PIN, plSynth1);
    //tx1.setToneMode(StdTx::ToneMode::SOFT);
    tx1.setTone(885);

    StdRx rx0(clock, log, 0, R0_COS_PIN, R0_CTCSS_PIN, CourtesyToneGenerator::Type::FAST_UPCHIRP);
    rx0.setCosMode(StdRx::CosMode::COS_EXT_HIGH);
    rx0.setToneMode(StdRx::ToneMode::TONE_EXT_HIGH);

    StdRx rx1(clock, log, 1, R1_COS_PIN, R1_CTCSS_PIN, CourtesyToneGenerator::Type::FAST_DOWNCHIRP);
    rx1.setCosMode(StdRx::CosMode::COS_EXT_HIGH);
    rx1.setToneMode(StdRx::ToneMode::TONE_EXT_HIGH);

    TxControl txCtl0(clock, log, tx0, toneSynth0, audioSource0);
    TxControl txCtl1(clock, log, tx1, toneSynth1, audioSource1);

    txCtl0.setRx(0, &rx0);
    txCtl0.setRx(1, &rx1);
    txCtl1.setRx(0, &rx0);
    txCtl1.setRx(1, &rx1);

    // ===== Main Event Loop =================================================

    int i = 0;

    while (true) { 
        
        int c = getchar_timeout_us(0);
        if (c == 's') {
            printf("Status:\n");
            printf("  RX0 isActve %d\n", (int)rx0.isActive());
            printf("  RX1 isActve %d\n", (int)rx1.isActive());
        }
        else if (c == ' ') {
            txCtl0.forceId();
            txCtl1.forceId();
        }
        // Radio 0 input display
        else if (c == '1') {
            float hold[ADC_SAMPLE_COUNT];
            for (unsigned int i = 0; i < ADC_SAMPLE_COUNT; i++) 
                hold[i] = raw_in_r1[i];
            printf("\n");
            for (unsigned int i = 0; i < ADC_SAMPLE_COUNT; i++) {
                printf("%f\n", hold[i]);
            }
            printf("\n");
        }
        else if (c == 'd') {
            diagFreqHz = 700;
            diagScaleDb = -10.0;
            // Convert from dB to linear
            diagScaleLinear = pow(10, (diagScaleDb / 20)) * MAX_DAC_SCALE;
            diagSynth0.setFreq(diagFreqHz);
            diagSynth1.setFreq(diagFreqHz);
            diagSynth0.setEnabled(true);
            diagSynth1.setEnabled(true);
            log.info("Diag tone enabled");
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
            diagScaleLinear = pow(10, (diagScaleDb / 20)) * MAX_DAC_SCALE;
            printf("Diag s=%f\n", diagScaleDb);
        }
        else if (c == '3') {
            diagScaleDb -= 1.0;
            // Convert from dB to linear
            diagScaleLinear = pow(10, (diagScaleDb / 20)) * MAX_DAC_SCALE;
            printf("Diag s=%f\n", diagScaleDb);
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

        // Do periodic display/diagnostic stuff
        if (flashTimer.poll()) {
            //printf("Flash %d\n", i);
            i++;
            //printf("DMA out %u\n", dma_out_count);
            //printf("ADC in %u\n", dma_in_count);

            if (liveDisplay) {
                printf("\033[H");
                printf("W1TKZ Software Defined Repater Controller\n");
                printf("\n");

                printf("Radio 0\n");

                printf("RX0 COS : ");
                if (rx0.isActive()) {
                    printf("\033[30;42m");
                    printf("ACTIVE  ");
                }
                else {
                    printf("\033[2m");
                    printf("INACTIVE");
                }
                printf("\n");
                printf("\033[0m");

                printf("TX0 PTT : ");
                if (tx0.getPtt()) {
                    printf("\033[30;42m");
                    printf("ACTIVE  ");
                }
                else {
                    printf("\033[2m");
                    printf("INACTIVE");
                }
                printf("\n");
                printf("\033[0m");

                int rx_rms_r0_db = calc_rms(adcSampleRmsBuf_r0, downSampleBufSize,
                    downSampleSize);
                int rx_peak_r0_db = calc_peak(adcSamplePeakBuf_r0, downSampleBufSize);
                printf("RX0 LVL : ");
                print_vu_bar(rx_rms_r0_db, rx_peak_r0_db);
                printf("\n");

                int tx_rms_r0_db = calc_rms(dacSampleRmsBuf_r0, downSampleBufSize,
                    downSampleSize);
                int tx_peak_r0_db = calc_peak(dacSamplePeakBuf_r0, downSampleBufSize);
                printf("TX0 LVL : ");
                print_vu_bar(tx_rms_r0_db, tx_peak_r0_db);
                printf("\n");

                printf("\n");
                
                printf("Radio 1\n");

                printf("RX1 COS : ");
                if (rx1.isActive()) {
                    printf("\033[30;42m");
                    printf("ACTIVE  ");
                }
                else {
                    printf("\033[2m");
                    printf("INACTIVE");
                }
                printf("\n");
                printf("\033[0m");

                printf("TX1 PTT : ");
                if (tx1.getPtt()) {
                    printf("\033[30;42m");
                    printf("ACTIVE  ");
                }
                else {
                    printf("\033[2m");
                    printf("INACTIVE");
                }
                printf("\n");
                printf("\033[0m");

                int rx_rms_r1_db = calc_rms(adcSampleRmsBuf_r1, downSampleBufSize,
                    downSampleSize);
                int rx_peak_r1_db = calc_peak(adcSamplePeakBuf_r1, downSampleBufSize);
                printf("RX1 LVL : ");
                print_vu_bar(rx_rms_r1_db, rx_peak_r1_db);
                printf("\n");

                int tx_rms_r1_db = calc_rms(dacSampleRmsBuf_r1, downSampleBufSize,
                    downSampleSize);
                int tx_peak_r1_db = calc_peak(dacSamplePeakBuf_r1, downSampleBufSize);
                printf("TX1 LVL : ");
                print_vu_bar(tx_rms_r1_db, tx_peak_r1_db);
                printf("\n");

                printf("\n");
            }
        }

        // Run all components
        tx0.run();
        tx1.run();
        rx0.run();
        rx1.run();
        txCtl0.run();
        txCtl1.run();
    }
}
