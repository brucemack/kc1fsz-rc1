#include "CourtesyToneGenerator.h"

namespace kc1fsz {

CourtesyToneGenerator::CourtesyToneGenerator(Log& log, Clock& clock, ToneSynthesizer& synth) 
:   _log(log),
    _clock(clock),
    _synth(synth)
{
}

void CourtesyToneGenerator::run() {
    if (_running) {
        if (_clock.isPast(_endTime)) {
            if (_part == 0) {
                if (_type == Type::FAST_DOWNCHIRP)
                    _synth.setFreq(1000);
                else if (_type == Type::FAST_UPCHIRP)
                    _synth.setFreq(1280);
                _part = 1;
                _endTime = _clock.time() + _chirpMs;
            } else {
                _running = false;
                _synth.setEnabled(false);
            }
        }
    }
}

void CourtesyToneGenerator::start() {
    _running = true;
    _part = 0;
    _endTime = _clock.time() + _chirpMs;
    if (_type == Type::FAST_DOWNCHIRP)
        _synth.setFreq(1280);
    else if (_type == Type::FAST_UPCHIRP)
        _synth.setFreq(1000);
    _synth.setEnabled(true);
}

bool CourtesyToneGenerator::isFinished() {
    return !_running;
}

}
