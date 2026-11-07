/* AAI05 diagnostic application. Three logical modules, one development effort. */
#include "app.h"

EXPORT uint32_t app_now_ms(void)
{
    SYSTIM now;
    (void)tk_get_otm(&now);
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
#if AAI05_REMOTE_APP
    tm_printf((UB *)"\n=== AAI05 MQTT MOTION v0.8: EXPLICIT REMOTE START ===\n");
#elif AAI05_MOTION_APP
    tm_printf((UB *)"\n=== AAI05 MOTION v0.7: LOCAL OPEN-LOOP EXERCISE ===\n");
#elif AAI05_COMMS_APP
    tm_printf((UB *)"\n=== AAI05 COMMS v0.5: NO MOTOR OUTPUT ===\n");
#elif TM_NET_LWIP && MOTOR_TEST_PROBE
    tm_printf((UB *)"\n=== AAI05 NETWORK PROBE v0.4: NO MOTOR OUTPUT ===\n");
#elif MOTOR_TEST_PROBE
    tm_printf((UB *)"\n=== AAI05 MOTOR PROBE v0.3p: NO MOTOR OUTPUT ===\n");
#else
    tm_printf((UB *)"\n=== AAI05 MOTOR PWM v0.6.1: FIXED-LOW INPUT ===\n");
    tm_printf((UB *)"Encoder quality warnings are nonfatal in this "
                       "bounded open-loop diagnostic.\n");
#endif
    tm_printf((UB *)"IR: L=GP26 R=GP27; encoders: L=GP4/5 R=GP16/17\n");
    tm_printf((UB *)"Encoder IRQ capture: core 0; counts are edges, not distance.\n");
#if TM_NET_LWIP
#if AAI05_REMOTE_APP
    tm_printf((UB *)"v2 MQTT START/KEEPALIVE/STOP;750 ms remote lease, "
                       "100 ms internal lease. No auto-start on reconnect.\n");
    tm_printf((UB *)"Raw IR/target storage; detected colours remain unknown.\n");
#elif AAI05_COMMS_APP
    tm_printf((UB *)"Binary target selection/ACK/raw IR; colours not calibrated.\n");
    tm_printf((UB *)"No movement command or colour classification in this image.\n");
#else
    tm_printf((UB *)"Network probe: association/IP and template MQTT heartbeat/received text.\n");
    tm_printf((UB *)"No colour calibration, app command handling or sensor MQTT telemetry yet.\n");
#endif
#else
    tm_printf((UB *)"Motor image: USB diagnostics; MQTT disabled in this build.\n");
#endif
#if AAI05_REMOTE_APP
    tm_printf((UB *)"GP20 local run is disabled. GP21 STOP; only fresh MQTT "
                       "START can rearm after GP21 is released.\n");
    tm_printf((UB *)"Open-loop electrical duties only; no calibrated speed "
                       "or physical-forward convention.\n");
#elif AAI05_MOTION_APP
    tm_printf((UB *)"GP20: one fresh press renews left D1/50%% commands "
                       "for 2 seconds; right wheel coasts.\n");
    tm_printf((UB *)"100 ms command/hardware lease. GP21 or expiry latches "
                       "STOPPED until reset. No automatic restart.\n");
    tm_printf((UB *)"Encoder warnings remain diagnostic; no speed feedback "
                       "or calibrated physical-forward convention.\n");
#else
    tm_printf((UB *)"GP20 NEXT: M1 D1, M1 D2, M2 D1, M2 D2. GP21 STOP latches until reset.\n");
#if MOTOR_TEST_PROBE
    tm_printf((UB *)"PROBE: 400 ms virtual pulse; both inputs remain high/coast. DRIVE stays OFF.\n");
#else
    tm_printf((UB *)"One wheel, %u percent duty, 400 ms timer cutoff; wait for IDLE before each fresh press.\n",
              (UINT)MOTOR_PULSE_MAX_DUTY);
#endif
#endif
    tm_printf((UB *)"WHEELS RAISED. No drive on boot. Never use the M1/M2 hardware test buttons.\n");
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
