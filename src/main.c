
#include "stm32h563.h"
#include "flash.h"

volatile __USED __attribute__((section (".hcflash"))) uint16_t test_data[] = {
    0x0738,
    0x0E70,
    0x1CE0,
    0x39C0,
    0x7380,
    0xE700,
    0xCE01,
    0x9C03,
    0x3807,
    0x700E,
    0xE01C,
    0xC039,
    0x8073,
    0x00E7,
    0x01CE,
    0x039C,
    0x0738
};
volatile __USED bool data_section_integrity;

int main (void)
{
    // ------------------------------------------------------------------------
    // Check integrity of test_data section
    // ------------------------------------------------------------------------
    uint16_t correct_val = 0x0738;
    uint16_t* flash_ptr = (uint16_t*) &test_data;
    data_section_integrity = true;
    
    for (uint16_t i = 0; i < 17; i++)
    {
        // check if this data entry is correct
        if (*flash_ptr != correct_val)
        {
            data_section_integrity = false;
        }

        // roll left (shift left and add overflow on the right)
        if (correct_val & 0x8000)
        {
            correct_val = (correct_val << 1) + 1;
        }
        else
        {
            correct_val = correct_val << 1;
        }

        // check next pointer
        flash_ptr++;
    }
    

    // ------------------------------------------------------------------------
    // Cyclic write into high cyclic memory to test gdb reads
    // ------------------------------------------------------------------------
    while (1)
    {
        // erase flash bank 2, page 120
        flash_erase(2, HIGH_CYCLIC_PAGE_OFFSET); // <========================= BREAKPOINT 1 here
        flash_write16((uint16_t*) (HIGH_CYCLIC_START_BANK2), 0x0123, 2);
        flash_write16((uint16_t*) (HIGH_CYCLIC_START_BANK2 + 2), 0x4567, 2);
        flash_write16((uint16_t*) (HIGH_CYCLIC_START_BANK2 + 4), 0x89AB, 2);
        flash_write16((uint16_t*) (HIGH_CYCLIC_START_BANK2 + 6), 0xCDEF, 2);

        //__builtin_trap();

        // erase flash bank 2, page 120
        flash_erase(2, HIGH_CYCLIC_PAGE_OFFSET); // <========================= BREAKPOINT 2 here
        flash_write16((uint16_t*) (HIGH_CYCLIC_START_BANK2), 0x7f7f, 2);
        flash_write16((uint16_t*) (HIGH_CYCLIC_START_BANK2 + 2), 0x5d5d, 2);
        flash_write16((uint16_t*) (HIGH_CYCLIC_START_BANK2 + 4), 0xc8c8, 2);
        flash_write16((uint16_t*) (HIGH_CYCLIC_START_BANK2 + 6), 0x0101, 2);

        //__builtin_trap();

    }
    
}