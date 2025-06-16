#ifndef _CourtesyToneGenerator_h
#define _CourtesyToneGenerator_h

#include "kc1fsz-tools/Log.h"
#include "kc1fsz-tools/Clock.h"
#include "kc1fsz-tools/ToneSynthesizer.h"

#include "ToneGenerator.h"

namespace kc1fsz {

    class CourtesyToneGenerator : public ToneGenerator {
public:

    CourtesyToneGenerator(Log& log, Clock& clock, ToneSynthesizer& synth);

    virtual void run();
    virtual void start();
    virtual bool isFinished();

private:

    Log& _log;
    Clock& _clock;
    ToneSynthesizer& _synth;

    bool _running = false;
    uint32_t _endTime = 0;
};

}

#endif