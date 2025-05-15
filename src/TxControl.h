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

    void _enterIDLE();

    Clock& _clock;
    Log& _log;
    Tx& _tx;

    enum State { INIT, IDLE, VOTING, ACTIVE, ID, URGENT_ID, COURTESY, HANG, LOCKOUT };
    State _state = State::INIT;   

    const static unsigned int _maxRxCount = 4;
    Rx* _rx[_maxRxCount] = { 0, 0, 0, 0 };
    Rx* _activeRx;

    uint32_t _lastIdleTime = 0;
    uint32_t _votingEndTime = 0;
    uint32_t _timeoutTime = 0;
    uint32_t _lockoutEndTime = 0;
    uint32_t _lastCommunicationTime = 0;
    uint32_t _lastIdTime = 0;

    // ----- Configurations 

    // Disabled for now
    uint32_t _votingWindowMs = 0;
    // How long a transmitter is allowed to stay active
    uint32_t _timeoutWindowMs = 1000 * 120;    
    // How long we sleep after a timeout is detected
    uint32_t _lockoutWindowMs = 1000 * 30;    
    // Amount of time that passes in the idle state before we decide the 
    // repeater has gone quiet
    uint32_t _quietWindowMs = 1000 * 5;
    // Time between mandatory IDs
    uint32_t _idRequiredWindowMs = 1000 * 60 * 10; 
};

}

#endif
