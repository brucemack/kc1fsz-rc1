/**
 * Digital Repeater Controller
 * Copyright (C) 2025, Bruce MacKinnon KC1FSZ
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * NOT FOR COMMERCIAL USE WITHOUT PERMISSION.
 */
#ifndef _AudioSourceControl_h
#define _AudioSourceControl_h

#include <cstdio>
#include "kc1fsz-tools/Runnable.h"
#include "kc1fsz-tools/Clock.h"

namespace kc1fsz {

class AudioSourceControl : public Runnable {
public:

    AudioSourceControl(Clock& clock, float fadeMs) 
    :  _clock(clock), 
       _fadeMs(fadeMs) { }

    enum Source { SILENT, RADIO0, RADIO1 };

    void setSource(Source source) { 
        _source = source; 
        _fadeEnv = 0;
        _fadeStart = _clock.time();
    }

    Source getSource() const { return _source; }

    float getFade() { return _fadeEnv; }

    virtual void run() { 
        // Fade up if necessary
        if (_fadeEnv < 1.0) {
            uint32_t elapsed = _clock.time() - _fadeStart;
            _fadeEnv = (float)elapsed / (float)_fadeMs;
            if (_fadeEnv > 1.0)
                _fadeEnv = 1.0;
        }
    }

private:

    const Clock& _clock;
    const uint32_t _fadeMs;

    volatile Source _source = Source::SILENT;
    float _fadeEnv = 0.0;
    uint32_t _fadeStart = 0;
};

}

#endif
