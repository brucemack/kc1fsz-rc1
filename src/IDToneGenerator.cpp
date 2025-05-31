#include "IDToneGenerator.h"

namespace kc1fsz {

IDToneGenerator::IDToneGenerator(Log& log, Clock& clock) 
:   _log(log),
    _clock(clock)
{
}

void IDToneGenerator::run() {
}

void IDToneGenerator::start() {
    _endTime = _clock.time() + 5000;
    _log.info("CWID start");
}

bool IDToneGenerator::isFinished() {
    return _clock.isPast(_endTime);
}

}
