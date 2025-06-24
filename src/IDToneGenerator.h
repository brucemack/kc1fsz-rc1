#ifndef _IDToneGenerator_h
#define _IDToneGenerator_h

#include "kc1fsz-tools/Log.h"
#include "kc1fsz-tools/Clock.h"
#include "kc1fsz-tools/ToneSynthesizer.h"

#include "ToneGenerator.h"

namespace kc1fsz {

class Config;

class IDToneGenerator : public ToneGenerator {
public:

    IDToneGenerator(Log& log, Clock& clock, ToneSynthesizer& synth, const Config& config);

    virtual void run();

    virtual void start();
    virtual bool isFinished();

private:

    Log& _log;
    Clock& _clock;
    ToneSynthesizer& _synth;
    const Config& _config;

    bool _running = false;
    uint32_t _endTime = 0;
    unsigned int _state = 0;
    unsigned int _callPtr = 0;
    unsigned int _symPtr = 0;
};

}

#endif
