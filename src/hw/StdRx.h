#ifndef _StdRx_h
#define _StdRx_h

#include "kc1fsz-tools/Log.h"
#include "kc1fsz-tools/Clock.h"
#include "Rx.h"

namespace kc1fsz {

class StdRx : public Rx {
public:

    StdRx(Clock& clock, Log& log, int id, int cosPin, int tonePin, 
        CourtesyToneGenerator::Type courtesyType);

    virtual int getId() const { return _id; }
    virtual void run();
    virtual bool isActive() const;
    virtual bool isCOS() const;
    virtual bool isCTCSS() const;

    /**
     * @brief Sets the CTCSS frequency used for 
     * soft decoding.
     * @param toneX10 The tone frequency in tenths of Hertz.
     */
    void setToneFreq(float hz) { _toneX10 = toneX10; }

    void setCosMode(CosMode mode) { _cosMode = mode; }

    void setToneMode(ToneMode mode) { _toneMode = mode; }

    virtual CourtesyToneGenerator::Type getCourtesyType() const { 
        return _courtesyType;
    }


private:

    Clock& _clock;
    Log& _log;
    const int _id;
    const int _cosPin;
    const int _tonePin;
    const CourtesyToneGenerator::Type _courtesyType;

    uint32_t _startTime;
    bool _active = false;
    unsigned int _state = 0;
    int _toneX10 = 0;
    CosMode _cosMode = CosMode::COS_EXT_HIGH;
    ToneMode _toneMode = ToneMode::TONE_IGNORE;
};

}

#endif
