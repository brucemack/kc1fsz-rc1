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
#ifndef _AudioCore_h
#define _AudioCore_h

#include <cstdint>
#include <cmath>

namespace kc1fsz {

/**
 * @brief The master application configuration structure.
 */
class AudioCore {
public:

    AudioCore(unsigned id);

    void cycle0(const float* adc_in, float* cross_out);
    void cycle1(const float** cross_in, float* dac_out);

    static const unsigned FS_ADC = 32000;
    static const unsigned BLOCK_SIZE_ADC = 256;
    static const unsigned FS = FS_ADC / 4;
    static const unsigned BLOCK_SIZE = BLOCK_SIZE_ADC / 4;

    float getSignalRms() const { return _signalRms; }
    float getNoiseRms() const { return _noiseRms; }

    float getSnr() const;
    float getSnrAvg() const;
    float getSnrMax() const;

    void setCtcssFreq(float hz);
    float getCtcssMag() const { return _ctcssMag; }

private:

    const unsigned _id;
    // These history buffers are used for FIR filter 
    // evaluation at the various sample rates
    float _hist32k[BLOCK_SIZE_ADC];
    float _hist16k[BLOCK_SIZE_ADC / 2];
    float _hist8k[BLOCK_SIZE_ADC / 4];

    static const unsigned FILTER_B_LEN = 41;
    static const float FILTER_B[FILTER_B_LEN];
    float _filtOutB[BLOCK_SIZE_ADC];

    static const unsigned FILTER_C_LEN = 19;
    static const float FILTER_C[FILTER_C_LEN];
    float _filtOutC[BLOCK_SIZE_ADC / 2];
    float _filtOutD[BLOCK_SIZE_ADC / 4];

    float _noiseRms;
    float _signalRms;
    const static unsigned SNR_HIST_SIZE = 8;
    float _snrHist[SNR_HIST_SIZE];
    unsigned _snrHistPtr = 0;

    // Used for CTCSS detection
    float _ctcssFreq = 123;
    float _gz1 = 0, _gz2 = 0;
    float _gcw, _gsw, _gc;
    float _ctcssMag = 0;
    unsigned _ctcssBlock = 0;
    unsigned _ctcssBlocks = 0;
};

}

#endif


