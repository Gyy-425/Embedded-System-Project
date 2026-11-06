#include "../app.h"
#include "encoder_capture.h"
#if TK_SUPPORT_SMP
#include <tk/smp.h>
static T_SPLOCK capture_lock;
#endif

/* RP2040 IO_BANK0, matching Pico SDK 2.3.0 hardware/regs/io_bank0.h. */
#define ENCODER_GPIO_IRQ 13U
#define ENCODER_IRQ_PRIORITY 2
#define ENCODER_INTR(bank) (IO_BANK0_BASE + 0xF0U + ((bank) * 4U))
#define ENCODER_PROC0_INTE(bank) (IO_BANK0_BASE + 0x100U + ((bank) * 4U))
#define ENCODER_PROC0_INTF(bank) (IO_BANK0_BASE + 0x110U + ((bank) * 4U))
#define ENCODER_PROC0_INTS(bank) (IO_BANK0_BASE + 0x120U + ((bank) * 4U))
#define ENCODER_PROC1_INTE(bank) (IO_BANK0_BASE + 0x130U + ((bank) * 4U))
#define ENCODER_PROC1_INTF(bank) (IO_BANK0_BASE + 0x140U + ((bank) * 4U))

uint32_t encoder_io_lock(void)
{
    UINT saved;
#if TK_SUPPORT_SMP
    (void)ISpinLock(&capture_lock, &saved);
#else
    DI(saved);
#endif
    return (uint32_t)saved;
}

void encoder_io_unlock(uint32_t saved)
{
#if TK_SUPPORT_SMP
    (void)ISpinUnlock(&capture_lock, (UINT)saved);
#else
    EI(saved);
#endif
}

uint32_t encoder_io_levels(void) { return (uint32_t)in_w(GPIO_IN); }
uint32_t encoder_io_pending(unsigned int bank) { return (uint32_t)in_w(ENCODER_PROC0_INTS(bank)); }
void encoder_io_ack(unsigned int bank, uint32_t edges) { out_w(ENCODER_INTR(bank), edges); }

static void encoder_inthdr(UINT intno)
{
    (void)intno;
    encoder_capture_irq();
}

int32_t encoder_capture_start(void)
{
    const UINT pins[] = {APP_ENCODER_LEFT_A, APP_ENCODER_LEFT_B,
                         APP_ENCODER_RIGHT_A, APP_ENCODER_RIGHT_B};
    const T_DINT attributes = {.intatr = TA_HLNG, .inthdr = (FP)encoder_inthdr};
    ER error;
#if TK_SUPPORT_SMP
    /* NVIC and PROC0 routing must be configured on physical core 0. */
    if (tk_get_prc() != 1) return E_CTX;
#endif
    /* Refuse another GPIO consumer; future users must join this dispatcher. */
    for (UINT bank = 0U; bank < 4U; ++bank) {
        /* Both cores share tk_def_int's vector/handler table in this port. */
        if ((in_w(ENCODER_PROC0_INTE(bank)) | in_w(ENCODER_PROC0_INTF(bank)) |
             in_w(ENCODER_PROC1_INTE(bank)) | in_w(ENCODER_PROC1_INTF(bank))) != 0U) return E_BUSY;
    }
#if TK_SUPPORT_SMP
    error = InitSpinLock(&capture_lock);
    if (error < E_OK) return error;
#endif
    DisableInt(ENCODER_GPIO_IRQ);
    for (UINT i = 0U; i < 4U; ++i) {
        error = gpio_set_pin(pins[i], GPIO_MODE_IN);
        if (error < E_OK) return error;
    }
    error = tk_def_int(ENCODER_GPIO_IRQ, &attributes);
    if (error < E_OK) return error;
    encoder_io_ack(ENCODER_LEFT_BANK, ENCODER_LEFT_EDGES);
    encoder_io_ack(ENCODER_RIGHT_BANK, ENCODER_RIGHT_EDGES);
    encoder_capture_reset();
    ClearInt(ENCODER_GPIO_IRQ);
    set_w(ENCODER_PROC0_INTE(ENCODER_LEFT_BANK), ENCODER_LEFT_EDGES);
    set_w(ENCODER_PROC0_INTE(ENCODER_RIGHT_BANK), ENCODER_RIGHT_EDGES);
    EnableInt(ENCODER_GPIO_IRQ, ENCODER_IRQ_PRIORITY);
    return E_OK;
}
