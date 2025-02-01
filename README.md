Build/Debug Commands
====================

One-time setup of make process:

        git submodule update --init
        mkdir build
        cd build
        cmake -DPICO_BOARD=pico ..

Build:

        cd build
        make

Flash code to board:        

        ~/git/openocd/src/openocd -s ~/git/openocd/tcl -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "adapter speed 5000" -c "program main.elf verify reset exit"

REMOVED: -c "rp2040.dap.core1 cortex_m reset_config sysresetreq"

Pico Pins
=========

Pico Pinout Notes
=================

GP0  - (Reserved for UART0 TX)
GP1  - (Reserved for UART0 RX)
GP2  - I2C1 SDA 
GP3  - I2C1 SCL 
GP4  - Audio Select Radio 0 
GP5  - PTT Radio 0
GP6  - COS Radio 0
GP7   - Audio Select Radio 1
GP8  - PTT Radio 1
GP9  - COS Radio 1
GP10 - Tone output (PWM)
GP11
GP12
GP13
GP14
GP15
GP16 - 
GP17 - 

Tone Notes
==========

The tone output must be low-pass filtered.  See the [RP2040 hardware design guide](https://datasheets.raspberrypi.com/rp2040/hardware-design-with-rp2040.pdf) on page 24 for an example circuit.
