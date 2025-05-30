#include "StdTx.h"

namespace kc1fsz {

StdTx::StdTx(Clock& clock, Log& log, int id, int pttPin) 
:   _clock(clock),
    _log(log),
    _id(id),
    _pttPin(pttPin) {
}

void TestTx::setPtt(bool ptt) {
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

void TestTx::run() {   
}

}