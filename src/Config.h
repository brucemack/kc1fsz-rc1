/**
 * Software Defined Repeater Controller
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
#ifndef _Config_h
#define _Config_h

#include <cstdint>
#include <cmath>

namespace kc1fsz {

/**
 * @brief The master application configuration structure.
 */
struct Config {

    const static int CONFIG_VERSION = 0xbabe + 1;
    const static int CONFIG_SIZE = 512;

    const static int callSignMaxLen = 16;
    const static int passMaxLen = 16;

    int magic;

    struct GeneralConfig {
        char callSign[callSignMaxLen]; 
        char pass[passMaxLen];
        uint32_t repeatMode;
    } general;

    struct ReceiverConfig {
        uint32_t cosMode;
        uint32_t cosActiveTime;
        uint32_t cosInactiveTime;
        float cosLevel;
        uint32_t toneMode;
        uint32_t toneActiveTime;
        uint32_t toneInactiveTime;
        float toneLevel;
        float toneFreq;
        float gain;
    } rx0, rx1;

    struct TransmitterConfig {
        uint32_t toneMode;
        float toneLevel;
        float toneFreq;
    } tx0, tx1;

    struct ControlConfig {
        uint32_t timeoutTime;
        uint32_t lockoutTime;
        uint32_t hangTime;
        uint32_t ctMode;
        float ctLevel;
        float idLevel; 
    } txc0, txc1;

    char pad[CONFIG_SIZE - 
        4 + 
        sizeof(GeneralConfig) + 
        2 * sizeof(ReceiverConfig) +
        2 * sizeof(TransmitterConfig) +
        2 * sizeof(ControlConfig)
        ];

    bool isValid() { return magic == CONFIG_VERSION; }
    
    static void saveConfig(const Config* cfg);
    static void loadConfig(Config* cfg);
    static void setFactoryDefaults(Config* cfg);
    static void show(const Config* cfg);
    static float dbToLinear(float db) { return pow(10, (db / 20)); } 
};

}

#endif
