#ifndef _TestRx_h
#define _TestRx_h

#include "kc1fsz-tools/Clock.h"
#include "Rx.h"

namespace kc1fsz {

class TestRx : public Rx {
public:

    TestRx(Clock& clock);

    virtual void run();
    virtual bool isActive() const { return _active; }

private:

    Clock& _clock;
    uint32_t _startTime;
    bool _active = false;
};

}

#endif
