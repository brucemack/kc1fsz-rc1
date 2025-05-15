#ifndef _TestTx_h
#define _TestTx_h

#include "kc1fsz-tools/Log.h"
#include "kc1fsz-tools/Clock.h"
#include "Tx.h"

namespace kc1fsz {

class TestTx : public Tx {
public:

    TestTx(Clock& clock, Log& log);

    virtual void setPtt(bool ptt);
    virtual void run();

private:

    Clock& _clock;
    Log& _log;
    bool _keyed = false;
};

}

#endif
