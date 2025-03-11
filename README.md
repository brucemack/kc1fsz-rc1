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

Pulling latest submodules:

        git submodule update --recursive --remote

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

Wiring Notes
============

Control Module 
* J2:1 - +3.3V
* J2:2 - Pico GP6 (COS Radio 0)
* J2:3 - Pico GP9 (COS Radio 1)
* J2:4 - Pico GP5 (PTT Radio 0)
* J2:5 - Pico GP8 (PTT Radio 1)
* J2:6 - GND
* J1:1 - COS Radio 0 - (Optocoupler cathode)
* J1:2 - COS Radio 0 + (Through 220R and optocoupler anode)
* J1:5 - PTT Radio 0 + (Optocoupler collector)
* J1:6 - PTT Radio 0 - (Optocoupler emitter)

Audio Module
* J4:1 - Pico GP4 (Audio Select Radio 0)
* J4:2 - Pico GP7 (Audio Select Radio 1)
* J1:1 - +5V
* J1:2 - GND

Tone Notes
==========

The tone output must be low-pass filtered.  See the [RP2040 hardware design guide](https://datasheets.raspberrypi.com/rp2040/hardware-design-with-rp2040.pdf) on page 24 for an example circuit.
