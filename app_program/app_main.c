/* AAI05 diagnostic application. Three logical modules, one development effort. */
#include "app.h"

EXPORT uint32_t app_now_ms(void)
{
    SYSTIM now;
    (void)tk_get_tim(&now);
    return (uint32_t)now.lo;
}

EXPORT ID app_start_task(FP entry, PRI priority, UW processor, const char *name)
{
    T_CTSK attributes = {0};
    ID task;
    ER error;
    (void)name;
    attributes.task = entry;
    attributes.itskpri = priority;
    attributes.stksz = TASK_STACK_SIZE;
    attributes.tskatr = TA_HLNG | TA_RNG3;
#if TK_SUPPORT_SMP
    if (processor != 0U) {
        attributes.tskatr |= TA_ASSPRC;
        attributes.assprc = processor;
    }
#else
    (void)processor;
#endif
    task = tk_cre_tsk(&attributes);
    if (task < E_OK) {
        return task;
    }
    error = tk_sta_tsk(task, 0);
    if (error < E_OK) {
        (void)tk_del_tsk(task);
        return error;
    }
    return task;
}

static void halt_on_error(ER error, const char *stage)
{
    if (error < E_OK) {
        tm_printf((UB *)"AAI05 INIT FAILURE: %s error=%d; motor drive disabled\n",
                  stage, (INT)error);
        for (;;) {
            (void)tk_dly_tsk(1000);
        }
    }
}

EXPORT INT usermain(void)
{
    /* Motor pins are put into coast before creating or starting app tasks. */
    halt_on_error(subsystem2_init(), "motion init");
    halt_on_error(subsystem1_init(), "IR init");
    halt_on_error(subsystem3_init(), "report init");
    tm_printf((UB *)"\n=== AAI05 DIAGNOSTICS v0.1: NO MOTOR DRIVE ===\n");
    tm_printf((UB *)"IR: L=GP26 R=GP27; encoders: L=GP4/5 R=GP16/17\n");
    tm_printf((UB *)"Encoder counts are slow manual-check edges, not distance.\n");
    tm_printf((UB *)"No tape calibration, colour classification or MQTT session yet.\n");
#if APP_TEST
    subsystem2_test();
#endif
    /* All kernel objects exist before any application task starts. */
    halt_on_error(subsystem2_start(), "motion start");
    halt_on_error(subsystem1_start(), "IR start");
    halt_on_error(subsystem3_start(), "report start");
    for (;;) {
        (void)tk_slp_tsk(TMO_FEVR);
    }
    return 0;
}
