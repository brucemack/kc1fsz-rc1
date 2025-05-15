#ifndef _Tx_h
#define _Tx_h

#include "kc1fsz-tools/Runnable.h"

namespace kc1fsz {

class Tx : public Runnable {
public:

    virtual void setPtt(bool ptt) = 0;
    virtual void run() = 0;

private:

};

}

#endif
