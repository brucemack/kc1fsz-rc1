#include <hardware/gpio.h>

#include "StdTx.h"

namespace kc1fsz {

StdTx::StdTx(Clock& clock, Log& log, int id, int pttPin,
    ToneSynthesizer& plSynth) 
:   _clock(clock),
    _log(log),
    _id(id),
    _pttPin(pttPin),
    _plSynth(plSynth) {
}

void StdTx::setPtt(bool ptt) {
    if (ptt != _keyed)
        if (ptt) {
            gpio_put(_pttPin, 1);
            _log.info("Transmitter keyed [%d]", _id);
        } else {
            gpio_put(_pttPin, 0);
            _log.info("Transmitter unkeyed [%d]", _id);
        }
    _keyed = ptt;
}

bool StdTx::getPtt() const {
    return _keyed;
}

void StdTx::run() {   
}

}