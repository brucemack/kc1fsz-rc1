#ifndef _IDToneGenerator_h
#define _IDToneGenerator_h

#include "kc1fsz-tools/Log.h"
#include "kc1fsz-tools/Clock.h"
#include "ToneGenerator.h"

namespace kc1fsz {

    class IDToneGenerator : public ToneGenerator {
public:

    IDToneGenerator(Log& log, Clock& clock);

    virtual void run();
    virtual void start();
    virtual bool isFinished();

private:

    Log& _log;
    Clock& _clock;
};

}

#endif
