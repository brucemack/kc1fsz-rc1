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
#include <cstdio>
#include <cstring>

#include "kc1fsz-tools/Common.h"
#include "Config.h"
#include "ShellCommand.h"

namespace kc1fsz {

static const char* INVALID_COMMAND = "Invalid Command\n";

void ShellCommand::process(const char* cmd) {
    // Tokenize
    const unsigned int maxTokenCount = 4;
    const unsigned int maxTokenLen = 32;
    char tokens[maxTokenCount][maxTokenLen];
    int tokenCount = 0;
    int tokenPtr = 0;
    for (int i = 0; i <= strlen(cmd) && tokenCount < maxTokenCount; i++) {
        // Space is the inter-token delimiter
        if (cmd[i] == ' ' || cmd[i] == 0) {
            // Leading spaces are ignored
            if (tokenPtr > 0)
                tokenCount++;
            tokenPtr = 0;
        } else {
            if (tokenPtr < maxTokenLen - 1) {
                tokens[tokenCount][tokenPtr++] = cmd[i];
                tokens[tokenCount][tokenPtr] = 0;
            }
        }
    }

    if (tokenCount == 1) {
        if (strcmp(tokens[0], "reset") == 0) {
            printf("Reboot requested");
            // The watchdog will take over from here
            while (true);            
        }
        else if (strcmp(tokens[0], "ping") == 0) {
            printf("pong\n");
        }
        else if (strcmp(tokens[0], "show") == 0) {
            Config::show(&_config);
        }
    }
    else if (tokenCount == 3) {
        if (strcmp(tokens[0], "set") == 0) {
            if (strcmp(tokens[1], "call") == 0) {
                strcpyLimited(_config.general.callSign, tokens[2], Config::callSignMaxLen);
            } else if (strcmp(tokens[1], "pass") == 0) {
                strcpyLimited(_config.general.pass, tokens[2], Config::passMaxLen);
            } else if (strcmp(tokens[1], "repeatmode") == 0) {
                _config.general.repeatMode = atoi(tokens[2]);
            } else {
                printf(INVALID_COMMAND);
            }
        }
        else {
            printf(INVALID_COMMAND);
        }
    }
    else if (tokenCount == 4) {
        if (strcmp(tokens[0], "set") == 0)
            if (strcmp(tokens[1], "cosmode") == 0)
                if (strcmp(tokens[2], "0") == 0)
                    _config.rx0.cosMode = atoi(tokens[3]);
                else if (strcmp(tokens[2], "1") == 0)
                    _config.rx1.cosMode = atoi(tokens[3]);
                else 
                    printf(INVALID_COMMAND);
            else if (strcmp(tokens[1], "cosactivetime") == 0)
                if (strcmp(tokens[2], "0") == 0)
                    _config.rx0.cosActiveTime = atoi(tokens[3]);
                else if (strcmp(tokens[2], "1") == 0)
                    _config.rx1.cosActiveTime = atoi(tokens[3]);
                else 
                    printf(INVALID_COMMAND);
            else if (strcmp(tokens[1], "cosinactivetime") == 0)
                if (strcmp(tokens[2], "0") == 0)
                    _config.rx0.cosInactiveTime = atoi(tokens[3]);
                else if (strcmp(tokens[2], "1") == 0)
                    _config.rx1.cosInactiveTime = atoi(tokens[3]);
                else 
                    printf(INVALID_COMMAND);                
            else if (strcmp(tokens[1], "coslevel") == 0)
                if (strcmp(tokens[2], "0") == 0)
                    _config.rx0.cosLevel =  Config::dbToLinear(atof(tokens[3]));
                else if (strcmp(tokens[2], "1") == 0)
                    _config.rx1.cosLevel = Config::dbToLinear(atof(tokens[3]));
                else 
                    printf(INVALID_COMMAND);                
        else
            printf(INVALID_COMMAND);
    }
}
}
