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
        _enterIDLE();
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
            _state = State::ID;
            _lastIdTime = _clock.time();
            _tx.setPtt(true);
            _log.info("Sending CWID");
        }
        else {
            // Check all of the receivers for activity. If anything happens then enter
            // the voting mode to decide which receiver to focus on.
            for (unsigned int i = 0; i < _maxRxCount; i++) {
                if (_rx[i] != 0 && _rx[i]->isActive()) {
                    _state = State::VOTING;
                    _votingEndTime = _clock.time() + _votingWindowMs;
                    _log.info("COS/CTCSS detected, voting");
                    break;
                }
            }
        }
    }
    else if (_state == State::ID) {
        // TODO: FIGURE OUT WHEN ID IS COMPLETE
        if (_clock.isPast(_lastIdTime + 5000)) {
            _enterIDLE();
        }
    }
    else if (_state == State::VOTING) {
        // During the voting period we collect receiver signal strength 
        if (_clock.isPast(_votingEndTime)) {
            // TODO: Current implementation is first-come-first-served
            for (unsigned int i = 0; i < _maxRxCount; i++) {
                if (_rx[i] != 0 && _rx[i]->isActive()) {
                    _state = State::ACTIVE;
                    _activeRx = _rx[i];
                    _timeoutTime = _clock.time() + _timeoutWindowMs;
                    _tx.setPtt(true);
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
            _state = State::LOCKOUT;
            _lockoutEndTime = _clock.time() + _lockoutWindowMs;
            _tx.setPtt(false);
            _log.info("Timed out, entering lock out");
        } 
        // Look for unkey
        else if (!_activeRx->isActive()) {
            _enterIDLE();
        }
    }
    else if (_state == State::LOCKOUT) {
        // Look for end of sleep
        if (_clock.isPast(_lockoutEndTime)) {
            _enterIDLE();
            _log.info("Lockout complete");
        }
    }

}

void TxControl::_enterIDLE() {
    _state = State::IDLE;
    _lastIdleTime = _clock.time();
    _tx.setPtt(false);
}

}
