#include <stdio.h>
#include <cassert>

#include <hardware/gpio.h>

#include "StdRx.h"

namespace kc1fsz {

StdRx::StdRx(Clock& clock, Log& log, int id, int cosPin, int tonePin,
    CourtesyToneGenerator::Type courtesyType) 
:   _clock(clock),
    _log(log),
    _id(id),
    _cosPin(cosPin),
    _tonePin(tonePin),
    _courtesyType(courtesyType),
    _startTime(_clock.time()) {
}

void StdRx::run() {
}

bool StdRx::isCOS() const {
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
    return cos;
}

bool StdRx::isCTCSS() const {
    bool tone = false;
    if (_toneMode == ToneMode::TONE_EXT_HIGH) {
        // There is a built-in inverter in the hardware
        tone = (gpio_get(_tonePin) == 0);
    } else if (_toneMode == ToneMode::TONE_EXT_LOW) {
        // There is a built-in inverter in the hardware
        tone = (gpio_get(_tonePin) == 1);
    }
    return tone;
}

bool StdRx::isActive() const { 
    return isCOS() && (_toneMode == ToneMode::TONE_IGNORE || isCTCSS());
}

}
