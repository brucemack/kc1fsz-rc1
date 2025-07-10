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

// BPF 127 taps
const float AudioCore::FILTER_F[] =
{
-0.00148000309963449, -0.0005269741357922974, -0.0005685671070370724, -0.0005711749829735852, -0.0005266916725728142, -0.00042913627308751234, -0.0002755717663130774, -6.669740116989601e-05, 0.00019289529930185335, 0.0004941557060915148, 0.0008232593800761903, 0.0011624032641297407, 0.001489723786596802, 0.0017811735068456627, 0.0020109214330739852, 0.0021540659927329407, 0.0021879021452668344, 0.002093326851899909, 0.0018571203462031307, 0.0014728586325032935, 0.0009427185177111338, 0.00027848441890266826, -0.0004971585694644127, -0.001351364271152696, -0.002243016335197132, -0.003123981930466243, -0.003939575261888304, -0.004629849806552187, -0.005136281334100473, -0.005410089999381532, -0.005397451726151065, -0.005066687724484985, -0.004393914824044533, -0.003374842955357053, -0.0020238573686101767, -0.0003761987668511685, 0.0015116002875365363, 0.0035626724415071653, 0.0056816769290264, 0.007757243155979987, 0.009666942303341823, 0.011280756622100796, 0.012468863095060578, 0.013105561896983975, 0.013077176347114467, 0.012287917930839233, 0.010664760904990755, 0.008163304148203593, 0.004771830172095333, 0.0005137604348397658, -0.004551985044294312, -0.01033095443097309, -0.016694548763785236, -0.023484203379860298, -0.03051728622988254, -0.037593632540821084, -0.04450143345882363, -0.05102617591830403, -0.05696183922773863, -0.06211400140976345, -0.06631327201855662, -0.06941849364373319, -0.07132493713929951, 0.9280322559969071, -0.07132493713929951, -0.06941849364373319, -0.06631327201855662, -0.06211400140976345, -0.05696183922773863, -0.05102617591830403, -0.04450143345882363, -0.037593632540821084, -0.03051728622988254, -0.023484203379860298, -0.016694548763785236, -0.01033095443097309, -0.004551985044294312, 0.0005137604348397658, 0.004771830172095333, 0.008163304148203593, 0.010664760904990755, 0.012287917930839233, 0.013077176347114467, 0.013105561896983975, 0.012468863095060578, 0.011280756622100796, 0.009666942303341823, 0.007757243155979987, 0.0056816769290264, 0.0035626724415071653, 0.0015116002875365363, -0.0003761987668511685, -0.0020238573686101767, -0.003374842955357053, -0.004393914824044533, -0.005066687724484985, -0.005397451726151065, -0.005410089999381532, -0.005136281334100473, -0.004629849806552187, -0.003939575261888304, -0.003123981930466243, -0.002243016335197132, -0.001351364271152696, -0.0004971585694644127, 0.00027848441890266826, 0.0009427185177111338, 0.0014728586325032935, 0.0018571203462031307, 0.002093326851899909, 0.0021879021452668344, 0.0021540659927329407, 0.0020109214330739852, 0.0017811735068456627, 0.001489723786596802, 0.0011624032641297407, 0.0008232593800761903, 0.0004941557060915148, 0.00019289529930185335, -6.669740116989601e-05, -0.0002755717663130774, -0.00042913627308751234, -0.0005266916725728142, -0.0005711749829735852, -0.0005685671070370724, -0.0005269741357922974, -0.00148000309963449
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
    float ctcssTotal = 0;
    
    for (unsigned i = 0; i < BLOCK_SIZE; i++) {
        
        float s = _filtOutD[i];

        // Build the history that the FIR filters will use
        _hist8k[_hist8KPtr] = s;

        // BPF
        {
            // Iterate backwards through history and forward
            // through the filter coefficients
            unsigned k = _hist8KPtr;
            float a = 0;
            for (unsigned j = 0; j < FILTER_F_LEN; j++) {
                a += (_hist8k[k] * FILTER_F[j]);
                // Move backwards with wrap
                if (k == 0)
                    k = HIST_8K_LEN - 1;
                else 
                    k = k - 1;
            }
            cross_out[i] = a;

            if (++_hist8KPtr == HIST_8K_LEN)
                _hist8KPtr = 0;
        }

        // CTCSS decode
        float z0 = s + _gc * _gz1 - _gz2;
        _gz2 = _gz1;
        _gz1 = z0;

        // DTMF decode        
    }

    // Look to see if we can update the CTCSS estimation
    if (++_ctcssBlock == _ctcssBlocks) {
        float gi = _gcw * _gz1 - _gz2;
        float gq = _gsw * _gz1;
        _ctcssMag = sqrt(gi * gi + gq * gq);
        // Scale down by half of the sample count
        _ctcssMag /= (float)(_ctcssBlocks * BLOCK_SIZE / 2.0);
        // Reset for the next block
        _gz1 = 0;
        _gz2 = 0;
        _ctcssBlock = 0;
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

void AudioCore::setCtcssFreq(float hz) {
    _ctcssFreq = hz;
    _ctcssBlocks = 8;
    _ctcssBlock = 0;
    float gw =  2.0 * 3.1415926 * hz / (float)FS;
    _gcw = cos(gw);
    _gsw = sin(gw);
    _gc = 2.0 * _gcw;
    _gz1 = 0;
    _gz2 = 0;
}

}

