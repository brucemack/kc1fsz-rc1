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
            _running = false;
            _synth.setEnabled(false);
        }
    }
}

void CourtesyToneGenerator::start() {
    _running = true;
    _endTime = _clock.time() + 70;
    _synth.setFreq(1200);
    _synth.setEnabled(true);

}

bool CourtesyToneGenerator::isFinished() {
    return !_running;
}

}
