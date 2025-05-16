#ifndef _TestRx_h
#define _TestRx_h

#include "kc1fsz-tools/Log.h"
#include "kc1fsz-tools/Clock.h"
#include "Rx.h"

namespace kc1fsz {

class TestRx : public Rx {
public:

    TestRx(Clock& clock, Log& log, int id);

    virtual int getId() const { return _id; }
    virtual void run();
    virtual bool isActive() const { return _active; }

private:

    Clock& _clock;
    Log& _log;
    uint32_t _startTime;
    bool _active = false;
    unsigned int _state = 0;
    int _id;
};

}

#endif
