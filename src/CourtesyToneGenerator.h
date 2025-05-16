#ifndef _CourtesyToneGenerator_h
#define _CourtesyToneGenerator_h

#include "kc1fsz-tools/Log.h"
#include "kc1fsz-tools/Clock.h"
#include "ToneGenerator.h"

namespace kc1fsz {

    class CourtesyToneGenerator : public ToneGenerator {
public:

    CourtesyToneGenerator(Log& log, Clock& clock);

    virtual void run();
    virtual void start();
    virtual bool isFinished();

private:

    Log& _log;
    Clock& _clock;
};

}

#endif