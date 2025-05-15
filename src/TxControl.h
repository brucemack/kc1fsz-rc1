#ifndef _TxControl_h
#define _TxControl_h

#include "kc1fsz-tools/Log.h"
#include "kc1fsz-tools/Clock.h"
#include "Tx.h"
#include "Rx.h"

namespace kc1fsz {

class TxControl {
public:

    TxControl(Clock& clock, Log& log, Tx& tx);

    virtual void run();

    /**
     * Makes a receiver visible to the transmitter.
     */
    void setRx(unsigned int i, Rx* rx);

private:

    Clock& _clock;
    Log& _log;
    Tx& _tx;

    enum State { INIT, IDLE, VOTING, ACTIVE, ID, URGENT_ID, COURTESY, HANG, POST_TIMEOUT_SLEEP };
    State _state = State::INIT;   

    const static unsigned int _maxRxCount = 4;
    Rx* _rx[_maxRxCount] = { 0, 0, 0, 0 };
    Rx* _activeRx;

    uint32_t _votingEndTime;
    uint32_t _timeoutTime;
    uint32_t _postTimeoutSleepEndTme;

    // ----- Configurations 

    // Disabled for now
    uint32_t _votingWindowMs = 0;
    // How long a transmitter is allowed to stay active
    //uint32_t _timeoutWindowMs = 1000 * 120;    
    uint32_t _timeoutWindowMs = 1000 * 10;    
    // How long we sleep after a timeout is detected
    //uint32_t _postTimeoutSleepWindowMs = 1000 * 30;    
    uint32_t _postTimeoutSleepWindowMs = 1000 * 3;    
};

}

#endif
