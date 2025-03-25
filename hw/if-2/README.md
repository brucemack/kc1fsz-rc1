## Audio Input Network Simulation

Simulated audio input network. The trim pot is set to half range.

![System Picture](docs/sch-in1.jpg)

Frequency response. We are using an ADC with a differential input so a differential gain is plotted:

![System Picture](docs/ac-in1.jpg)

Frequency response focusing on the range from 60 Hz to 2 kHz. This range is important since we will
be decoding sub-audible CTCSS tones and we want to make sure they are passed into the ADC. The lowest
tone (66 Hz) is only attenuated by about 0.3 dB vs the audible range starting at 200 Hz.

![System Picture](docs/ac-in1-b.jpg)

## Audio Output Network Simulation

This is the final filter in the audio output chain. The trim pot is set to half range. A low-pass network is used to suppress noise. This is not used for pre-/de-emphasis.

CTCSS tones need to pass through this network so very low frequency response is necessary.

The gain is provided in case the full-scale DAC amplitude isn't enough to drive the audio input of the radio.  

Simulated audio output network (stage 2):

![System Picture](docs/sch-out2.jpg)

Frequency response:

![System Picture](docs/ac-out2.jpg)

Frequency response focusing on the range from 60 Hz to 2 kHz. The lowest tone is only attenuated by about 0.6 dB vs. the audible range. This should be fine.

![System Picture](docs/ac-out2-b.jpg)

## ADC Testing at Low Audio Frequency

(To Follow)

## DAC Testing at Low Audio Frequency

(To Follow)