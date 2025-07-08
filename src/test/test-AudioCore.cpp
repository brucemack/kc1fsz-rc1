#include <iostream>
#include <fstream>
#include <random>
#include <string>

// Radlib
#include "util/dsp_util.h"

#include "AudioCore.h"

using namespace std;
using namespace kc1fsz;
using namespace radlib;

static float db(float l) {
    return 20.0 * log10(l);
}

// Function to generate a block of white noise samples
void generateWhiteNoise(unsigned int numSamples, double amplitude, float* out) {
    // Obtain a random number from hardware
    std::random_device rd;
    // Seed the generator
    std::mt19937 gen(rd()); 
    std::uniform_real_distribution<> distrib(-amplitude, amplitude);
    for (unsigned int i = 0; i < numSamples; ++i)
        out[i] = distrib(gen);
}

unsigned loadFromFile(const char* fn, float* target, 
    unsigned target_max) {
    ifstream is(fn);
    unsigned i = 0;
    float scale = 32767.0;
    std::string line;
    while (getline(is, line))
        if (i < target_max)
            target[i++] = stof(line) / scale;
    return i;
}

int main(int argc, const char** argv) {

    AudioCore core0(0), core1(1);

    const unsigned test_in_max = AudioCore::FS_ADC * 7;
    const unsigned test_blocks = test_in_max / AudioCore::BLOCK_SIZE_ADC;
    float test_in_0[test_in_max];
    float test_in_1[test_in_max];
    for (unsigned i = 0; i < test_in_max; i++) {
        test_in_0[i] = 0;
        test_in_1[i] = 0;
    }
 
    // Fill in the test audio
    float ft = 3000;
    //generateWhiteNoise(test_in_len / 2, 1.0, test_in_0);
    //make_real_tone_f32(test_in_0, test_in_max, AudioCore::FS_ADC, ft); 
    //make_real_tone_f32(test_in_0 + (test_in_len / 2), test_in_len / 2, AudioCore::FS_ADC, ft); 
    unsigned test_in_0_len = loadFromFile("./tests/clip-1.txt", test_in_0, test_in_max);

    /*
    ofstream os("out.txt");
    for (unsigned i = 0; i < AudioCore::FS_ADC; i++)
        os << test_in_0[i] << endl;
    os.close();
    */

    //float ft = 3000;
    make_real_tone_f32(test_in_1, test_in_max, AudioCore::FS_ADC, ft); 

    float* adc_in_0 = test_in_0;
    float* adc_in_1 = test_in_1;

    float cross_out_0[AudioCore::BLOCK_SIZE];
    float cross_out_1[AudioCore::BLOCK_SIZE];
    const float* cross_in[2] = { cross_out_0, cross_out_1 };

    float dac_out_0[AudioCore::BLOCK_SIZE_ADC];
    float dac_out_1[AudioCore::BLOCK_SIZE_ADC];

    ofstream os("tests/clip-1b.txt");

    bool noiseSquelchEnabled = true;
    enum SquelchState { SQUELCHED, UNSQUELCHED, TAIL }
        squelchState = SquelchState::SQUELCHED;
    float lastSnr = 0;
    unsigned tailCount = 0;

    for (unsigned block = 0; block < test_blocks; block++) {

        // Cycle 0
        core0.cycle0(adc_in_0, cross_out_0);
        core1.cycle0(adc_in_1, cross_out_1);

        // Cycle 1
        core0.cycle1(cross_in, dac_out_0);
        core1.cycle1(cross_in, dac_out_1);

        adc_in_0 += AudioCore::BLOCK_SIZE_ADC;
        adc_in_1 += AudioCore::BLOCK_SIZE_ADC;
        
        float n_0 = db(core0.getNoiseRms());
        float s_0 = db(core0.getSignalRms());
        float snr = core0.getSnr();
        //float snr = core0.getSnrMax();
        //cout << block << " " << s_0 << " " << n_0 << endl;
        //cout << block << " " << core0.getSnr() << " " << core0.getSnrAvg() << 
        //" " << s_0 << " " << n_0 << endl;
        //cout << n_0 << endl;
        //os << s_0 << endl;
        if (snr <= 10)
            cout << block << " " << s_0 << " " << n_0 << " " << snr << endl;
        //os << core0.getSnrMax() << endl;

        // Calculate the noise squelch
        if (squelchState == SquelchState::SQUELCHED) {
            // Look for unsquelch
            if (snr > 10) {
                squelchState = SquelchState::UNSQUELCHED;
            } 
        }
        else if (squelchState == SquelchState::TAIL) {
            // Look for unsquelch
            if (snr > 10) {
                squelchState = SquelchState::UNSQUELCHED;
            } 
            else if (tailCount == 0) {
                squelchState = SquelchState::SQUELCHED;
            }
            else {
                tailCount--;
            }
        }
        else if (squelchState == SquelchState::UNSQUELCHED) {
            // If the SNR is good 
            if (snr > 10) {
            }
            else {
                squelchState = SquelchState::TAIL;
                // If we dropping from a high number then 
                // squelch immediately
                if (lastSnr > 20) {
                    tailCount = 18;
                } 
                else {
                    tailCount = 4;
                }
            }
        }

        lastSnr = snr;

        // Write out block of audio
        for (unsigned i = 0; i < AudioCore::BLOCK_SIZE_ADC / 4; i++) {
            if (!noiseSquelchEnabled ||
                squelchState != SquelchState::SQUELCHED) {
                os << (int)(cross_out_0[i] * 32767.0) << endl;                
            } else {
                os << 0 << endl;
            }
        }

    }

    os.close();

    return 0;
}
