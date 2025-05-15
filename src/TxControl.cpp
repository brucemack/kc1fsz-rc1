#include "TxControl.h"

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
        _state = State::IDLE;
    }
    else if (_state == State::IDLE) {
        // Check all of the receivers for activity. If anything happens then enter
        // the voting mode to decide which receiver to focus on.
        for (unsigned int i = 0; i < _maxRxCount; i++) {
            if (_rx[i] != 0 && _rx[i]->isActive()) {
                _state = State::VOTING;
                _votingEndTime = _clock.time() + _votingWindowMs;
                _log.info("Voting");
                break;
            }
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
                    _log.info("Receiver %d is active", i);
                    _tx.setPtt(true);
                    break;
                }
            }

        }
    }
    else if (_state == State::ACTIVE) {
        // Look for timeout
        if (_clock.isPast(_timeoutTime)) {
            _state = State::POST_TIMEOUT_SLEEP;
            _postTimeoutSleepEndTme = _clock.time() + _postTimeoutSleepWindowMs;
            _log.info("Timed out, sleeping");
            _tx.setPtt(false);
        }
    }
    else if (_state == State::POST_TIMEOUT_SLEEP) {
        // Look for end of sleep
        if (_clock.isPast(_postTimeoutSleepEndTme)) {
            _state = State::IDLE;
            _log.info("Sleeping complete");
        }
    }

}

}
