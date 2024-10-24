
#include "flash.h"
#define CHECK_HDP
#define CHECK_WRP
#define WRITE_CRITICAL_SECTION

#define FLASH_KEY1              (0x45670123UL)
#define FLASH_KEY2              (0xCDEF89ABUL)
#define FLASH_OPT_KEY1          (0x08192A3BUL)
#define FLASH_OPT_KEY2          (0x4C5D6E7FUL)

#define FLASH_ERROR_FLAGS       (FLASH_SR_OPTCHANGEERR | FLASH_SR_INCERR | FLASH_SR_STRBERR | FLASH_SR_PGSERR | FLASH_SR_WRPERR)
#define FLASH_OP_INCOMPLETE     (FLASH_SR_BSY | FLASH_SR_DBNE | FLASH_SR_WBNE)

#define RETURN_TRUE_IF_TRUE(cond) if(cond) {return true;}

#ifdef CHECK_WRP
/**
 * @brief Checks if WRP applies to the supplied sector range
 * 
 * @param startSector the first sector to check
 * @param endSector the last sector to check
 * @param bank the corresponding bank
 * @return true if at least parts of the sector range is WRP-protected
 * @return false if sector range is WRP-unprotected
 */
static bool checkWRP(const uint32_t startSector, const uint32_t endSector, const uint32_t bank)
{
    uint32_t startGroup = (startSector >> 2);
    uint32_t endGroup = (endSector >> 2);
    uint32_t bitmap = ((0x2UL << endGroup) - 1) & (~((0x1UL << startGroup) - 1));

    return ((bank == 1 && ((~FLASH->WRP1R_CUR) & bitmap) != 0) ||
            (bank == 2 && ((~FLASH->WRP2R_CUR) & bitmap) != 0));
}
#endif

#ifdef CHECK_HDP
#define HDPL0 0xB4
#define HDPL1 0x51
#define HDPL2 0x8A
#define HDPL3 0x6F

/**
 * @brief Checks if HDP applies to the supplied sector range
 * 
 * @param startSector the first sector to check
 * @param endSector the last sector to check
 * @param bank the corresponding bank
 * @return true if at least parts of the sector range is HDP-protected
 * @return false if sector range is HDP-unprotected
 */
static bool checkHDP(const uint32_t startSector, const uint32_t endSector, const uint32_t bank)
{
    uint32_t hdplLevel = SBS->HDPLSR & SBS_HDPLSR_HDPL_Msk;
    uint32_t hdpStart;
    uint32_t hdpEnd;
    uint32_t hdpExtMask;
    uint32_t hdpExtPos;

    if ( hdplLevel != HDPL0 && hdplLevel != HDPL1)
    {
        if (bank == 1)
        {   
            hdpStart = FLASH->HDP1R_CUR & FLASH_HDPR_HDP_STRT_Msk;
            hdpEnd = (FLASH->HDP1R_CUR & FLASH_HDPR_HDP_END_Msk) >> FLASH_HDPR_HDP_END_Pos;
            hdpExtPos = FLASH_HDPEXTR_HDP1_EXT_Pos;
            hdpExtMask = FLASH_HDPEXTR_HDP1_EXT_Msk;
        }
        else if (bank == 2)
        {
            hdpStart = FLASH->HDP2R_CUR & FLASH_HDPR_HDP_STRT_Msk;
            hdpEnd = (FLASH->HDP2R_CUR & FLASH_HDPR_HDP_END_Msk) >> FLASH_HDPR_HDP_END_Pos;
            hdpExtPos = FLASH_HDPEXTR_HDP2_EXT_Pos;
            hdpExtMask = FLASH_HDPEXTR_HDP2_EXT_Msk;
        }
        else
        {
            return true;
        }

        RETURN_TRUE_IF_TRUE((hdpEnd >= hdpStart) && endSector >= hdpStart && startSector <= hdpEnd) // covers all three intersections: ([)], [(]), [()]
        
        if (hdplLevel != HDPL2)
        {
            uint32_t hdpExt = (FLASH->HDPEXTR & hdpExtMask) >> hdpExtPos;

            RETURN_TRUE_IF_TRUE((hdpEnd >= hdpStart) && endSector >= hdpStart && startSector <= hdpEnd + hdpExt)

            // HDP behaves slightly differently when hdpStart > hdpEnd
            RETURN_TRUE_IF_TRUE(hdpStart > hdpEnd && endSector >= hdpEnd && hdpStart <= hdpEnd + hdpExt)
        }
    }
    return false;
}
#endif

/**
 * @brief get the corresponding bank number (1/2) for an address range in high cyclic memory
 * 
 * @param address the first address of the address range inside high cyclic memory
 * @param size the amount of bytes in range
 * @return 1 or 2 if completely inside the configured high cyclic memory, 0 otherwise
 */
static uint32_t highCyclic_getBank(const void* address, const uint32_t size)
{
    uint32_t sectorCount1 = 0;
    if (FLASH->EDATA1R_CUR & FLASH_EDATAR_EDATA_EN)
    {
        sectorCount1 = 1 + ((FLASH->EDATA1R_CUR & FLASH_EDATAR_EDATA_STRT_Msk) >> FLASH_EDATAR_EDATA_STRT_Pos);
    }
    uint32_t sectorCount2 = 0;
    if (FLASH->EDATA2R_CUR & FLASH_EDATAR_EDATA_EN)
    {
        sectorCount2 = 1 + ((FLASH->EDATA2R_CUR & FLASH_EDATAR_EDATA_STRT_Msk) >> FLASH_EDATAR_EDATA_STRT_Pos);
    }

    if ((uint32_t) address >= (HIGH_CYCLIC_END_BANK1 - HIGH_CYCLIC_SECTOR_SIZE * sectorCount1) &&
            ((uint32_t) address + size - 1 <= HIGH_CYCLIC_END_BANK1))
    {
        return 1;
    }
    else if ((uint32_t) address >= (HIGH_CYCLIC_END_BANK2 - HIGH_CYCLIC_SECTOR_SIZE * sectorCount2) &&
            ((uint32_t) address + size - 1 <= HIGH_CYCLIC_END_BANK2))
    {
        return 2;
    }
    else
    {
        return 0;
    }
}

/**
 * @brief get the corresponding bank number (1/2) for an address range in flash memory
 * 
 * @param address the first address of the address range inside flash memory
 * @param size the amount of bytes in range
 * @return 1 or 2 if completely inside the configured flash memory, 0 otherwise
 */
static uint32_t flash_getBank(const void* address, const uint32_t size)
{
    if (((uint32_t) address) >= FLASH_START_BANK1 &&
        ((uint32_t) address) <= FLASH_END_BANK1 &&
        ((uint32_t) address + size - 1) >= FLASH_START_BANK1 &&
        ((uint32_t) address + size - 1) <= FLASH_END_BANK1)
    {
        return 1;
    }
    else if (((uint32_t) address) >= FLASH_START_BANK2 &&
             ((uint32_t) address) <= FLASH_END_BANK2 &&
             ((uint32_t) address + size - 1) >= FLASH_START_BANK2 &&
             ((uint32_t) address + size - 1) <= FLASH_END_BANK2)
    {
        return 2;
    }
    else
    {
        return 0;
    }
}

/**
 * @brief unlock the Flash for modification by unlocking NSKEYR
 */
static void unlockFlash()
{
    FLASH->NSKEYR = FLASH_KEY1;
    FLASH->NSKEYR = FLASH_KEY2;
}

/**
 * @brief unlock the Flash for modification of option bytes by unlocking OPTKEYR
 */
static void unlockFlashOptionBytes()
{
    FLASH->OPTKEYR = FLASH_OPT_KEY1;
    FLASH->OPTKEYR = FLASH_OPT_KEY2;
}

/**
 * @brief set the area of which pages should be treated as high cyclic
 * 
 * @param reg the EDATAR for the corresponding bank to set the area for
 * @param sectorCount the amount of sectors to use. 0 deactivates high cyclic memory
 */
static void highCyclic_setArea_internal(const uint32_t bank, const uint32_t sectorCount)
{
    volatile uint32_t *eDataRegCur;
    volatile uint32_t *eDataRegProg;
    if (bank == 1)
    {
        eDataRegCur = &(FLASH->EDATA1R_CUR);
        eDataRegProg = &(FLASH->EDATA1R_PRG);
    }
    else if (bank == 2)
    {
        eDataRegCur = &(FLASH->EDATA2R_CUR);
        eDataRegProg = &(FLASH->EDATA2R_PRG);
    }
    else
    {
        return;
    }

    uint32_t configuration;
    if (sectorCount > 0)
    {
        configuration = FLASH_EDATAR_EDATA_EN | (((sectorCount - 1) & 0x7) << FLASH_EDATAR_EDATA_STRT_Pos);
    }
    else
    {
        configuration = (0 << FLASH_EDATAR_EDATA_EN_Pos) | (0 << FLASH_EDATAR_EDATA_STRT_Pos);
    }

    // check if configuration is already present
    if (*eDataRegCur == configuration)
    {
        return;
    }

    // check error flags and BSY, DBNE, WBNE
    RETURN_TRUE_IF_TRUE((FLASH->NSSR & (FLASH_ERROR_FLAGS | FLASH_OP_INCOMPLETE)) != 0)

    // unlock flash option bytes
    unlockFlashOptionBytes();
    

    // write configuration data into programming register
    *eDataRegProg = configuration;

    // start flashing
    FLASH->OPTCR |= FLASH_OPTCR_OPTSTART;

    // wait for bsy clear
    while (FLASH->NSSR & FLASH_SR_BSY) {};

    // lock flash again
    FLASH->OPTCR = FLASH_OPTCR_OPTLOCK;

}

/**
 * @brief erase a flash page
 * 
 * @param page the page number
 * @return false    OK
 * @return true     Error
 */
static bool flashErase(const uint32_t bank, const uint32_t page)
{
    // error if page number is invalid
    RETURN_TRUE_IF_TRUE(!(bank == 1 || bank == 2))
    RETURN_TRUE_IF_TRUE(page >= FLASH_PAGES_PER_BANK)

    // HDP(temporal isolation protection/hide protection) and WRP(write protection) can be checked before erasing,
    // but as a fallback they both will also result in errors if enabled and not checked beforehand, nevertheless.
    // Since probably are not configured at all, these checks can probably be omitted
#ifdef CHECK_HDP
    RETURN_TRUE_IF_TRUE(checkHDP(page, page, bank))
#endif
#ifdef CHECK_WRP
    RETURN_TRUE_IF_TRUE(checkWRP(page, page, bank))
#endif

    // any flash error in status-register?
    // also check BSY, DBNE, WBNE
    RETURN_TRUE_IF_TRUE((FLASH->NSSR & (FLASH_ERROR_FLAGS | FLASH_OP_INCOMPLETE)) != 0)

    // unlock NSCR if not yet unlocked
    unlockFlash();
    if ((FLASH->NSSR & FLASH_SR_WBNE) || (FLASH->NSSR & FLASH_SR_DBNE))
    {
        for (;;) {}
    }

    // set bksel, ser and snb in NSCR
    uint32_t nscrMsk = FLASH_CR_OPTCHANGEERRIE | FLASH_CR_INCERRIE | FLASH_CR_STRBERRIE | FLASH_CR_PGSERRIE | FLASH_CR_WRPERRIE | FLASH_CR_EOPIE;
    FLASH->NSCR = (FLASH->NSCR & nscrMsk) | (page << FLASH_CR_SNB_Pos) | ((bank-1) << FLASH_CR_BKSEL_Pos) | FLASH_CR_SER;
    
    // wait for bsy clear
    while (FLASH->NSSR & FLASH_SR_BSY) {};

    // set strt in nscr
    FLASH->NSCR |= FLASH_CR_START;

    // wait for bsy clear
    while (FLASH->NSSR & FLASH_SR_BSY) {};

    // clear ser
    FLASH->NSCR &= ~FLASH_CR_SER_Msk;

    // lock flash again
    FLASH->NSCR = FLASH_CR_LOCK;

    // check for errors again
    RETURN_TRUE_IF_TRUE((FLASH->NSSR & FLASH_ERROR_FLAGS) != 0)

    return false;
}

/**
 * @brief write 128 Bit quad-words into main flash
 * @warning WRITE_CRITICAL_SECTION deaktivates all interrupts for the critical write section. define as required.
 * @note CHECK_HDP defines if the addresses should be checked for HDP locks
 * @note CHECK_WRP defines if the addresses should be checked for WRP locks
 *
 * @param address   pointer to target address in flash
 * @param data      pointer to data
 * @param size      amount of bytes to write, has to be multiple of 16
 * @return true     OK
 * @return false    Error
 */
static bool flashWrite128 (uint32_t *address, const uint32_t *data, const uint32_t size)
{
    // Address 128 Bit aligned?
    RETURN_TRUE_IF_TRUE(((uint32_t)address & 0x0000000f) != 0)
    // size multiple of 128 Bit?
    RETURN_TRUE_IF_TRUE((size & 0x0000000f) != 0)

    // check for valid flash addresses
    uint32_t bank = flash_getBank((void*) address, size);
    RETURN_TRUE_IF_TRUE(!(bank == 1 || bank == 2))
    
    // calculate sector if HDP or WRP should be checked
#if defined(CHECK_HDP) || defined(CHECK_WRP)
    uint32_t bankStart = (bank == 1) ? FLASH_START_BANK1 : FLASH_START_BANK2;
    uint32_t startSector = (((uint32_t) address) - bankStart) / FLASH_PAGES_PER_BANK;
    uint32_t endSector = (((uint32_t) address) + (size - 1) - bankStart) / FLASH_PAGES_PER_BANK;
#ifdef CHECK_HDP
        RETURN_TRUE_IF_TRUE(checkHDP(startSector, endSector, bank))
#endif
#ifdef CHECK_WRP
        RETURN_TRUE_IF_TRUE(checkWRP(startSector, endSector, bank))
#endif
#endif

    // Any Flash Error in Status-Register and not Busy?
    RETURN_TRUE_IF_TRUE((FLASH->NSSR & (FLASH_ERROR_FLAGS | FLASH_OP_INCOMPLETE)) != 0)

    unlockFlash();

    uint32_t nscrMsk = FLASH_CR_OPTCHANGEERRIE | FLASH_CR_INCERRIE | FLASH_CR_STRBERRIE | FLASH_CR_PGSERRIE | FLASH_CR_WRPERRIE | FLASH_CR_EOPIE;
    FLASH->NSCR = (FLASH->NSCR & nscrMsk) | FLASH_CR_PG;

    // Write Data Section
#ifdef WRITE_CRITICAL_SECTION
    // Enter critical section: Disable interrupts to avoid any interruption during the loop
    uint32_t primaskBit = __get_PRIMASK();
    __disable_irq();
#endif

    for (uint32_t i = 0; i < size; i += 16)
    {
        // program quad-word
        *address++ = *data++;             // program first 32 bit
        *address++ = *data++;             // program second 32 bit
        *address++ = *data++;             // program third 32 bit
        *address++ = *data++;             // program fourth 32 bit

        // wait for bsy clear
        while (FLASH->NSSR & FLASH_SR_BSY) {};
    }
    
#ifdef WRITE_CRITICAL_SECTION
    // Exit critical section: restore previous priority mask
    __set_PRIMASK(primaskBit);
#endif

    // cleanup after write and check errors

    // wait for bsy clear
    while (FLASH->NSSR & FLASH_SR_BSY) {};

    // clear ser
    FLASH->NSCR &= ~FLASH_CR_PG;

    // lock flash again
    FLASH->NSCR = FLASH_CR_LOCK;

    // check for errors again
    RETURN_TRUE_IF_TRUE((FLASH->NSSR & FLASH_ERROR_FLAGS) != 0)

    return false;
}

/**
 * @brief write 16 bits half-words to high cyclic flash
 * @warning WRITE_CRITICAL_SECTION deaktivates all interrupts for the critical write section. define as required.
 * @note CHECK_HDP defines if the addresses should be checked for HDP locks
 * @note CHECK_WRP defines if the addresses should be checked for WRP locks
 *
 * @param address   pointer to the address to write the data into the high cyclic flash
 * @param data      pointer to the data to write
 * @param size      amount of bytes to write, has to be multiple of 2
 * @return true     OK
 * @return false    Error
 */
static bool highCyclic_write16(uint16_t *address, const uint16_t *data, const uint32_t size)
{
    // Address 128 Bit aligned?
    RETURN_TRUE_IF_TRUE(((uint32_t)address & 0x00000001) != 0)
    // size multiple of 16 Bit?
    RETURN_TRUE_IF_TRUE((size & 0x00000001) != 0)

    // check for valid high cyclic addresses
    uint32_t bank = highCyclic_getBank((void*) address, size);
    RETURN_TRUE_IF_TRUE(!(bank == 1 || bank == 2))

    // calculate sector if HDP or WRP should be checked
#if defined(CHECK_HDP) || defined(CHECK_WRP)
    uint32_t highCyclicBankStart = (bank == 1) ? HIGH_CYCLIC_START_BANK1 : HIGH_CYCLIC_START_BANK2;
    uint32_t startSector = HIGH_CYCLIC_PAGE_OFFSET + (((uint32_t) address) - highCyclicBankStart) / HIGH_CYCLIC_SECTOR_SIZE;
    uint32_t endSector = HIGH_CYCLIC_PAGE_OFFSET + (((uint32_t) address) + (size - 1) - highCyclicBankStart) / HIGH_CYCLIC_SECTOR_SIZE;
#ifdef CHECK_HDP
    RETURN_TRUE_IF_TRUE(checkHDP(startSector, endSector, bank))
#endif
#ifdef CHECK_WRP
    RETURN_TRUE_IF_TRUE(checkWRP(startSector, endSector, bank))
#endif
#endif

    // Any Flash Error in Status-Register and not Busy?
    RETURN_TRUE_IF_TRUE((FLASH->NSSR & (FLASH_ERROR_FLAGS | FLASH_OP_INCOMPLETE)) != 0)

    unlockFlash();

    uint32_t nscrMsk = FLASH_CR_OPTCHANGEERRIE | FLASH_CR_INCERRIE | FLASH_CR_STRBERRIE | FLASH_CR_PGSERRIE | FLASH_CR_WRPERRIE | FLASH_CR_EOPIE;
    FLASH->NSCR = (FLASH->NSCR & nscrMsk) | FLASH_CR_PG;

#ifdef WRITE_CRITICAL_SECTION
    // Enter critical section: Disable interrupts to avoid any interruption during the loop
    uint32_t primaskBit = __get_PRIMASK();
    __disable_irq();
#endif

    // program 
    for (uint32_t i = 0; i < size; i += 2)
    {
        *address++ = *data++;
    }
    
#ifdef WRITE_CRITICAL_SECTION
    // Exit critical section: restore previous priority mask
    __set_PRIMASK(primaskBit);
#endif

    // wait for bsy clear
    while (FLASH->NSSR & FLASH_SR_BSY) {};

    // clear ser
    FLASH->NSCR &= ~FLASH_CR_PG;

    // lock flash again
    FLASH->NSCR = FLASH_CR_LOCK;

    // check for errors again
    RETURN_TRUE_IF_TRUE((FLASH->NSSR & FLASH_ERROR_FLAGS) != 0)

    return false;
}

/**
 * @brief Configure how much flash should be treated as high cyclic memory
 * 
 * @param sectorCountBank1 amount of sectors in bank 1
 * @param sectorCountBank2 amount of sectors in bank 2
 */
void __attribute__((used)) highCyclic_setArea(const uint32_t sectorCountBank1, const uint32_t sectorCountBank2)
{
    highCyclic_setArea_internal(1, sectorCountBank1);
    highCyclic_setArea_internal(2, sectorCountBank2);
}

// ----------------------------------------------------------------------------
// extern section
// ----------------------------------------------------------------------------
/**
 * @brief external function to erase a flash page
 * 
 * @param bank Bank 1 or 2
 * @param page page number
 */
void flash_erase(const uint8_t bank, const uint8_t page)
{
    flashErase(bank, page);
}
/**
 * @brief external function to write half-words to a flash
 * 
 * @param address target address
 * @param data data to write
 * @param size amount of bytes to write
 */
void flash_write16(uint16_t* address, const uint16_t data, const uint32_t size)
{
    uint32_t bank = highCyclic_getBank(address, 1);
    if ((bank == 1) || (bank == 2))
    {
        // high cyclic flash is writable by 16 bit
        if ((size & 0x1) == 0)
        {
            (void) highCyclic_write16(address, &data, size);
        }
    }
    else
    {
        // normal flash is writable by 128 bit
        if ((size & 0xf) == 0)
        {
            (void) flashWrite128((uint32_t*) address, (const uint32_t*) &data, size);
        }
    }
}
