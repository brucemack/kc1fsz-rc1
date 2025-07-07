/**
 * Software Defined Repeater Controller
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
#include "AudioCore.h"

#include <cstring>
#include <fstream>

using namespace std;

namespace kc1fsz {

// HPF 41 taps, [0, 4000/32000]:0, [6000/32000, 0.5]:1.0
const float AudioCore::FILTER_B[] = 
{ 0.013698037426030683, -0.02113737303116375, -0.0296339958124383, -0.019902906836080543, 0.014630005866669179, 0.05145973167665524, 0.04961367173976759, -0.018617819885340652, -0.13902037897612904, -0.2554169605572119, 0.6964253195122007, -0.2554169605572119, -0.13902037897612904, -0.018617819885340652, 0.04961367173976759, 0.05145973167665524, 0.014630005866669179, -0.019902906836080543, -0.0296339958124383, -0.02113737303116375, 0.013698037426030683 };

AudioCore::AudioCore(unsigned id)
:   _id(id) {
    for (unsigned i = 0; i < FILTER_B_LEN; i++)
        _filtHistB[i] = 0;
}

void AudioCore::cycle0(const float* adc_in, float* cross_out) {

    for (unsigned i = 0; i < BLOCK_SIZE_ADC; i++) {
        
        // ----- HPF for noise detection -----
        {
            // Move the new sample into history
            _filtHistB[i] = adc_in[i];
            // Iterate backwards through history and forward
            // through the filter coefficients
            unsigned k = i;
            float a = 0;
            for (unsigned j = 0; j < FILTER_B_LEN; j++) {
                a += (_filtHistB[k] * FILTER_B[j]);
                // Move backwards with wrap
                if (k == 0)
                    k = BLOCK_SIZE_ADC - 1;
                else 
                    k = k - 1;
            }
            _filtOutB[i] = a;
        }
        
        // Decimation /2
        

        // Decimation /2 

        // BPF

        // CTCSS decode

        // DTMF decode
    }

    // Compute noise RMS
    float a = 0;
    for (unsigned i = 0; i < BLOCK_SIZE_ADC; i++)
        a += (_filtOutB[i] * _filtOutB[i]);
    a /= (float)BLOCK_SIZE_ADC;
    _noiseRms = sqrt(a);
}

void AudioCore::cycle1(const float** cross_in, float* dac_out) {

    // Noise squelch 

    // CTCSS encoder

    // Other tones/voice

    // Transmit Mix [L]

    // LPF 2.3kHz [M]

    // Interpolation *4 [N]
}

}

