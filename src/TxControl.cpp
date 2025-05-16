#include "TxControl.h"

/*


*/


namespace kc1fsz {

TxControl::TxControl(Clock& clock, Log& log, Tx& tx)
:   _clock(clock),
    _log(log),
    _tx(tx)
{
}

void TxControl::setRx(unsigned int i, Rx* rx) {
    if (i < _maxRxCount) {
        _rx[i] = rx;
    }
}

void TxControl::run() { 
    if (_state == State::INIT) {
        _enterIdle();
    }
    else if (_state == State::IDLE) {
        // Check to see if it's time to send the ID from idle. We ID
        // if:
        // 1. Any communication has happened since the last ID
        // 2. It's been more than 10 minutes since the last ID
        // 3. The pause window has passed (to make sure the )
        if (_lastCommunicationTime > _lastIdTime && 
            (_lastIdTime == 0 || _clock.isPast(_lastIdTime + _idRequiredWindowMs)) &&
            _clock.isPast(_lastIdleTime + _quietWindowMs)) {
            _log.info("CWID start");
            _enterId();
        }
        // Check all of the receivers for activity. If anything happens then enter
        // the voting mode to decide which receiver to focus on.
        else if (_anyRxActivity()) {
            _log.info("RX activity seen, voting start");
            _enterVoting();
        }
    }
    // In this state we are sending the CW ID.  Nothing
    // can interrupt.
    else if (_state == State::ID) {
        // Is the tone finished sending?
        if (_idToneGenerator.isFinished()) {
            _log.info("CWID end");
            _enterIDLE();
        }
    }
    // In this state we collect receiver status and decide
    // which receiver to select. 
    else if (_state == State::VOTING) {
        if (_clock.isPast(_votingEndTime)) {
            // TODO: Current implementation is first-come-first-served
            for (unsigned int i = 0; i < _maxRxCount; i++) {
                if (_rx[i] != 0 && _rx[i]->isActive()) {
                    _enterActive(_rx[i]);
                    _log.info("Receiver %d is active", i);
                    break;
                }
            }
        }
    }
    else if (_state == State::ACTIVE) {
        
        // Keep on updating the timestamp
        _lastCommunicationTime = _clock.time();

        // Look for timeout
        if (_clock.isPast(_timeoutTime)) {
            _log.info("Timeout detected, lockout start");
            _enterLockout();
        } 
        // Look for unkey of active receiver.
        else if (!_activeRx->isActive()) {
            _log.info("Quick pause before courtesy tone");
            _enterPreCourtesy();
        }
    }
    // In this state we wait a bit to make sure nobody
    // else is talking and then trigger the courtesy tone.
    else if (_state == State::PRE_COURTESY) {
        if (_clock.isPast(_preCourtesyEndTime))  {
            _log.info("Courtesy tone start");
            _enterCourtesy();
        }
        // Check to see if the previously active receiver
        // has come back (i.e. debounce)
        else if (_activeRx->isActive()) {
            _log.info("RX activity, cancelled courtesy tone");
            _enterActive(_activeRx);
        }
    }
    // In this state we are waiting for the courtesy
    // tone to complete. Nothing can interrupt this.
    else if (_state == State::COURTESY) {
        if (_courtesyTone.isFinished()) {
            _log.info("Hang start");
            _enterHang();
        }
    }
    // In this state we are waiting a bit before allowing
    // the transmitter to drop. This can be interrupted 
    // by another station transmitting.
    else if (_state == State::HANG) {
        if (_clock.isPast(_hangEndTime))  {
            _log.info("Hang end");
            _enterIdle();
        }
        // Any receive activity will end the hang period
        // and will jump back into the voting state.
        else if (_anyRxActivity()) {
            _log.info("RX activity, hang end");
            _enterVoting();
        }
    }
    // In this state we are waiting a defined period of 
    // time, during which nothing can happen. 
    else if (_state == State::LOCKOUT) {
        // Look for end of sleep
        if (_clock.isPast(_lockoutEndTime)) {
            _log.info("Lockout end");
            _enterIDLE();
        }
    }
}

bool TxControl::_anyRxActivity() const {
    for (unsigned int i = 0; i < _maxRxCount; i++) {
        if (_rx[i] != 0 && _rx[i]->isActive()) {
            return true;
        }
    }
    return false;
}

void TxControl::_enterIDLE() {
    _state = State::IDLE;
    _lastIdleTime = _clock.time();
    _tx.setPtt(false);
}

void TxControl::_enterVoting() {
    _state = State::VOTING;
    _votingEndTime = _clock.time() + _votingWindowMs;
}

void TxControl::_enterActive(Rx* rx) {
    _state = State::ACTIVE;
    _activeRx = rx;
    _timeoutTime = _clock.time() + _timeoutWindowMs;
    _tx.setPtt(true);
}

void TxControl::_enterId() {
    _state = State::ID;
    _lastIdTime = _clock.time();
    _tx.setPtt(true);
    _idTone.start();
}

void TxControl::_enterIdUrgent() {
}

void TxControl::_enterActive() {
}

void TxControl::_enterPreCourtesy() {
}

void TxControl::_enterCourtesy() {
    // We no longer give any special treatment to 
    // the previously active receiver.
    _activeRx = 0;
}

void TxControl::_enterLockout() {
    _state = State::LOCKOUT;
    _lockoutEndTime = _clock.time() + _lockoutWindowMs;
    _tx.setPtt(false);
}

}
