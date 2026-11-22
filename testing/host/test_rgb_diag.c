/* Exercise the real task/snapshot adapter with substituted kernel boundaries. */
#define AAI05_APP_H
#ifndef AAI05_RGB_DIAG
#define AAI05_RGB_DIAG 1
#endif
#include "rgb_diag.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

typedef int ID;
typedef int ER;
typedef int INT;
typedef unsigned int UINT;
typedef unsigned char UB;
typedef void (*FP)(INT, void *);
typedef struct {unsigned int mtxatr;} T_CMTX;
#define E_OK 0
#define E_NOEXS (-42)
#define TMO_POL 0
#define TMO_FEVR (-1)
#define TA_INHERIT 3U
#define APP_PRI_RGB 15
static ID g_mutex_result = 1;
static ID g_task_result = 2;
static bool g_locked;
static bool g_busy;
static uint32_t g_now = 100U;
static char g_output[2048];
static size_t g_output_size;
static ID tk_cre_mtx(const T_CMTX *p_attributes)
{assert(p_attributes->mtxatr == TA_INHERIT); return g_mutex_result;}
static ER tk_loc_mtx(ID id, int timeout)
{
    assert(id == 1 && !g_locked && (timeout == TMO_POL || timeout == TMO_FEVR));
    if (g_busy) {return -50;}
    g_locked = true;
    return E_OK;
}
static ER tk_unl_mtx(ID id)
{assert(id == 1 && g_locked); g_locked = false; return E_OK;}
static ER tk_dly_tsk(int delay) {assert(!g_locked && delay == 10); return 0;}
static ID app_start_task(FP function, int priority, unsigned int processor, const char *p_name)
{
    assert(function != NULL && p_name != NULL && priority == 15 && processor == 0U);
    return g_task_result;
}
uint32_t app_now_ms(void);
uint32_t app_now_ms(void) {return g_now;}
static void tm_printf(const UB *p_format, ...)
{
    assert(!g_locked && p_format != NULL);
    va_list args;
    va_start(args, p_format);
    int count = vsnprintf(&g_output[g_output_size], sizeof(g_output) - g_output_size,
                          (const char *)p_format, args);
    va_end(args);
    assert(count >= 0 && (size_t)count < sizeof(g_output) - g_output_size);
    g_output_size += (size_t)count;
}
const rgb_bus_t *rgb_rp2040_bus(void);
const rgb_bus_t *rgb_rp2040_bus(void) {return NULL;}

#include "../../app_program/subsystem1/rgb_diag.c"

int main(void)
{
    rgb_snapshot_t snapshot;
    g_mutex_result = -57;
    assert(rgb_diag_init() == -57);
    assert(rgb_diag_snapshot(&snapshot));
    assert(!snapshot.valid && !snapshot.ready && snapshot.status == 8U);
    assert(rgb_diag_start() == E_NOEXS);
    rgb_diag_report();
    assert(strstr(g_output, "RGB_CLASS[") != NULL);
    assert(strstr(g_output, "class=UNKNOWN") != NULL);
#if AAI05_FOLLOW_APP
    assert(strstr(g_output, "DRIVE=OWNER") != NULL);
    assert(strstr(g_output, "DRIVE=OFF") == NULL);
#endif
    g_output_size = 0U; g_output[0] = '\0';
    g_mutex_result = 1;
    assert(rgb_diag_init() == E_OK);
    g_task_result = -34;
    assert(rgb_diag_start() == -34);
    assert(rgb_diag_snapshot(&snapshot) && snapshot.status == 8U && !snapshot.ready);
    g_task_result = 2;
    assert(rgb_diag_init() == E_OK && rgb_diag_start() == E_OK);
    g_latest = (rgb_snapshot_t){.valid=true, .has_sample=true, .sampled_ms=90U};
    assert(rgb_diag_snapshot(&snapshot) && snapshot.valid && !g_locked);
    g_now = 291U;
    assert(rgb_diag_snapshot(&snapshot) && !snapshot.valid);
    g_now = 100U;
    g_latest = (rgb_snapshot_t){.red=274U, .green=276U, .blue=252U,
        .clear=662U, .sampled_ms=90U, .sequence=7U, .device_id=0x44U,
        .status=RGB_OK, .valid=true, .has_sample=true, .ready=true};
    rgb_diag_report();
    assert(strstr(g_output, "R=274 G=276 B=252 C=662") != NULL);
    assert(strstr(g_output, "class=BLACK") != NULL);
    assert(strstr(g_output, "reason=MATCH") != NULL);
    assert(strstr(g_output, "sample=90 seq=7") != NULL);
    g_output_size = 0U; g_output[0] = '\0';
    g_now = 291U;
    rgb_diag_report();
    assert(strstr(g_output, "class=UNKNOWN reason=STALE") != NULL);
    g_output_size = 0U; g_output[0] = '\0';
    g_busy = true;
    assert(!rgb_diag_snapshot(&snapshot) && !g_locked);
    rgb_diag_report();
    assert(strstr(g_output, "SNAPSHOT_BUSY") != NULL);
    assert(strstr(g_output, "class=UNKNOWN") != NULL);
    assert(strstr(g_output, "class=BLACK") == NULL);
    puts("RGB task adapter: resource failures/nonblocking snapshot/freshness PASS");
    return 0;
}
