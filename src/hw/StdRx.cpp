#include <stdio.h>
#include <cassert>

#include <hardware/gpio.h>

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
    if (_cosMode == CosMode::COS_EXT_HIGH) {
        // There is a built-in inverter in the hardware
        cos = (gpio_get(_cosPin) == 0);
    }
    else if (_cosMode == CosMode::COS_EXT_LOW) {
        // There is a built-in inverter in the hardware
        cos = (gpio_get(_cosPin) == 1);
    }
    else {
        assert(false);
    }
    bool tone;
    if (_toneMode == ToneMode::TONE_NONE) {
        tone = true;
    }     
    else if (_toneMode == ToneMode::TONE_EXT_HIGH) {
        // There is a built-in inverter in the hardware
        tone = (gpio_get(_tonePin) == 0);
    }
    else if (_toneMode == ToneMode::TONE_EXT_LOW) {
        // There is a built-in inverter in the hardware
        tone = (gpio_get(_tonePin) == 1);
    }
    else {
        assert(false);
    }

    return cos && tone; 
}

}
