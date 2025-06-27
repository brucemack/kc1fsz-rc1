/**
 * Software Defined Repeater Controller
 * Copyright (C) 2025, Bruce MacKinnon KC1FSZ
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * NOT FOR COMMERCIAL USE WITHOUT PERMISSION.
 */
#ifndef _StdTx_h
#define _StdTx_h

#include "kc1fsz-tools/Log.h"
#include "kc1fsz-tools/Clock.h"
#include "kc1fsz-tools/ToneSynthesizer.h"
#include "Tx.h"

namespace kc1fsz {

class StdTx : public Tx {
public:

    StdTx(Clock& clock, Log& log, int id, int pttPin,
        ToneSynthesizer& plSynth);

    virtual void run();

    virtual void setPtt(bool ptt);
    virtual bool getPtt() const;

    void setToneMode(ToneMode mode) { 
        _toneMode = mode; 
        if (_toneMode == ToneMode::SOFT) {
            _plSynth.setEnabled(true);
        } else {
            _plSynth.setEnabled(false);
        }
    }

    void setToneFreq(float hz) { 
        _toneFreq = hz;
        _plSynth.setFreq(_toneFreq); 
    }

    void setToneLevel(float lvl) {
        _toneLevel = lvl;
    }

private:

    Clock& _clock;
    Log& _log;
    const int _id;
    const int _pttPin;
    ToneSynthesizer& _plSynth;

    bool _keyed = false;

    // Configuration
    ToneMode _toneMode = ToneMode::NONE;
    float _toneFreq = 0;
    float _toneLevel = 0;
};

}

#endif
