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
