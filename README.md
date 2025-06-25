Overview
========

This is a basic two-radio repeater controller that can be used 
to create an amateur or GMRS repeater site. Our goal is to implement
as much of the repeater functionality in software as 
possible. For this reason, we call this the "Software 
Defined Repeater Controller" (SDRC) project. This Github
repo contains the source code for the controller and the 
KiCad design files for the hardware.

The hardware is on two boards. The radio interface board is on the left
and the digital control board is on the right. The digital control board
is mostly a Pico Pi 2. (To avoid any confusion: the Pico is an ARM Cortex-M33 microcontroller. This _is not_ a Linux system like you'd find on a full 
a Raspberry Pi single-board computer).

![Controller](docs/boards.jpg)

One of the advantages of an all-digital architecture is that integration
with other digital-voice technologies should be seamless. Please
see the related project called [MicroLink](https://github.com/brucemack/microlink) which
provides a compact EchoLink implementation. Soon the SDRC and MicroLink
systems will be integrated. Reverse-engineering of the D-Star/DMR protocols
will also enable direct integration of those modes at some point. 

Much of the hardware used in traditional analog repeater designs
(analog cross-point switches, FPGAs, custom DTMF/CTCSS tone detection chips, 
digital voice CODECs, voice delay modules, etc.) will not be necessary
if these functions can be performed in software.

A [users guide is provided here](docs/users.md).

The SDRC system was developed by Bruce MacKinnon (KC1FSZ) with extensive design 
input from Dan Brown (W1DAN) of the Wellesley Amateur Radio Society (W1TKZ).
Please reach out with any questions/suggestions.

![Controller](docs/sdrc1.jpg)

Capabilities
============

The development is in process. The prototype is undergoing bench 
testing at the moment. We plan to install it at our repeater site within the 
next month.

Key capabilities of the software so far:

* Support for two receivers and two transmitters. Radios
can operate independently or can be linked to support
remote receiver or cross-band repeater operation.
* Hardware COS and CTCSS inputs for each radio. Positive
and negative logic are supported.
* An optically-isolated hardware PTT output for each radio.
* Optional soft CTCSS (PL) tone encoding and decoding, with support 
for independent tone frequencies for each transmitter/receiver.
* CWID generation.
* Configurable hang time.
* Configurable courtesy tone generation.
* Timeout and lockout with configurable times.
* Optional digital voice ID and other prompts.
* Soft RX/TX gain control adjustable remotely.
* "Soft console" via USB-connected computer computer with
serial terminal provides a configuration shell and live display of the following:
  - Carrier detect (COS) status
  - PL tone detect (CTCSS) status
  - Push-to-talk (PTT) status
  - Receive/transmit audio level RMS and peak
* (In development) Remote firmware update via LoRa connection.
* (In development) <=1 second digital audio delay to avoid "static crashes."
* Microcontroller uses a watchdog timer to limit the risk of lockup.

Other things to know:

* The audio input/output range is around 1.5Vpp into 600 ohms.
* Hardware gain adjustments (pots) are used to calibrate dynamic range 
during initial installation.
* The audio path is about 10 kHz wide, which should be plenty for 
an FM analog repeater system.
* The controller runs on +12VDC power input.
* A DB25 connection is used for radio interfaces.
* Over-voltage protection is provided on the audio and logic inputs.
* Over-voltage, reverse-polarity, and transient spike protection is provided
on the power input.
* During testing of the prototype board it was determined that the 
controller consumes ~105mA when idling and ~135mA when repeating. These are
fairly low current consumptions that indicate the the controller may 
be relevant in solar/battery installations.

Hardware specs:

* Microcontroller is the RP2350 running at 125 MHz.
* Audio path is 24-bits at 32k samples/second. More than enough for 
an analog FM repeater.
* Low-noise op amps are used for audio scaling (TLV9152).

More documentation:

* A demonstration video of the current prototype 
[can be seen here](https://www.youtube.com/watch?v=HBwrpokd7FI).
* A demonstration video of the soft console [can be seen here](https://www.youtube.com/watch?v=gWjOw0UzMgY).
* The schematic for the main radio interface board [is here](https://github.com/brucemack/kc1fsz-rc1/blob/main/hw/if-2/plots/if-2.pdf).
* The schematic for the microcontroller board [is here](https://github.com/brucemack/kc1fsz-rc1/blob/main/hw/digital-2/plots/digital-2.pdf).
* The hardware project for the LoRa integration [is here](https://github.com/brucemack/lora-r2).
* The software project for the LoRa integration [is here](https://github.com/brucemack/remote-probe).

Credit/Thanks
=============

I've received help from a lot of smart people on this project.

* Dan Brown W1DAN, former president of the Wellesley Club.
* Tom Kinahan N1CPE, trustee of the W1TKZ repeater system.
* Leandra MacLennan AF1R
* Steve Kondo K1STK
* George Zafiropoulos KJ6VU

Legal/License
=============

This work is being made available for non-commercial use by the amateur radio community. Redistribution, commercial use or sale of any part is prohibited.

The hardware for this project is published under the terms of [The TAPR Open Hardware License](https://tapr.org/the-tapr-open-hardware-license).

The software for this project is published under the terms of [GNU GENERAL PUBLIC LICENSE](https://www.gnu.org/licenses/gpl-3.0.en.html).

Technical/Design Notes
======================

The ADC/DAC is 24-bit, so full scale runs from +/- 8,388,608. Audio sample rate
is 16kHz.

Dan suggested that tones (ex: CWID) should be generated at a level of between
-14dB and -10dB of full DAC full scale.  A tone of -10dB down should scale 
by 10<sup>(-10/20)</sup> = 0.32 linear scale. 

The PL tone should be another -12dB down, or 0.32 * 0.25 = 0.08 linear scale.

Op Amp Notes
------------

* [TL072]() - Output to within +/- 1.5V (typ) of rail.
* [LMC660](https://www.ti.com/lit/ds/symlink/lmc662.pdf) - Used in SCOM-7K, output rail-to-rail, can drive +/- 18mA.  Characterized as 4.6 to 0.3 output swing with R<sub>L</sub> = 600 ohms. A quad device, not a good choice.
* [TS952](https://www.st.com/content/ccc/resource/technical/document/datasheet/b8/ac/21/72/20/a7/4d/ea/CD00001338.pdf/files/CD00001338.pdf/jcr:content/translations/en.CD00001338.pdf) - Used in the SCOM 7330, output close to rail-to-rail with R<sub>L</sub> = 600 ohms.
* [TLV2462](https://www.ti.com/lit/ds/symlink/tlv2462-q1.pdf?ts=1750512090542) - Output rail-to-rail, can drive +/- 80mA. Noise 11nV/RHz.
* [TLV9152](https://www.ti.com/lit/ds/symlink/tlv9152.pdf?ts=1750522536760) - Suggested by Dan, output rail-to-rail, noise 10.5nV/RHz.

Notes on Zener Biasing for Single Supply Op Amps
------------------------------------------------

From [Analog Devices application note AN-581](https://www.analog.com/en/resources/app-notes/an-581.html):

A Zener should be chosen that has an operating voltage close to Vs/2. Resistor RZ needs to be selected to provide a high enough Zener current to operate the Zener at its stable rated voltage and to keep the Zener output noise low. It is also important to minimize power consumption (and heating) and to prolong the life of the Zener. As the op amp’s input current is essentially zero, it’s a good idea to choose a low power Zener. A 250 mW device is best but the more common 500 mW types are also acceptable. The ideal Zener current varies with each manufacturer but practical IZ levels between 5 mA (250 mW Zener) and 5 µA (500 mW Zener) are usually a good compromise for this application.

Relevant FCC Regulations
========================

### FCC Part 97 Section 119: Station identification

(a) Each amateur station, except a space station or telecommand station, must transmit its assigned call sign on its transmitting channel at the end of each communication, and at least every 10 minutes during a communication, for the purpose of clearly making the source of the transmissions from the station known to those receiving the transmissions. No station may transmit unidentified communications or signals, or transmit as the station call sign, any call sign not authorized to the station.

### FCC Part 97 Section 213: Telecommand of an amateur station.

An amateur station on or within 50 km of the Earth's surface may be under telecommand where:

(a) There is a radio or wireline control link between the control point and the station sufficient for the control operator to perform his/her duties. If radio, the control link must use an auxiliary station. A control link using a fiber optic cable or another telecommunication service is considered wireline.

(b) Provisions are incorporated to limit transmission by the station to a period of no more than 3 minutes in the event of malfunction in the control link.

(c) The station is protected against making, willfully or negligently, unauthorized transmissions.

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

Work In Process
===============
Firmware
* Finish configuration/command interface
* Separate Morse ID from voice ID 
* Implement polarity control PTT pins
* DTMF decode and TX shutoff commands.
* Improve layout of live status page
* Come up with a way to load voice prompts
* Load rest of voice prompts
* AGC experimentation
* Implement blended audio mode

Hardware
* Enclosure
* Change to MOSFET switch for PTT
* Overvoltage protection testing
* Send board revision to fab
* Live test at NWH
* Self-test feature

