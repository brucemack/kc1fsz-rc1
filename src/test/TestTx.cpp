#include "TestTx.h"

namespace kc1fsz {

TestTx::TestTx(Clock& clock, Log& log, int id) 
:   _clock(clock),
    _log(log),
    _id(id) {
}

void TestTx::setPtt(bool ptt) {
    if (ptt != _keyed)
        if (ptt) {
            _log.info("Transmitter keyed [%d]", _id);
        } else {
            _log.info("Transmitter unkeyed [%d]", _id);

        }
    _keyed = ptt;
}

void TestTx::run() {   
}

}