#ifndef _Rx_h
#define _Rx_h

#include "kc1fsz-tools/Runnable.h"
#include "CourtesyToneGenerator.h"

namespace kc1fsz {

class Rx : public Runnable {
public:

    virtual void run() = 0;

    virtual int getId() const = 0;

    /**
     * @returns true when the receiver audio is valid. This will depend
     * on some conbination of COS (hard or soft) and CTCSS, depending on 
     * the configuration of the receiver. isActive() factor together 
     * everything.
     */
    virtual bool isActive() const = 0;

    /**
     * @returns true when carrier is detected.  May be hard or soft, 
     * depending on the configuration.
     */
    virtual bool isCOS() const = 0;

    /**
     * @returns true when the CTCSS tone is detected.  May be hard or soft, 
     * depending on the configuration.
     */
    virtual bool isCTCSS() const = 0;

    virtual CourtesyToneGenerator::Type getCourtesyType() const { 
        return CourtesyToneGenerator::Type::FAST_UPCHIRP; 
    }
};

}

#endif
