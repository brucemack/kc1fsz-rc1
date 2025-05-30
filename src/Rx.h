#ifndef _Rx_h
#define _Rx_h

#include "kc1fsz-tools/Runnable.h"

namespace kc1fsz {

class Rx : public Runnable {
public:

    virtual int getId() const = 0;
    virtual void run() = 0;
    virtual bool isActive() const = 0;

private:

};

}

#endif
