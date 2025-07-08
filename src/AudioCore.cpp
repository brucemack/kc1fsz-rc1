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
#include <cmath>

using namespace std;

static float db(float l) {
    return 20.0 * log10(l);
}

namespace kc1fsz {

// HPF 41 taps, [0, 4000/32000]:0, [6000/32000, 0.5]:1.0
const float AudioCore::FILTER_B[] = 
{
0.009496502349662752, 0.032168266001826, -0.004020017447607337, -0.029774359071379836, -0.03025119554604127, 0.02111609845361212, 0.07216728736619965, 0.038324850322965634, -0.10997562615757675, -0.292262898960302, 0.6251660988707263, -0.292262898960302, -0.10997562615757675, 0.038324850322965634, 0.07216728736619965, 0.02111609845361212, -0.03025119554604127, -0.029774359071379836, -0.004020017447607337, 0.032168266001826, 0.009496502349662752
}
/*
{
    0.01412168806807286, -0.021791106217694617, -0.030550511146843623, -0.020518460655753096, 0.015082480274916685, 0.05305126976974765, 0.05114811519563668, -0.01919362874777388, -0.14331997832590626, -0.2633164541826926, 0.6870364118682479, -0.2633164541826926, -0.14331997832590626, -0.01919362874777388, 0.05114811519563668, 0.05305126976974765, 0.015082480274916685, -0.020518460655753096, -0.030550511146843623, -0.021791106217694617, 0.01412168806807286
}
*/
/*
{ 
    0.013698037426030683, -0.02113737303116375, -0.0296339958124383, -0.019902906836080543, 0.014630005866669179, 0.05145973167665524, 0.04961367173976759, -0.018617819885340652, -0.13902037897612904, -0.2554169605572119, 0.6964253195122007, -0.2554169605572119, -0.13902037897612904, -0.018617819885340652, 0.04961367173976759, 0.05145973167665524, 0.014630005866669179, -0.019902906836080543, -0.0296339958124383, -0.02113737303116375, 0.013698037426030683 
}
*/
;

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
    for (unsigned i = 0; i < SNR_HIST_SIZE; i++) {
        _snrHist[i] = 0;
    }
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

    // Work on the 16kHz samples
    for (unsigned i = 0; i < BLOCK_SIZE_ADC / 2; i++) {
        
        // Build the history that the FIR filters will use
        _hist16k[i] = _filtOutC[i];
        
        // Decimation LPF down to 8kHz. We only do this
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
                    a += (_hist16k[k] * FILTER_C[j]);
                // Move backwards with wrap
                if (k == 0)
                    k = (BLOCK_SIZE_ADC / 2) - 1;
                else 
                    k = k - 1;
            }
            // NOTE: Packing into half of the space
            _filtOutD[i / 2] = a;
        }
    }
    
    // Work on the 8kHz samples
    for (unsigned i = 0; i < BLOCK_SIZE_ADC / 4; i++) {
        
        // Build the history that the FIR filters will use
        _hist8k[i] = _filtOutD[i];

        // BPF
        // TODO: TEMP
        cross_out[i] = _hist8k[i];

        // CTCSS decode

        // DTMF decode        
    }

    // Compute noise RMS
    {
        float a = 0;
        for (unsigned i = 0; i < BLOCK_SIZE_ADC; i++)
            a += (_filtOutB[i] * _filtOutB[i]);
        a /= (float)BLOCK_SIZE_ADC;
        _noiseRms = sqrt(a);
    }

    // Compute the signal RMS
    {
        float a = 0;
        for (unsigned i = 0; i < BLOCK_SIZE_ADC / 4; i++)
            a += (_filtOutD[i] * _filtOutD[i]);
        a /= (float)(BLOCK_SIZE_ADC / 4);
        _signalRms = sqrt(a);
    }

    // SNR history
    float snr = db(getSignalRms() / getNoiseRms());
    _snrHist[_snrHistPtr] = snr;

    if (++_snrHistPtr == SNR_HIST_SIZE)
        _snrHistPtr = 0;
}

float AudioCore::getSnr() const {
    return db(getSignalRms() / getNoiseRms());
}

float AudioCore::getSnrAvg() const {
    float a = 0;
    for (unsigned i = 0; i < SNR_HIST_SIZE; i++)
        a += _snrHist[i];
    return a / (float)SNR_HIST_SIZE;
}

float AudioCore::getSnrMax() const {
    float a = 0;
    for (unsigned i = 0; i < SNR_HIST_SIZE; i++)
        if (_snrHist[i] > a) {
            a = _snrHist[i];
        }
    return a;
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

