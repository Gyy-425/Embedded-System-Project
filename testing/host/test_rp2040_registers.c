/* Independent RP2040 datasheet addresses; never dereference hardware on host. */
#include <stdint.h>
#include <stdio.h>
#define __SYS_MACHINE_H__
#include "../../include/sys/sysdepend/cpu/rp2040/sysdef.h"

static unsigned int failures;

static void check_address(const char *name, uint32_t actual, uint32_t expected)
{
    if (actual != expected) {
        fprintf(stderr, "%s: got 0x%08x, expected 0x%08x\n",
                name, (unsigned int)actual, (unsigned int)expected);
        ++failures;
    }
}

int main(void)
{
    /* SIO table:0x008 is QSPI input; ordinary GPIO output is0x010. */
    check_address("GPIO_OUT", GPIO_OUT, UINT32_C(0xD0000010));
    check_address("GPIO_OUT_SET", GPIO_OUT_SET, UINT32_C(0xD0000014));
    check_address("GPIO_OE_SET", GPIO_OE_SET, UINT32_C(0xD0000024));
    check_address("GPIO8 CTRL", GPIO_CTRL(8), UINT32_C(0x40014044));
    check_address("GPIO9 CTRL", GPIO_CTRL(9), UINT32_C(0x4001404C));
    check_address("GPIO10 CTRL", GPIO_CTRL(10), UINT32_C(0x40014054));
    check_address("GPIO11 CTRL", GPIO_CTRL(11), UINT32_C(0x4001405C));
    check_address("GPIO_CTRL conditional true", GPIO_CTRL(1 ? 8 : 9), UINT32_C(0x40014044));
    check_address("GPIO_CTRL conditional false", GPIO_CTRL(0 ? 8 : 9), UINT32_C(0x4001404C));
    check_address("GPIO_CTRL addition", GPIO_CTRL(7 + 1), UINT32_C(0x40014044));
    check_address("GPIO_CTRL subtraction", GPIO_CTRL(10 - 1), UINT32_C(0x4001404C));
    check_address("GPIO pad conditional true", GPIO(1 ? 26 : 27), UINT32_C(0x4001C06C));
    check_address("GPIO pad conditional false", GPIO(0 ? 26 : 27), UINT32_C(0x4001C070));
    check_address("GPIO pad addition", GPIO(25 + 1), UINT32_C(0x4001C06C));
    check_address("GPIO pad subtraction", GPIO(28 - 1), UINT32_C(0x4001C070));
    if (failures != 0U) return 1;
    puts("PASS: actual BSP SIO/GPIO addresses and conditional/arithmetic macro arguments");
    return 0;
}
