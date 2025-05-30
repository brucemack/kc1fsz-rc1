#ifndef _StdTx_h
#define _StdTx_h

#include "kc1fsz-tools/Log.h"
#include "kc1fsz-tools/Clock.h"
#include "Tx.h"

namespace kc1fsz {

class StdTx : public Tx {
public:

    StdTx(Clock& clock, Log& log, int id, int pttPin);

    virtual void setPtt(bool ptt);
    virtual void run();

    enum ToneMode { NONE, SOFT };

    void setToneMode(ToneMode mode) { _toneMode = mode; }

    void setTone(int toneX10) { _toneX10 = toneX10; }

private:

    Clock& _clock;
    Log& _log;
    bool _keyed = false;
    int _id;
    int _toneX10 = 0;
    int _pttPin;
    ToneMode _toneMode = ToneMode::NONE;
};

}

#endif
