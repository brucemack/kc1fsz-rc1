#include "TestRx.h"

namespace kc1fsz {

TestRx::TestRx(Clock& clock) 
:   _clock(clock) {
    _startTime = _clock.time();
    _active = false;
}

void TestRx::run() {
    if (_clock.isPast(_startTime + 10 * 1000)) {
        _active = true; 
    }
}

}
