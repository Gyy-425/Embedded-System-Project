/* Execute the real RP2040 startup code against register/kernel boundary fakes.
   The single-core conditional avoids pulling target SMP assembly into the host;
   GPIO ownership checks are identical in SMP and non-SMP builds. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define AAI05_APP_H
#define TK_SUPPORT_SMP 0
#define IO_BANK0_BASE UINT32_C(0x40014000)
#define GPIO_IN UINT32_C(0xD0000004)
#define GPIO_MODE_IN 0U
#define E_OK 0
#define E_BUSY (-65)
#define TA_HLNG 1U
typedef unsigned int UINT;
typedef int ER;
typedef void (*FP)(void);
typedef struct {unsigned int intatr; FP inthdr;} T_DINT;

static uint32_t registers[128];
static unsigned int definitions;
static unsigned int enabled;
static unsigned int configured;
static unsigned int resets;
static int definition_error;

static unsigned int register_index(uintptr_t address)
{
    assert(address >= IO_BANK0_BASE && address < IO_BANK0_BASE + sizeof(registers));
    return (unsigned int)(address - IO_BANK0_BASE) / 4U;
}
static uint32_t read_register(uintptr_t address)
{
    if (address == GPIO_IN) return 0U;
    return registers[register_index(address)];
}
static void write_register(uintptr_t address, uint32_t value)
{
    /* Startup must touch only GPIO IRQ registers, never motor output registers. */
    assert(address >= IO_BANK0_BASE + 0xF0U);
    registers[register_index(address)] = value;
}
#define in_w(address) read_register(address)
#define out_w(address,value) write_register((address),(value))
#define set_w(address,value) write_register((address),read_register(address) | (value))
#define DI(saved) ((saved) = 23U)
#define EI(saved) ((void)(saved))
static void DisableInt(UINT irq) { assert(irq == 13U); enabled = 0U; }
static void ClearInt(UINT irq) { assert(irq == 13U); }
static void EnableInt(UINT irq, int priority)
{
    assert(irq == 13U && priority == 2 && resets == 1U);
    enabled = 1U;
}
static ER gpio_set_pin(UINT pin, UINT mode)
{
    assert((pin == 4U || pin == 5U || pin == 16U || pin == 17U) && mode == GPIO_MODE_IN);
    ++configured;
    return E_OK;
}
static ER tk_def_int(UINT irq, const T_DINT *attributes)
{
    assert(irq == 13U && attributes->intatr == TA_HLNG && attributes->inthdr != NULL);
    ++definitions;
    return definition_error;
}
void encoder_capture_reset(void) { assert(enabled == 0U); ++resets; }
void encoder_capture_irq(void) { }

#include "../../app_program/subsystem2/encoder_capture_rp2040.c"

static void reset_hardware(void)
{
    memset(registers, 0, sizeof(registers));
    definitions = enabled = configured = resets = 0U;
    definition_error = 0;
}

int main(void)
{
    /* Any core-1 GPIO owner conflicts with the port's globally shared IRQ vector. */
    for (unsigned int bank = 0U; bank < 4U; ++bank) {
        reset_hardware();
        registers[register_index(ENCODER_PROC1_INTE(bank))] = 1U;
        assert(encoder_capture_start() == E_BUSY);
        assert(definitions == 0U && configured == 0U && enabled == 0U);
        reset_hardware();
        registers[register_index(ENCODER_PROC1_INTF(bank))] = 8U;
        assert(encoder_capture_start() == E_BUSY);
        assert(definitions == 0U && enabled == 0U);
        reset_hardware();
        registers[register_index(ENCODER_PROC0_INTE(bank))] = 1U;
        assert(encoder_capture_start() == E_BUSY);
        assert(definitions == 0U && enabled == 0U);
    }
    reset_hardware();
    definition_error = -5;
    assert(encoder_capture_start() == -5);
    assert(enabled == 0U && resets == 0U);
    reset_hardware();
    assert(encoder_capture_start() == E_OK);
    assert(configured == 4U && definitions == 1U && enabled == 1U && resets == 1U);
    assert(in_w(ENCODER_PROC0_INTE(0U)) == ENCODER_LEFT_EDGES);
    assert(in_w(ENCODER_PROC0_INTE(2U)) == ENCODER_RIGHT_EDGES);
    assert(in_w(ENCODER_PROC0_INTE(1U)) == 0U && in_w(ENCODER_PROC0_INTE(3U)) == 0U);
    puts("PASS: shared-vector ownership, startup failure and owned-pin IRQ setup");
    return 0;
}
