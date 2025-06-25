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
    strcpyLimited(cfg->general.callSign, "W1TKZ", Config::callSignMaxLen);
    strcpyLimited(cfg->general.pass, "781", Config::passMaxLen);
    cfg->general.repeatMode = 2;
}

void Config::show(const Config* cfg) {

    // General configuration
    printf("callsign    : %s\n", cfg->general.callSign);

    // Receiver configuration
    printf("R0 cosmode  : %d\n", cfg->rx0.cosMode);
}

}



