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
{ 
    0.013698037426030683, -0.02113737303116375, -0.0296339958124383, -0.019902906836080543, 0.014630005866669179, 0.05145973167665524, 0.04961367173976759, -0.018617819885340652, -0.13902037897612904, -0.2554169605572119, 0.6964253195122007, -0.2554169605572119, -0.13902037897612904, -0.018617819885340652, 0.04961367173976759, 0.05145973167665524, 0.014630005866669179, -0.019902906836080543, -0.0296339958124383, -0.02113737303116375, 0.013698037426030683 
};

// Half-band LPF 19 taps
const float AudioCore::FILTER_C[] =
{
0.004556288373474164, 0, -0.01312956925973412, 0, 0.03591260638886463, 0, -0.08753689171678947, 0, 0.3117950946271571, 0.5, 0.3117950946271571, 0, -0.08753689171678947, 0, 0.03591260638886463, 0, -0.01312956925973412, 0, 0.004556288373474164     
};    

AudioCore::AudioCore(unsigned id)
:   _id(id) {
    for (unsigned i = 0; i < BLOCK_SIZE_ADC; i++)
        _hist32k[i] = 0;
    for (unsigned i = 0; i < BLOCK_SIZE_ADC / 2; i++)
        _hist16k[i] = 0;
    for (unsigned i = 0; i < BLOCK_SIZE_ADC / 4; i++)
        _hist8k[i] = 0;
}

void AudioCore::cycle0(const float* adc_in, float* cross_out) {

    // Work on the 32kHz samples
    for (unsigned i = 0; i < BLOCK_SIZE_ADC; i++) {
        
        // Build the history the the FIR filters will use
        _hist32k[i] = adc_in[i];
        
        // HPF for noise detection
        {
            // Iterate backwards through history and forward
            // through the filter coefficients
            unsigned k = i;
            float a = 0;
            for (unsigned j = 0; j < FILTER_B_LEN; j++) {
                a += (_hist32k[k] * FILTER_B[j]);
                // Move backwards with wrap
                if (k == 0)
                    k = BLOCK_SIZE_ADC - 1;
                else 
                    k = k - 1;
            }
            _filtOutB[i] = a;
        }
        
        // Decimation LPF down to 16kHz. We only do this
        // on half of the input samples.
        if (i % 2 == 0) {
            // Iterate backwards through history and forward
            // through the filter coefficients.
            // TODO: LEVERAGE THE SYMMETRY OF THE COEFFICIENTS
            // TO ELIMINATE SOME MULTIPLICATIONS.
            unsigned k = i;
            float a = 0;
            for (unsigned j = 0; j < FILTER_C_LEN; j++) {
                // Many of the coefficients will be zero
                if (FILTER_C[j] != 0)
                    a += (_hist32k[k] * FILTER_C[j]);
                // Move backwards with wrap
                if (k == 0)
                    k = BLOCK_SIZE_ADC - 1;
                else 
                    k = k - 1;
            }
            // NOTE: Packing into half of the space
            _filtOutC[i / 2] = a;
        }
    }

    // Compute noise RMS
    {
        float a = 0;
        for (unsigned i = 0; i < BLOCK_SIZE_ADC; i++)
            a += (_filtOutB[i] * _filtOutB[i]);
        a /= (float)BLOCK_SIZE_ADC;
        _noiseRms = sqrt(a);
    }
    
    /*
    // Work on the 16kHz samples
    for (unsigned i = 0; i < BLOCK_SIZE_ADC / 2; i++) {
        
        // Build the history that the FIR filters will use
        _hist16k[i] = _filtOutC[i];
        
        // Decimation LPF down to 8kHz
        if (i % 2 == 0)
            decimate2(_hist16k, BLOCK_SIZE_ADC / 2, i,
                FILTER_C, FILTER_C_LEN, _filtOutD);
    }
    */

    // Compute the signal RMS
    {
        float a = 0;
        for (unsigned i = 0; i < BLOCK_SIZE_ADC / 2; i++)
            a += (_filtOutC[i] * _filtOutC[i]);
        a /= (float)(BLOCK_SIZE_ADC / 2);
        _signalRms = sqrt(a);
    }

    // BPF

    // CTCSS decode

    // DTMF decode
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

