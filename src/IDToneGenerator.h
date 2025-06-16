#ifndef _IDToneGenerator_h
#define _IDToneGenerator_h

#include "kc1fsz-tools/Log.h"
#include "kc1fsz-tools/Clock.h"
#include "kc1fsz-tools/ToneSynthesizer.h"

#include "ToneGenerator.h"

namespace kc1fsz {

    class IDToneGenerator : public ToneGenerator {
public:

    IDToneGenerator(Log& log, Clock& clock, ToneSynthesizer& synth);

    virtual void run();
    virtual void start();
    virtual bool isFinished();

private:

    static const char _id[];

    Log& _log;
    Clock& _clock;
    ToneSynthesizer& _synth;
    
    bool _running = false;
    uint32_t _endTime = 0;
    unsigned int _idPtr = 0;
};

}

#endif
