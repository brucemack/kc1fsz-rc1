#include "IDToneGenerator.h"

namespace kc1fsz {

// Space, dot, dash
// TODO: REMOVE TRAILING PAUSE
const char IDToneGenerator::_id[] = ". - -   . - - - -   -   - . -   - - . .      ";
const float freq = 600;
const unsigned int dotMs = 50;

IDToneGenerator::IDToneGenerator(Log& log, Clock& clock, ToneSynthesizer& synth) 
:   _log(log),
    _clock(clock),
    _synth(synth)
{
}

void IDToneGenerator::run() {
    if (_running) {
        if (_clock.isPast(_endTime))  {            
            if (_id[_idPtr] == '.') {
                _synth.setFreq(freq);
                _synth.setEnabled(true);
                _endTime = _clock.time() + dotMs;
                _idPtr++;
            }
            else if (_id[_idPtr] == '-') {
                _synth.setFreq(freq);
                _synth.setEnabled(true);
                _endTime = _clock.time() + (3 * dotMs);
                _idPtr++;
            }
            else if (_id[_idPtr] == ' ') {
                // Create a silent pause
                _synth.setEnabled(false);
                _endTime = _clock.time() + dotMs;
                _idPtr++;
            }
            else if (_id[_idPtr] == 0) {
                _synth.setEnabled(false);
                _running = false;
                _log.info("CWID end");
            }
        }
    }
}

void IDToneGenerator::start() {
    _running = true;
    _idPtr = 0;
    // This is set in the past so that we immediately start working on the first
    // symbol of the ID.
    _endTime = 0;
    _log.info("CWID start");
}

bool IDToneGenerator::isFinished() {
    return !_running;
}

}
