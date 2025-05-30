#include <stdio.h>
#include "StdRx.h"

namespace kc1fsz {

StdRx::StdRx(Clock& clock, Log& log, int id, int cosPin, int tonePin) 
:   _clock(clock),
    _log(log),
    _id(id),
    _cosPin(cosPin),
    _tonePin(tonePin) {
    _startTime = _clock.time();
}

void StdRx::run() {
}

bool StdRx::isActive() const { 
    bool cos;
    if (_cosMode == CosMode::EXT_HIGH) {
        cos = gpio_get(_cosPin) == 1;
    }
    else if (_cosMode == CosMode::EXT_LOW) {
        cos = gpio_get(_cosPin) == 0;
    }
    else {
        assert(false);
    }
    bool tone;
    if (_toneMode == ToneMode::NONE) {
        tone = true;
    }     
    else if (_toneMode == ToneMode::EXT_HIGH) {
        tone = gpio_get(_tonePin) == 1;
    }
    else if (_toneMode == ToneMode::EXT_LOW) {
        tone = gpio_get(_tonePin) == 0;
    }
    else {
        assert(false);
    }

    return cos && tone; 
}

}
