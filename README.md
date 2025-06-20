Overview
========

This project attempts to create a basic 
two-radio repeater controller typical of the kind used
at analog VHF/UHF repeater sites. Our goal is to implement
as much of the repeater functionality in software as 
possible. For this reason, we call this a "Software 
Defined Repeater Controller" (SDRC). 

The system was developed by Bruce MacKinnon (KC1FSZ) and
Dan Brown (W1DAN) of the Wellesley Amateur Radio Society (W1TKZ).

Key capabilities:

* Support for two receivers and two transmitters. Radios
can operate independently or can be linked to support
remote receiver or cross-band repeater systems.
* CTCSS (PL) tone encoding and decoding, with support 
for independent frequencies for each transmitter/receiver.
* CWID generation.
* Configurable hang time.
* Configurable courtesy tone generation.
* Timeout and lockout with configurable times.
* Optional digital voice ID and other prompts.
* Remote firmware update via LoRa connection.
* Hardware gain adjustments to calibrate dynamic range during initial installation.
* Soft RX/TX gain control adjustable remotely.
* "Soft console" via USB-connected computer computer with
serial terminal provides live display of the following
for each radio:
  - Carrier detect (COS) status
  - PL tone detect (CTSS) status
  - Push-to-talk (PTT) status
  - Receiver audio level RMS and peak

Other things to know:

* Runs on +12VDC power input.
* DB25 connection for radio interfaces.

A demonstration video of the current prototype 
[can be seen here](https://www.youtube.com/watch?v=HBwrpokd7FI).

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

Relevant Regs
=============

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
