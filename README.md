Overview
========

This project attempts to create a basic two-radio repeater controller typical used
at analog FM VHF/UHF repeater sites. Our goal is to implement
as much of the repeater functionality in software as 
possible. For this reason, we call this the "Software 
Defined Repeater Controller" (SDRC) project. This Github
repo contains the source code for the controller and the 
KiCad designs for the hardware.

One of the advantages of an all-digital architecture is that integration
with other digital-voice technologies should be seamless. Please
see the related project called [MicroLink](https://github.com/brucemack/microlink) which
provides a compact EchoLink implementation. Soon the SDRC and MicroLink
systems will be integrated. Reverse-engineering of the D-Star/DMR protocols
will also enable direct integration of those modes at some point. 

Much of the hardware traditionally used in analog repeater designs
(analog cross-point switches, FPGAs, custom DTMF/CTCSS tone detection chips, 
digital voice CODECs, voice delay modules, etc.) will not be necessary
if all of these functions can be performed in software.

The SDRC system was developed by Bruce MacKinnon (KC1FSZ) with design 
input from Dan Brown (W1DAN) of the Wellesley Amateur Radio Society (W1TKZ).
Please reach out with any questions/suggestions.

![Controller](docs/sdrc1.jpg)

Capabilities
============

This is development in process. The prototype is undergoing bench 
testing at the moment. We plan to install it at our repeater site within the 
next month.

Key capabilities of the software so far:

* Support for two receivers and two transmitters. Radios
can operate independently or can be linked to support
remote receiver or cross-band repeater systems.
* Hardware COS and CTCSS inputs are available for each radio. Positive
and negative logic are supported.
* An optically-isolated hardware PTT output is available for each radio.
* Optional soft CTCSS (PL) tone encoding and decoding, with support 
for independent frequencies for each transmitter/receiver.
* CWID generation.
* Configurable hang time.
* Configurable courtesy tone generation.
* Timeout and lockout with configurable times.
* Optional digital voice ID and other prompts.
* Soft RX/TX gain control adjustable remotely.
* "Soft console" via USB-connected computer computer with
serial terminal provides live display of the following
for each radio:
  - Carrier detect (COS) status
  - PL tone detect (CTSS) status
  - Push-to-talk (PTT) status
  - Receiver audio level RMS and peak
* (In development) Remote firmware update via LoRa connection.
* (In development) <= 3 second digital audio delay to avoid "static crashes."

Other things to know:

* The audio input/output range is around 2Vpp.
* Hardware gain adjustments (pots) are used to calibrate dynamic range 
during initial installation.
* The audio path is about 10 kHz wide, which should be plenty for 
an FM analog repeater system.
* Runs on +12VDC power input.
* DB25 connection for radio interfaces.
* Overvoltage protection is provided on the audio and logic inputs.
* Overvoltage, reverse-polarity, and transient spike protection is provided
on the power input.

Hardware specs:

* Microcontroller is the RP2350 running at 125 MHz.
* Audio path is 24-bits at 48k samples/second. More than enough for 
an analog FM repeater.
* Low-noise op amps are used for audio scaling (TL072).

More documentation:

* A demonstration video of the current prototype 
[can be seen here](https://www.youtube.com/watch?v=HBwrpokd7FI).
* A demonstration video of the soft console [can be seen here](https://www.youtube.com/watch?v=gWjOw0UzMgY).
* The hardware for the LoRa integration [is here](https://github.com/brucemack/lora-r2).
* The software for the LoRa integration [is here](https://github.com/brucemack/remote-probe).

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
