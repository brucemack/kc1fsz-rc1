#include "Config.h"

#include <cstring>

#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/irq.h"
#include "hardware/sync.h"

#include "kc1fsz-tools/Common.h"

namespace kc1fsz {

void Config::saveConfig(const Config* cfg) {
    uint32_t ints = save_and_disable_interrupts();
    // Must erase a full sector first (4096 bytes)
    flash_range_erase((PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE), FLASH_SECTOR_SIZE);
    // IMPORTANT: Must be a multiple of 256!
    flash_range_program((PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE), (uint8_t*)cfg, 
        Config::CONFIG_SIZE);
    restore_interrupts(ints);
}

void Config::loadConfig(Config* cfg) {
    // The very last sector of flash is used. Compute the memory-mapped address, 
    // remembering to include the offset for RAM
    const uint8_t* addr = (uint8_t*)(XIP_BASE + (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE));
    memcpy((void*)cfg, (const void*)addr, Config::CONFIG_SIZE);
}

void Config::setFactoryDefaults(Config* cfg) {
    cfg->magic = CONFIG_VERSION;

    // General
    strcpyLimited(cfg->general.callSign, "W1TKZ", Config::callSignMaxLen);
    strcpyLimited(cfg->general.pass, "781", Config::passMaxLen);
    cfg->general.repeatMode = 2;

    // Receiver
    cfg->rx0.cosMode = 2;
    cfg->rx0.cosActiveTime = 25;
    cfg->rx0.cosInactiveTime = 25;
    cfg->rx0.cosLevel = dbToLinear(-20);
    cfg->rx0.toneMode = 2;
    cfg->rx0.toneActiveTime = 25;
    cfg->rx0.toneInactiveTime = 25;
    cfg->rx0.toneLevel = dbToLinear(-10);
    cfg->rx0.toneFreq = 700;
    cfg->rx0.gain = dbToLinear(0);
    cfg->rx1 = cfg->rx0;

    // Transmitter
    cfg->tx0.toneMode = 0;
    cfg->tx0.toneFreq = 0;
    cfg->tx0.toneLevel = 0;
    cfg->tx1.toneMode = 1;
    cfg->tx1.toneFreq = 88.5;
    cfg->tx1.toneLevel = dbToLinear(-16);

    cfg->tx1 = cfg->tx0;

    // Controller
    cfg->txc0.timeoutTime = 120 * 1000;
    cfg->txc0.lockoutTime = 60 * 1000;
    cfg->txc0.hangTime = 1500;
    cfg->txc0.ctMode = 2;
    cfg->txc0.ctLevel = dbToLinear(-10);
    cfg->txc0.idLevel = dbToLinear(-10);
    cfg->txc1 = cfg->txc0;

}

void Config::show(const Config* cfg) {

    // General configuration
    printf("callsign    : %s\n", cfg->general.callSign);
    printf("pass        : %s\n", cfg->general.pass);
    printf("repeatmode  : %d\n", cfg->general.repeatMode);

    // Receiver configuration
    printf("R0 cosmode  : %d\n", cfg->rx0.cosMode);
}

}



