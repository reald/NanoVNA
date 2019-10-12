#include <stdint.h>
#include "nanovna.h"

#if defined(__ICCARM__)

#pragma section = "FLASH_CONFIG"
const size_t __flash_config_size_block = __section_size("FLASH_CONFIG");
const size_t __flash_config_size_page = 0x800;
__root __no_init const config_t     __flash_config_main     @ "FLASH_CONFIG_MAIN";
__root __no_init const properties_t __flash_config_save0    @ "FLASH_CONFIG_SAVE0";
__root __no_init const properties_t __flash_config_save1    @ "FLASH_CONFIG_SAVE1";
__root __no_init const properties_t __flash_config_save2    @ "FLASH_CONFIG_SAVE2";
__root __no_init const properties_t __flash_config_save3    @ "FLASH_CONFIG_SAVE3";
__root __no_init const properties_t __flash_config_save4    @ "FLASH_CONFIG_SAVE4";


#elif defined(__GNUC__) || defined(__DOXYGEN__)

const size_t __flash_config_size_block = 0x8000;
const size_t __flash_config_size_page = 0x800;
const config_t     __flash_config_main  __attribute__((section("FLASH_CONFIG_MAIN")));
const properties_t __flash_config_save0 __attribute__((section("FLASH_CONFIG_SAVE0")));
const properties_t __flash_config_save1 __attribute__((section("FLASH_CONFIG_SAVE1")));
const properties_t __flash_config_save2 __attribute__((section("FLASH_CONFIG_SAVE2")));
const properties_t __flash_config_save3 __attribute__((section("FLASH_CONFIG_SAVE3")));
const properties_t __flash_config_save4 __attribute__((section("FLASH_CONFIG_SAVE4")));

#else

const size_t __flash_config_size_block = 0x8000;
const size_t __flash_config_size_page = 0x800;
const config_t     __flash_config_main     @ 0x08018000;
const properties_t __flash_config_save0    @ 0x08018800;
const properties_t __flash_config_save1    @ 0x0801a000;
const properties_t __flash_config_save2    @ 0x0801b800;
const properties_t __flash_config_save3    @ 0x0801d000;
const properties_t __flash_config_save4    @ 0x0801e800;

#endif
