#ifndef _CourtesyToneGenerator_h
#define _CourtesyToneGenerator_h

#include "kc1fsz-tools/Log.h"
#include "kc1fsz-tools/Clock.h"
#include "kc1fsz-tools/ToneSynthesizer.h"

#include "ToneGenerator.h"

namespace kc1fsz {

class CourtesyToneGenerator : public ToneGenerator {
public:

    enum Type { FAST_UPCHIRP, FAST_DOWNCHIRP };

    CourtesyToneGenerator(Log& log, Clock& clock, ToneSynthesizer& synth);

    virtual void run();
    virtual void start();
    virtual bool isFinished();
    void setType(Type type) { _type = type; }

private:

    Log& _log;
    Clock& _clock;
    ToneSynthesizer& _synth;

    unsigned int _chirpMs = 50;
    bool _running = false;
    Type _type = Type::FAST_DOWNCHIRP;
    int _part = 0;
    uint32_t _endTime = 0;
};

}

#endif