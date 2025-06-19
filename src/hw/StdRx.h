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

    /**
     * @brief Sets the CTCSS frequency used for 
     * soft decoding.
     * @param toneX10 The tone frequency in tenths of Hertz.
     */
    void setTone(int toneX10) { _toneX10 = toneX10; }

    enum CosMode {
        COS_EXT_LOW, COS_EXT_HIGH, COS_SOFT
    };

    void setCosMode(CosMode mode) { _cosMode = mode; }

    /**
     * @brief Controls how the CTCSS tone decode works.
     */
    enum ToneMode {
        TONE_NONE, TONE_EXT_LOW, TONE_EXT_HIGH, TONE_SOFT
    };

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
    ToneMode _toneMode = ToneMode::TONE_NONE;
};

}

#endif
