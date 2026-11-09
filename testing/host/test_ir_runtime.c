/* Real IR task adapter, with only kernel/ADC boundaries substituted. */
#define AAI05_APP_H
#include "colour_runtime.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef int ID;
typedef int ER;
typedef int INT;
typedef void (*FP)(INT, void *);
typedef struct {unsigned int mtxatr;} T_CMTX;
typedef struct
{
    uint32_t sequence;
    uint32_t sampled_ms;
    uint16_t left_raw;
    uint16_t right_raw;
    ER error;
    bool valid;
} ir_snapshot_t;
#define EXPORT
#define E_OK 0
#define E_PAR (-17)
#define E_TMOUT (-50)
#define TMO_POL 0
#define TMO_FEVR (-1)
#define TA_INHERIT 3U
#define APP_PRI_IR 14
static bool busy;
static bool locked;
static uint32_t now = 100U;
static ER adc_error;
static ID tk_cre_mtx(const T_CMTX *p_attributes)
{ assert(p_attributes->mtxatr == TA_INHERIT); return 1; }
static ER tk_loc_mtx(ID id, int timeout)
{
    assert(id == 1 && (timeout == TMO_POL || timeout == TMO_FEVR));
    assert(!locked);
    if (busy) return E_TMOUT;
    locked = true; return E_OK;
}
static ER tk_unl_mtx(ID id) {assert(id == 1 && locked); locked = false; return E_OK;}
static ER tk_dly_tsk(int delay) {(void)delay; return E_OK;}
static ID app_start_task(FP function, int priority, unsigned int processor, const char *name)
{ (void)function; (void)name; assert(priority == 14 && processor == 0U); return 2; }
uint32_t app_now_ms(void) {return now;}
ER drv_ir_open(void) {return E_OK;}
ER drv_ir_read(uint16_t *left, uint16_t *right)
{ *left = 123U; *right = 456U; return adc_error; }

#include "../../app_program/subsystem1/subsystem1.c"

int main(void)
{
    colour_report_t report;
    ir_snapshot_t sample = {0};
    bool opened = false;
    assert(subsystem1_init() == E_OK);
    assert(!subsystem1_colour_latest(&report));
    ir_sample(&sample, &opened);
    assert(opened && !locked);
    assert(subsystem1_colour_latest(&report));
    assert(report.observation.raw[0] == 123U && report.observation.raw[1] == 456U);
    assert(report.observation.result[0] == 15U && report.observation.sequence == 1U);
    assert(subsystem1_colour_pop(&report));
    assert(!subsystem1_colour_pop(&report));
    now = 110U;
    adc_error = -57;
    ir_sample(&sample, &opened);
    assert(subsystem1_colour_latest(&report) && report.observation.result[0] == 39U);
    ir_snapshot_t raw;
    assert(subsystem1_get_snapshot(&raw) == E_OK && !raw.valid && raw.error == -57);
    busy = true;
    assert(!subsystem1_colour_latest(&report));
    assert(!subsystem1_colour_pop(&report));
    assert(!subsystem1_colour_begin(&report));
    assert(!locked);
    busy = false;
    assert(subsystem1_colour_begin(&report));
    assert(!subsystem1_colour_pop(&report));
    bool snapshot = false;
    assert(!subsystem1_colour_take(&report, &snapshot, false));
    assert(subsystem1_colour_take(&report, &snapshot, true) && snapshot);
    assert(subsystem1_start() == E_OK);
    puts("PASS: real IR acquisition publishes raw and colour atomically; busy readers do not block");
    return 0;
}
