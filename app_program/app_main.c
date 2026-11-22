/* AAI05 diagnostic application. Three logical modules, one development effort. */
#include "app.h"
#if AAI05_RGB_ACTIVE
#include "subsystem1/rgb_diag.h"
#endif

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
#if AAI05_RGB_ACTIVE
    ER rgb_init_error = (ER)rgb_diag_init();
    if (rgb_init_error < E_OK)
    {
#if AAI05_FOLLOW_APP
        halt_on_error(rgb_init_error, "RGB init");
#else
        tm_printf((UB *)"RGB snapshot unavailable: err=%d; IR/report continue.\n",
                  (INT)rgb_init_error);
#endif
    }
#if AAI05_RGB_DIAG
    tm_printf((UB *)"\n=== AAI05 RGB CLASSIFIER v0.12.1: NO MOTOR OUTPUT ===\n");
    tm_printf((UB *)"TCS34725: I2C1 SDA=GP2 SCL=GP3 addr=0x29; "
                       "fixed 4x / 50.4ms. BLACK/WHITE/YELLOW/BACKGROUND/RED/BLUE.\n");
#endif
#endif
#if AAI05_FOLLOW_APP
#if FOLLOW_CLIPPED_LOOP
#if AAI05_FOLLOW_MQTT
    tm_printf((UB *)"\n=== AAI05 MQTT FOLLOW v1: v0.87 steering, %ums, join_max=%ums ===\n",
#elif FOLLOW_TWO_TURN
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.28: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_RED_SPIN_HOLD
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.87: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_RED_CORNER_REFINE
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.86: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_RED_RIGHT_EDGE_OUTSIDE
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.85: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_RED_OUTSIDE_RECOVERY
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.84: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_RED_PIVOT_INSIDE_RELEASE
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.83: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_BLUE_RED_JOIN_EXTEND
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.82: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_RED_FINAL_ARC_STRONG
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.81: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_RED_FINAL_ARC_EXTEND
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.80: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_RED_DIAGONAL_REACQUIRE
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.79: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_BLUE_RED_LATE_APPROACH
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.78: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_BLUE_EXIT_EDGE_CONFIRM
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.77: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_RED_FINAL_LEFT_BOOST
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.76: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_BLUE_SECOND_LEFT_BOOST || FOLLOW_RED_SECOND_ARC_BOOST
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.75: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_RED_FIRST_LEFT_BOOST
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.74: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_BLUE_EXIT_LEFT_BOOST
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.73: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_BLUE_RED_BLEND_MARGIN
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.72: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_BLUE_RED_BLEND_GRACE
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.71: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_BLUE_RED_LATE_CONFIRM
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.70: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_BLUE_EXIT_PIVOT
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.69: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_RED_EXIT_TURN_HOLD
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.68: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_RED_EXIT_TURN_STRONG
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.67: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_RED_RETURN_GRACE
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.66: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_RED_EXIT_TURN_TIGHTER
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.65: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_RED_EXIT_TURN_EARLY
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.64: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_RED_EXIT_TURN
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.63: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_RED_INSIDE_RELEASE
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.62: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_RED_BETWEEN_BENDS
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.61: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_RED_EXIT_ALIGN
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.60: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_RED_HANDOFF_ALIGN
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.59: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_BLUE_DIAGONAL_EXIT
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.58: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_RED_SECOND_EXIT_REFINEMENT
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.57: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_BEND_CUE_REFINEMENT
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.56: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_BLUE_RED_JOIN_REFINEMENT
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.55: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_RED_FIRST_BEND_TIGHTER
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.54: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_RED_FIRST_BEND_BOOST
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.53: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_RED_BEND_AFTER_HANDOFF
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.52: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_RED_SECOND_SLOW
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.51: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_RED_SECOND_PIVOT
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.50: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_BLUE_RED_BEND_CUES
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.49: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_BLUE_RED_BEND_TRIAL
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.48: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_RED_EXIT_EARLY
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.47: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_RED_EXIT_LEAD
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.46: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_BLUE_EXIT_AFTER_RECOVERY
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.45: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_BLUE_RED_CONFIRM_GRACE
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.44: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_BLUE_EXIT_TURN == 22
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.43: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_BLUE_EXIT_TURN == 18
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.42: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_BLUE_EXIT_LEAD
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.41: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_BEND_TRACE
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.40: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_BLUE_CONFIRM_GRACE
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.39: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_BLUE_BEND_SLOW
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.38: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_BLUE_OUTER_RECOVERY
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.37: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_BLUE_OUTER_EDGE
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.36: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_BLUE_SECOND_BOOST
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.35: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_BLUE_RED_HANDOFF
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.34: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_STOP_TRACE
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.33: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_BLUE_STEER_WIDE_RANGE
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.32: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_BLUE_REVERSAL_DAMPING
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.31: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_YELLOW_LEAD_CUE
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.30: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_YELLOW_EXIT_CUE
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.29: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_BLUE_STEER_RAMP
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v" FOLLOW_BLUE_RAMP_VERSION
                       ": LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_HOME_TRACK
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.25: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_EARLY_BEND
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.24: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_LOOP_RECOVERY_DIRECTION != 0
    tm_printf((UB *)"\n=== AAI05 " FOLLOW_TRACK_NAME " v0.23: LOCAL %ums, join_max=%ums ===\n",
#elif FOLLOW_BLUE_BEND_RIGHT
    tm_printf((UB *)"\n=== AAI05 BLUE BEND RIGHT v0.22: LOCAL %ums, join_max=%ums ===\n",
#else
    tm_printf((UB *)"\n=== AAI05 CLIPPED LOOP v0.21: LOCAL %ums, join_max=%ums ===\n",
#endif
              (UINT)FOLLOW_CLIPPED_RUN_MS, (UINT)FOLLOW_JOIN_MAX_MS);
    tm_printf((UB *)"Forward steering: base=%u peak=%u.\n",
              (UINT)FOLLOW_TURN_LIMIT_PCT, (UINT)FOLLOW_TURN_PEAK_PCT);
#elif FOLLOW_CORNER_DIRECTION != 0
    tm_printf((UB *)"\n=== AAI05 CORNER TEST v0.19: " FOLLOW_CORNER_NAME
                       ", LOCAL %ums ===\n", (UINT)FOLLOW_CORNER_RUN_MS);
#elif FOLLOW_MULTICOLOUR_CORRIDOR
    tm_printf((UB *)"\n=== AAI05 MULTICOLOUR FOLLOW v0.18: LOCAL 1000ms, join_max=%ums ===\n",
              (UINT)FOLLOW_JOIN_MAX_MS);
#elif FOLLOW_BLUE_CORRIDOR
    tm_printf((UB *)"\n=== AAI05 BLUE FOLLOW v0.16: LOCAL 1000ms EXERCISE ===\n");
#elif FOLLOW_RED_CORRIDOR
    tm_printf((UB *)"\n=== AAI05 RED FOLLOW v0.15: LOCAL 1000ms EXERCISE ===\n");
#elif FOLLOW_YELLOW_CORRIDOR
    tm_printf((UB *)"\n=== AAI05 YELLOW FOLLOW v0.14: LOCAL 1000ms EXERCISE ===\n");
#else
    tm_printf((UB *)"\n=== AAI05 BLACK FOLLOW v0.13.3: LOCAL 1000ms EXERCISE ===\n");
#endif
#if AAI05_FOLLOW_MQTT
    tm_printf((UB *)"Output inhibit=%u; MQTT FOLLOW starts; GP21 STOP.\n",
                      (UINT)MOTOR_TEST_PROBE);
#else
    tm_printf((UB *)"Output inhibit=%u; no network commands. "
                       "GP20 fresh press starts; GP21 STOP.\n", (UINT)MOTOR_TEST_PROBE);
#endif
    tm_printf((UB *)"Wheel duty limit=%u" FOLLOW_DUTY_DISPLAY_SUFFIX "%%; nominal steering bounded%u%%; "
                       "fresh IR and RGB " FOLLOW_TARGET_NAME " required. No automatic search.\n",
                       (UINT)FOLLOW_DUTY_LIMIT_PCT, (UINT)FOLLOW_TURN_PEAK_PCT);
#elif AAI05_RGB_DIAG
    /* RGB banner printed above; do not also identify this as an old image. */
#elif AAI05_REMOTE_APP && AAI05_ENCODER_PIO
    tm_printf((UB *)"\n=== AAI05 MQTT MOTION v0.9: PIO ENCODER CAPTURE ===\n");
#elif AAI05_REMOTE_APP
    tm_printf((UB *)"\n=== AAI05 MQTT MOTION v0.8: EXPLICIT REMOTE START ===\n");
#elif AAI05_DISTANCE_RUN
    tm_printf((UB *)"\n=== AAI05 DISTANCE RUN R2: 2S STRAIGHT D25 ===\n");
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
#if AAI05_ENCODER_PIO
    tm_printf((UB *)"PIO capture: 1 MHz state-machine clock; raw counts L-/R+ forward.\n");
    tm_printf((UB *)"PIO_BAD flags are sticky; GPIO irq/over counters do not apply.\n");
#else
    tm_printf((UB *)"Encoder IRQ capture: core 0; counts are edges, not distance.\n");
#endif
#if TM_NET_LWIP
#if AAI05_FOLLOW_MQTT
    tm_printf((UB *)"MQTT FOLLOW/STRAIGHT/KEEPALIVE/STOP and encoder distance; 340 mm calibration.\n");
#elif AAI05_REMOTE_APP
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
#if AAI05_RGB_ACTIVE
    tm_printf((UB *)"RGB/IR image: USB diagnostics; MQTT disabled in this build.\n");
#else
    tm_printf((UB *)"Motor image: USB diagnostics; MQTT disabled in this build.\n");
#endif
#endif
#if AAI05_FOLLOW_MQTT
    tm_printf((UB *)"No drive on boot or reconnect. Fresh MQTT FOLLOW is required.\n");
#elif AAI05_FOLLOW_APP
    tm_printf((UB *)"No drive on boot. Each run stops at nominal1000ms, "
                       "sensor loss or GP21; fresh release/press needed to restart.\n");
#elif AAI05_RGB_DIAG
    tm_printf((UB *)"Read-only RGB/IR diagnostics. GP20 is not needed; "
                       "all firmware motor output is disabled.\n");
#elif AAI05_REMOTE_APP
    tm_printf((UB *)"GP20 local run is disabled. GP21 STOP; only fresh MQTT "
                       "START can rearm after GP21 is released.\n");
    tm_printf((UB *)"Open-loop electrical duties only; no calibrated speed "
                       "or physical-forward convention.\n");
#elif AAI05_DISTANCE_RUN
    tm_printf((UB *)"GP20: one 2 s run at 25%% forward duty per wheel. "
                       "GP21 STOP; reset to run again.\n");
    tm_printf((UB *)"Record ENC before GP20 and after the wheels stop. "
                       "PIO_BAD must stay 0/0.\n");
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
#if AAI05_RGB_DIAG
    tm_printf((UB *)"Keep car stationary. Do not press the M1/M2 hardware test buttons.\n");
#else
    tm_printf((UB *)"WHEELS RAISED. No drive on boot. Never use the M1/M2 hardware test buttons.\n");
#endif
#if APP_TEST
    subsystem2_test();
#endif
    /* All kernel objects exist before any application task starts. */
#if AAI05_FOLLOW_APP
    /* Finish mutable RGB startup availability before any Motion consumer. */
    halt_on_error(subsystem1_start(), "IR start");
    halt_on_error((ER)rgb_diag_start(), "RGB start");
    halt_on_error(subsystem2_start(), "motion start");
#else
    halt_on_error(subsystem2_start(), "motion start");
    halt_on_error(subsystem1_start(), "IR start");
#if AAI05_RGB_ACTIVE
    ER rgb_start_error = (ER)rgb_diag_start();
    if (rgb_start_error < E_OK)
    {
        tm_printf((UB *)"RGB task unavailable: err=%d; IR/report continue.\n",
                  (INT)rgb_start_error);
    }
#endif
#endif
    halt_on_error(subsystem3_start(), "report start");
    for (;;) {
        (void)tk_slp_tsk(TMO_FEVR);
    }
    return 0;
}
