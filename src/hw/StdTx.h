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

    virtual void setPtt(bool ptt);
    virtual void run();

    enum ToneMode { NONE, SOFT };

    void setToneMode(ToneMode mode) { 
        _toneMode = mode; 
        if (_toneMode == ToneMode::SOFT) {
            _plSynth.setEnabled(true);
        } else {
            _plSynth.setEnabled(false);
        }
    }

    void setTone(int toneX10) { 
        _toneX10 = toneX10;
        _plSynth.setFreq(((float)_toneX10) / 10.0); 
    }

private:

    Clock& _clock;
    Log& _log;
    const int _id;
    const int _pttPin;
    ToneSynthesizer& _plSynth;

    bool _keyed = false;
    int _toneX10 = 0;
    ToneMode _toneMode = ToneMode::NONE;
};

}

#endif
