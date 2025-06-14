Features
========

* CTCSS Decode
* CWID
* Courtesy Tone (1480/1760 20ms)
* Noise Reduction
* AGC/Input Balance
* DTMF Commands
   - Open/close VHF-UHF link

Parameters
==========

* CTCSS Decoder
   - Minimum time for valid CTCSS
   - CTCSS debounce interval (i.e. max drop-out)
* CWID
   - Time between IDs
   - Max interruption interval (i.e. how long before CWID steps on normal use)
* Misc
   - Transmit timeout
   - Lockout time after transmit timeout
   - Hang time (how long TX stays keyed after input drops)
   - COS debounce interval (i.e. max drop-out of COS)

CTCSS Tone Notes
================

* CTCSS/PL frequency on receive and transmit
* 


Relevant Rules
==============

FCC Section 97.119 Station identification

(a) Each amateur station, except a space station or telecommand station, must transmit its assigned call sign on its transmitting channel at the end of each communication, and at least every 10 minutes during a communication, for the purpose of clearly making the source of the transmissions from the station known to those receiving the transmissions. No station may transmit unidentified communications or signals, or transmit as the station call sign, any call sign not authorized to the station.

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

        ~/git/openocd/src/openocd -s ~/git/openocd/tcl -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "adapter speed 5000" -c "rp2350.dap.core1 cortex_m reset_config sysresetreq" -c "program main.elf verify reset exit"

Serial Console
==============

Connect serial-USB module to GPIO0/GPIO1 pins and use this command:

        minicom -b 115200 -o -D /dev/ttyUSB0

Wiring Notes (Revision B)
=========================

Pico 2 Pinout Notes
```
GP0  - (Reserved for UART0 TX)
GP1  - (Reserved for UART0 RX)

GP2  - I2C SDA
GP3  - I2C SCL
GP4  - I2S SCK out to PCM1804 ADC and PCM5100 DAC
GP5  - RST out to PCM1804 ADC

GP6  - I2S DIN in from PC1804 ADC
GP7  - I2S BCK out to PC1804 ADC
GP8  - I2S LRCK out to PC1804 ADC
GP9  - I2S DOUT out to PCM5100 DAC

GP10 - I2S BCK out to PCM5100 DAC
GP11 - I2S LRCK out to PCM5100 DAC
GP12 - Radio 0 PTT Out
GP13 - Radio 0 CTCSS In
GP14 - Radio 0 COS In
GP15 - Radio 1 PTT Out
GP16 - Radio 1 CTCSS In
GP17 - Radio 1 COS In

GP18 - LED0 Out
GP19 - X
GP20 - X
GP21 - X

GP22 - X
GP26 - X
GP27 - X
GP28 - X
```

Wiring Notes (Revision A)
=========================

Pico Module:
* GP0  - (Reserved for UART0 TX)
* GP1  - (Reserved for UART0 RX)
* GP2  - I2C1 SDA 
* GP3  - I2C1 SCL 
* GP4  - Audio Select Radio 0 
* GP5  - PTT Radio 0
* GP6  - COS Radio 0
* GP7  - Audio Select Radio 1
* GP8  - PTT Radio 1
* GP9  - COS Radio 1
* GP10 - Tone output (PWM)
* GP11
* GP12
* GP13
* GP14
* GP15
* GP16 - 
* GP17 - 

Control Module: 
* J2:1 -> +3.3V
* J2:2 -> Pico GP6 (COS Radio 0)
* J2:3 -> Pico GP9 (COS Radio 1)
* J2:4 -> Pico GP5 (PTT Radio 0)
* J2:5 -> Pico GP8 (PTT Radio 1)
* J2:6 -> GND
* J1:1 -> COS- Radio 0 (Optocoupler cathode)
* J1:2 -> COS+ Radio 0 (Through 220R and optocoupler anode)
* J1:5 -> PTT+ Radio 0 (Optocoupler collector)
* J1:6 -> PTT- Radio 0 (Optocoupler emitter)

Audio Module:
* J4:1 -> Pico GP4 (Audio Select Radio 0)
* J4:2 -> Pico GP7 (Audio Select Radio 1)
* J1:1 -> +5V
* J1:2 -> GND

GPIO Tone Generation Notes
==========================

The tone output must be low-pass filtered.  See the [RP2040 hardware design guide](https://datasheets.raspberrypi.com/rp2040/hardware-design-with-rp2040.pdf) on page 24 for an example circuit.
