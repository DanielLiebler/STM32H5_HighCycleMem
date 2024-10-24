
#ifndef FLASH_H
#define FLASH_H
#include "stm32h563.h"

#define FLASH_PAGE_SIZE         FLASH_SECTOR_SIZE
#define FLASH_PAGES_PER_BANK    128

// static bank size, since original define is probably evaluated at runtime
#define FLASH_BANK_SIZE_STATIC  (FLASH_PAGES_PER_BANK * FLASH_PAGE_SIZE)
#define FLASH_START_BANK1       (0x08000000)
#define FLASH_END_BANK1         (FLASH_START_BANK1 + FLASH_BANK_SIZE_STATIC - 1)
#define FLASH_START_BANK2       (0x08100000)
#define FLASH_END_BANK2         (FLASH_START_BANK2 + FLASH_BANK_SIZE_STATIC - 1)

#define FLASH_PAGE_OFFSET_BANK2 FLASH_PAGES_PER_BANK

/*the offset between high cyclic sector numbers and corresponding normal flash numbers*/
#define HIGH_CYCLIC_PAGE_OFFSET 120

#define HIGH_CYCLIC_START_BANK1 0x09000000UL
#define HIGH_CYCLIC_START_BANK2 0x0900C000UL
#define HIGH_CYCLIC_SECTOR_SIZE 0x00001800UL
#define HIGH_CYCLIC_END_BANK1   (HIGH_CYCLIC_START_BANK1 + 8*HIGH_CYCLIC_SECTOR_SIZE - 1)
#define HIGH_CYCLIC_END_BANK2   (HIGH_CYCLIC_START_BANK2 + 8*HIGH_CYCLIC_SECTOR_SIZE - 1)

extern void flash_erase(const uint8_t bank, const uint8_t page);
extern void flash_write16(uint16_t* address, const uint16_t data, const uint32_t size);
extern void highCyclic_setArea(const uint32_t sectorCountBank1, const uint32_t sectorCountBank2);

#endif // FLASH_H