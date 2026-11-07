#include "drive_service.h"
#if AAI05_REMOTE_APP
/* Keep SDK register definitions separate from the kernel BSP namespaces. */
#include "hardware/regs/addressmap.h"
#include "hardware/regs/rosc.h"
#include "hardware/regs/timer.h"

_Static_assert(ROSC_BASE + ROSC_RANDOMBIT_OFFSET == UINT32_C(0x4006001C),
               "Session entropy requires the RP2040 ROSC random-bit register");
_Static_assert(TIMER_BASE + TIMER_TIMERAWL_OFFSET == UINT32_C(0x40054028),
               "Session seed requires the RP2040 raw timer-low register");

static uint32_t drive_entropy_read(uint32_t address)
{
    return *(volatile const uint32_t *)(uintptr_t)address;
}

uint32_t drive_io_seed(void)
{
    uint32_t seed = drive_entropy_read(TIMER_BASE + TIMER_TIMERAWL_OFFSET);
    for (uint32_t index = 0U; index < 128U; ++index)
    {
        seed = (seed << 1U) | (seed >> 31U);
        seed ^= drive_entropy_read(ROSC_BASE + ROSC_RANDOMBIT_OFFSET) & 1U;
        seed ^= drive_entropy_read(TIMER_BASE + TIMER_TIMERAWL_OFFSET);
    }
    return seed; /* Not an authentication or cryptographic-quality claim. */
}
#endif
