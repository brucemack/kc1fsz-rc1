#ifndef _TestTx_h
#define _TestTx_h

#include "kc1fsz-tools/Clock.h"
#include "Tx.h"

namespace kc1fsz {

class TestTx : public Tx {
public:

    TestTx(Clock& clock);

    virtual void run();

private:

    Clock& _clock;
};

}

#endif
