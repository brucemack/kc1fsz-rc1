#include <stdio.h>
#include "TestRx.h"

namespace kc1fsz {

TestRx::TestRx(Clock& clock, Log& log) 
:   _clock(clock),
    _log(log) {
    _startTime = _clock.time();
    _active = false;
}

void TestRx::run() {
    // Simulate a quick kerchunck
    if (_state == 0) {
        if (_clock.isPast(_startTime + (10 * 1000))) {
            _active = true; 
            _state = 1;
            _log.info("TEST: Simulating station kerchunk");
        }
    }
    else if (_state == 1) {
        if (_clock.isPast(_startTime + (12 * 1000))) {
            _active = false; 
            _state = 2;
        }
    }
    // Simulate a long conversation (125 seconds)
    else if (_state == 2) {
        if (_clock.isPast(_startTime + (40 * 1000))) {
            _active = true; 
            _state = 3;
            _log.info("TEST: Simulating station key down for 125 seconds");
        }
    }
    else if (_state == 3) {
        if (_clock.isPast(_startTime + (165 * 1000))) {
            _active = false; 
            _state = 4;
        }
    }
    // Simulate a quick kerchunck inside of the lockout
    else if (_state == 4) {
        if (_clock.isPast(_startTime + (170 * 1000))) {
            _active = true; 
            _state = 5;
            _log.info("TEST: Simulating kerchunk inside of lockout (will be ignored)");
        }
    }
    else if (_state == 5) {
        if (_clock.isPast(_startTime + (172 * 1000))) {
            _active = false; 
            _state = 6;
        }
    }
    // Simulate a quick kerchunck after lockout
    else if (_state == 6) {
        if (_clock.isPast(_startTime + (205 * 1000))) {
            _active = true; 
            _state = 7;
            _log.info("TEST: Simulating kerchunk after lockout (should work normally now)");
        }
    }
    else if (_state == 7) {
        if (_clock.isPast(_startTime + (207 * 1000))) {
            _active = false; 
            _state = 8;
        }
    }
}

}
