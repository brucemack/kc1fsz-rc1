#include "TestTx.h"

namespace kc1fsz {

TestTx::TestTx(Clock& clock, Log& log) 
:   _clock(clock),
    _log(log) {
}

void TestTx::setPtt(bool ptt) {
    if (ptt) {
        _log.info("Transmitter keyed");
    } else {
        _log.info("Transmitter unkeyed");

    }
}

void TestTx::run() {   
}

}