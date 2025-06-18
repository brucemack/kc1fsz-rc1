#ifndef _TxControl_h
#define _TxControl_h

#include "kc1fsz-tools/Log.h"
#include "kc1fsz-tools/Clock.h"
#include "kc1fsz-tools/ToneSynthesizer.h"

#include "Tx.h"
#include "Rx.h"
#include "CourtesyToneGenerator.h"
#include "IDToneGenerator.h"

namespace kc1fsz {

class TxControl {
public:

    TxControl(Clock& clock, Log& log, Tx& tx, ToneSynthesizer& toneSynth);

    virtual void run();

    /**
     * Makes a receiver visible to the transmitter.
     */
    void setRx(unsigned int i, Rx* rx);

    void forceId() {
        _enterPreId();
    }

private:

    enum State { INIT, IDLE, VOTING, ACTIVE, PRE_ID, ID, POST_ID, ID_URGENT, PRE_COURTESY, COURTESY, HANG, LOCKOUT };

    void _setState(State state, uint32_t timeoutWindowMs = 0);
    bool _isStateTimedOut() const;

    /**
     * @returns An indication of whether an ID needs to be sent now. This
     * is slightly dependent on whether a QSO is active since we can have
     * a grace period.
     */
    bool _isIdRequired(bool qsoActive) const;

    void _enterIdle();
    void _enterVoting();
    void _enterActive(Rx* rx);
    void _enterPreId();
    void _enterId();
    void _enterPostId();
    void _enterIdUrgent();
    void _enterPreCourtesy();
    void _enterCourtesy();
    void _enterHang();
    void _enterLockout();
    bool _anyRxActivity() const;

    Clock& _clock;
    Log& _log;
    Tx& _tx;

    State _state = State::INIT;   

    const static unsigned int _maxRxCount = 4;
    Rx* _rx[_maxRxCount] = { 0, 0, 0, 0 };
    Rx* _activeRx;

    CourtesyToneGenerator _courtesyToneGenerator;
    IDToneGenerator _idToneGenerator;

    uint32_t _lastIdleTime = 0;
    uint32_t _timeoutTime = 0;
    uint32_t _lastCommunicationTime = 0;
    uint32_t _lastIdTime = 0;

    uint32_t _currentStateEndTime = 0;
    //void (TxControl::*_currentStateTimeoutFuncPtr)() = 0;

    // ----- Configurations 

    // Disabled for now
    uint32_t _votingWindowMs = 25;
    // How long between the end of transmission and the courtesy tone
    uint32_t _preCourtseyWindowMs = 1500;    
    // How long we pause with the transmitter keyed before sending the CWID
    uint32_t _preIdWindowMs = 1000;    
    // How long we pause with the transmitter keyed after sending the CWID
    uint32_t _postIdWindowMs = 1000;    
    // How long a transmitter is allowed to stay active
    //uint32_t _timeoutWindowMs = 1000 * 120;    
    //TEMP
    uint32_t _timeoutWindowMs = 1000 * 20;    
    // How long we sleep after a timeout is detected
    //uint32_t _lockoutWindowMs = 1000 * 60;
    //TEMP
    uint32_t _lockoutWindowMs = 1000 * 20;
    // Length of hang interval
    uint32_t _hangWindowMs = 1000 * 2;
    // Amount of time that passes in the idle state before we decide the 
    // repeater has gone quiet
    uint32_t _quietWindowMs = 1000 * 5;
    // Time between mandatory IDs
    uint32_t _idRequiredWindowMs = 1000 * 60 * 10; 
    // Length of the grace period before we raise an urgent ID
    uint32_t _idGraceWindowMs = 1000 * 15;
};

}

#endif
