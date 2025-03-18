Simulation Notes
================

## Audio Output Stage 2

This is the final filter in the audio output chain. A low-pass network is used to suppress noise. This is not used for pre-/de-emphasis.

The gain is provided in case the full-scale DAC amplitude isn't enough to drive the audio input of the radio.  

Simulated audio output filter (stage 2):

![System Picture](docs/sch-out2.jpg)

Frequency response of audio output filter (stage 2):

![System Picture](docs/ac-out2.jpg)
