#pragma once

// Keep this layout consistent with the board's linker NVM region.
// The shared STM32F4 NVM implementation requires two equal-size erase sectors.
#define ODRIVE_NVM_SECTOR_A FLASH_SECTOR_10
#define ODRIVE_NVM_SECTOR_A_BASE (const volatile uint8_t*)0x080C0000UL
#define ODRIVE_NVM_SECTOR_A_SIZE 0x20000UL
#define ODRIVE_NVM_SECTOR_B FLASH_SECTOR_11
#define ODRIVE_NVM_SECTOR_B_BASE (const volatile uint8_t*)0x080E0000UL
#define ODRIVE_NVM_SECTOR_B_SIZE 0x20000UL
