#include "CourtesyToneGenerator.h"

namespace kc1fsz {

CourtesyToneGenerator::CourtesyToneGenerator(Log& log, Clock& clock) 
:   _log(log),
    _clock(clock)
{
}

void CourtesyToneGenerator::run() {
}

void CourtesyToneGenerator::start() {
    _endTime = _clock.time() + 2000;
}

bool CourtesyToneGenerator::isFinished() {
    return _clock.isPast(_endTime);
}

}
