# Software Defined Repeater Controller - Users Guide

## Connectors

### Power Connector

Standard 5.5mm power connector. +12VDC on center pin.

### DB25 Radio Connector

| Pin | Function                | IO |
|-----|-------------------------|----|
| 1   | Radio 0 audio in        | I  |
| 2   | Radio 0 COS             | I  |
| 3   | Radio 1 COS             | I  |
| 4   | NC                      |    |
| 5   | Radio 0 CTCSS           | I  |
| 6   | Radio 1 CTCSS           | I  |
| 7   | NC                      |    |
| 8   | Radio 1 audio in        | I  |
| 9   | NC                      |    |
| 10  | Radio 0 PTT             | O  |
| 11  | Radio 1 PTT             | O  |
| 12  | NC                      |    |
| 13  | NC                      |    |
| 14  | Radio 0 audio out       | O  |
| 15  | Radio 1 audio out       | O  |
| 16  | NC                      |    |
| 17  | NC                      |    |
| 18  | NC                      |    |
| 19  | Ground                  |    |
| 20  | Ground                  |    |
| 21  | Ground                  |    |
| 22  | Ground                  |    |
| 23  | NC                      |    |
| 24  | NC                      |    |
| 25  | NC                      |    |

NOTES:
* The terms "audio in" and "audio out" are from the perspective of the controller. The "audio in" pins take audio from a receiver and the "audio out" pins deliver audio to a transmitter.
* Audio in and out pins have DC blocking capacitors.
* Audio out pins support 0 to 1.5Vpp into a 600 ohm load. 
* The COS/CTCSS are 5V logic inputs. The software allows the polarity of these controls to be configured to support positive or negative logic (depending on radio)
* The PTT pins are open collector to ground *and can sink at most 50mA*.  Please plan accordingly if your transmitter PTT circuit involves a larger current requirement (i.e. mechanical relays, etc.)

### USB Connector

Male, USB-A. For console connection. Runs at 115,200 baud.

## Shell Commands

####  ping
Responds with pong

#### reset 
Causes a reboot

#### save 
Saves the current configuration to flash

#### factory
Restores the configuration to the factory defaults

#### set call (callsign)
Sets the station callsign. Limit 16 characters, slashes are allowed.

#### set timeout (radio) (duration_ms)
Controls the longest continuous transmission before the transmitter times out and enters lockout state.

(radio) is either 0 or 1.

(duration_ms) Milliseconds for timeout. Default is 2 minutes.

#### set lockout (radio) (duration_ms)
Controls how long the radio is locked out (i.e. shut off) after a timeout condition is detected. This timer starts after the connected receivers go  inactive.

(radio) is either 0 or 1.

(duration_ms) Milliseconds for lockout.  Default is 1 minute.

#### set hang (radio) (duration_ms)

Controls the length of the hang interval time. The hang time starts when the receiver drops and ends when the courtesy tone is generated. The transmitters remain keyed during the hang time.

(radio) is either 0 or 1.

(ms) milliseconds for the hang interval.  Default is 1000ms.

#### set ctmode (radio) (mode)

Controls the courtesy tone mode

(radio) is either 0 or 1.

(mode) one of the following:

* 0 - No courtesy tone
* 1 - Single tone
* 1 - Upchirp
* 2 - Downchrip

#### set ctlvl (radio) (level_db)
Controls the audio level of the courtesy tone.

(radio) is either 0 or 1.

(level) is the audio level in dB relative to full scale.

#### set idlvl (radio) (level_db)
Controls the audio level of the CW ID.

(radio) is either 0 or 1.

(level) is the audio level in dB relative to full scale.

#### set cosmode (radio) (mode)
(radio) is either 0 or 1.

(mode) is one of the following:

* 0 - No COS detection used.
* 1 - Hardware COS detect, active low polarity
* 2 - Hardware COS detect, active high polarity (default)
* 3 - Soft COS detect based on receive audio level

#### set cosondur (radio) (duration_ms)

(radio) is either 0 or 1.

(duration_ms) the length of time in milliseconds that the COS signal be on before it is considered active. Default is 50ms.

#### set cosoffdur (radio) (duration_ms)

(radio) is either 0 or 1.

(duration_ms) the length of time in milliseconds that the COS signal be off before it is considered inactive. Default is 50ms.

#### set coslvl (radio) (level_db) 
Used for soft COS detect only.

(radio) is either 0 or 1.

(level) is the audio level in dB relative to full scale used to trigger the soft COS.

#### set ctcssdecmode (radio) (mode)
(radio) is either 0 or 1.

(mode) is one of the following:

* 0 - No CTCSS detection used.
* 1 - Hardware CTCSS detect, active low polarity
* 2 - Hardware CTCSS detect, active high polarity (default)
* 3 - Soft CTCSS detect

#### set ctcssdecondur (radio) (duration_ms)

(radio) is either 0 or 1.

(duration_ms) the length of time in milliseconds that the CTCSS signal must be on before it is considered active.  Default is 50ms.

#### set ctcssdecoffdur (radio) (duration_ms)

(radio) is either 0 or 1.

(duration_ms) the length of time in milliseconds that the CTCSS signal be off before it is considered inactive. Default is 50ms.

#### set ctcssdecfreq (radio) (freq_hz) 

Used for soft CTCSS detect only.

(radio) is either 0 or 1.

(freq_hz) the CTCSS frequency being detected

#### set ctcssencmode (radio) (mode)

Controls the CTCSS encoding function.

(radio) is either 0 or 1.

(mode) is one of the following:

* 0 - No CTCSS tone is generated (default)
* 1 - CTCSS is generated in software 

#### set ctcssencfreq (radio) (freq_hz) 
Used for soft CTCSS encoding only.

(radio) is either 0 or 1.

(freq_hz) the CTCSS frequency being encoded
