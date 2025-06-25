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

    void setCosMode(CosMode mode) { _cosMode = mode; }
    void setCosActiveTime(unsigned ms) { _cosActiveTime = ms; }
    void setCosInactiveTime(unsigned ms) { _cosInactiveTime = ms; }
    void setCosLevel(float lvl) { _cosLevel = lvl; }
    void setToneMode(ToneMode mode) { _toneMode = mode; }
    void setToneActiveTime(unsigned ms) { _toneActiveTime = ms; }
    void setToneInactiveTime(unsigned ms) { _toneInactiveTime = ms; }
    void setToneLevel(float lvl) { _toneLevel = lvl; }
    void setToneFreq(float hz) { _toneFreq = hz; }
    void setGain(float lvl) { _gain = lvl; }

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

    CosMode _cosMode = CosMode::COS_EXT_HIGH;
    uint32_t _cosActiveTime = 0;
    uint32_t _cosInactiveTime = 0;
    float _cosLevel = 0.0;

    ToneMode _toneMode = ToneMode::TONE_IGNORE;
    uint32_t _toneActiveTime = 0;
    uint32_t _toneInactiveTime = 0;
    float _toneFreq = 0;
    float _toneLevel = 1.0;
    float _gain = 1.0;
};

}

#endif
