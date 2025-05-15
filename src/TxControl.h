#ifndef _TxControl_h
#define _TxControl_h

#include "kc1fsz-tools/Clock.h"
#include "Tx.h"

namespace kc1fsz {

class TxControl {
public:

    TxControl(Clock& clock, Tx& tx);

    virtual void run();

private:

    Clock& _clock;
    Tx& _tx;

    enum State { INIT, IDLE, ACTIVE, ID, URGENT_ID, COURTESY, HANG, TIMED_OUT };
    State _state = State::INIT;   
};

}

#endif
