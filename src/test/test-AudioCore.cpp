#include <iostream>

// Radlib
#include "util/dsp_util.h"

#include "AudioCore.h"



using namespace std;
using namespace kc1fsz;
using namespace radlib;

int main(int argc, const char** argv) {

    AudioCore core0(0), core1(1);

    const unsigned test_in_len = AudioCore::FS_ADC;
    unsigned test_blocks = test_in_len / AudioCore::BLOCK_SIZE_ADC;

    // Fill in the test audio
    float test_in_0[test_in_len];
    make_real_tone_f32(test_in_0, test_in_len, AudioCore::FS_ADC, 1000); 

    float test_in_1[test_in_len];
    make_real_tone_f32(test_in_1, test_in_len, AudioCore::FS_ADC, 1000); 

    float* adc_in_0 = test_in_0;
    float* adc_in_1 = test_in_1;

    float cross_out_0[AudioCore::BLOCK_SIZE];
    float cross_out_1[AudioCore::BLOCK_SIZE];
    const float* cross_in[2] = { cross_out_0, cross_out_1 };

    float dac_out_0[AudioCore::BLOCK_SIZE_ADC];
    float dac_out_1[AudioCore::BLOCK_SIZE_ADC];

    for (unsigned block = 0; block < test_blocks; block++) {

        // Cycle 0
        core0.cycle0(adc_in_0, cross_out_0);
        core1.cycle0(adc_in_1, cross_out_1);

        // Cycle 1
        core0.cycle1(cross_in, dac_out_0);
        core1.cycle1(cross_in, dac_out_1);

        adc_in_0 += AudioCore::BLOCK_SIZE_ADC;
        adc_in_1 += AudioCore::BLOCK_SIZE_ADC;
    }

    return 0;
}
