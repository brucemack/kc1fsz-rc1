#ifndef _Rx_h
#define _Rx_h

#include "kc1fsz-tools/Runnable.h"
#include "CourtesyToneGenerator.h"

namespace kc1fsz {

class Rx : public Runnable {
public:

    virtual void run() = 0;

    virtual int getId() const = 0;
    virtual bool isActive() const = 0;
    virtual CourtesyToneGenerator::Type getCourtesyType() const { 
        return CourtesyToneGenerator::Type::FAST_UPCHIRP; 
    }
};

}

#endif
