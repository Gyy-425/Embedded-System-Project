/* Exercise actual startup ordering and RGB-start failure without hardware. */
#define AAI05_APP_H
#define AAI05_FOLLOW_APP 1
#define AAI05_RGB_ACTIVE 1
#define AAI05_RGB_DIAG 0
#define AAI05_ENCODER_PIO 1
#define AAI05_REMOTE_APP 0
#define AAI05_MOTION_APP 0
#define AAI05_COMMS_APP 0
#define MOTOR_TEST_PROBE 0
#define TM_NET_LWIP 0
#define TK_SUPPORT_SMP 1
#define APP_TEST 0
#define TASK_STACK_SIZE 2048
#define EXPORT
#define E_OK 0
#define TMO_FEVR (-1)
#define TA_HLNG 1U
#define TA_RNG3 2U
#define TA_ASSPRC 4U
#include <assert.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "rgb_diag.h"
#include "../../config/follow_config.h"
typedef int ID, ER, INT, PRI;
typedef unsigned int UINT, UW;
typedef unsigned char UB;
typedef void (*FP)(INT,void *);
typedef struct {FP task;int itskpri,stksz;unsigned int tskatr,assprc;} T_CTSK;
typedef struct {uint32_t lo;} SYSTIM;
static jmp_buf g_done;
static unsigned int g_events[16],g_event_count;
static bool g_fail_rgb;
static char g_output[2048];
static size_t g_output_size;
static void event(unsigned int code) {g_events[g_event_count++]=code;}
static int tk_get_otm(SYSTIM *p_now) {p_now->lo=0U;return 0;}
static int tk_cre_tsk(const T_CTSK *p_attr) {(void)p_attr;return 1;}
static int tk_sta_tsk(int task,int code) {(void)task;(void)code;return 0;}
static int tk_del_tsk(int task) {(void)task;return 0;}
static int tk_slp_tsk(int timeout) {(void)timeout;longjmp(g_done,2);}
static int tk_dly_tsk(int delay) {(void)delay;longjmp(g_done,1);}
static void tm_printf(const UB *p_format,...)
{
    va_list args;
    va_start(args,p_format);
    int count=vsnprintf(g_output+g_output_size,sizeof(g_output)-g_output_size,
                        (const char *)p_format,args);
    va_end(args);
    assert(count>=0 && (size_t)count<sizeof(g_output)-g_output_size);
    g_output_size+=(size_t)count;
}
int subsystem2_init(void) {event(1U);return 0;}
int subsystem1_init(void) {event(2U);return 0;}
int subsystem3_init(void) {event(3U);return 0;}
int32_t rgb_diag_init(void) {event(4U);return 0;}
int subsystem1_start(void) {event(5U);return 0;}
int32_t rgb_diag_start(void) {event(6U);return g_fail_rgb ? -42 : 0;}
int subsystem2_start(void) {event(7U);return 0;}
int subsystem3_start(void) {event(8U);return 0;}
#include "../../app_program/app_main.c"
int main(void)
{
    int result=setjmp(g_done);
    if(result==0){(void)usermain();}
    assert(result==2 && g_event_count==8U);
#if FOLLOW_CLIPPED_LOOP
#if FOLLOW_TWO_TURN
    assert(strstr(g_output,"HOME_TRACK_TWO_TURN v0.28")!=NULL);
#elif FOLLOW_RED_SPIN_HOLD
    assert(strstr(g_output,"HOME_TRACK_RED_SPIN_HOLD v0.87")!=NULL);
#elif FOLLOW_RED_CORNER_REFINE
    assert(strstr(g_output,"HOME_TRACK_RED_CORNER_REFINE v0.86")!=NULL);
#elif FOLLOW_RED_RIGHT_EDGE_OUTSIDE
    assert(strstr(g_output,"HOME_TRACK_RED_RIGHT_EDGE_OUTSIDE v0.85")!=NULL);
#elif FOLLOW_RED_OUTSIDE_RECOVERY
    assert(strstr(g_output,"HOME_TRACK_RED_OUTSIDE_RECOVERY v0.84")!=NULL);
#elif FOLLOW_RED_PIVOT_INSIDE_RELEASE
    assert(strstr(g_output,"HOME_TRACK_RED_PIVOT_INSIDE_RELEASE v0.83")!=NULL);
#elif FOLLOW_BLUE_RED_JOIN_EXTEND
    assert(strstr(g_output,"HOME_TRACK_BLUE_RED_JOIN_EXTEND v0.82")!=NULL);
#elif FOLLOW_RED_FINAL_ARC_STRONG
    assert(strstr(g_output,"HOME_TRACK_RED_FINAL_ARC_STRONG v0.81")!=NULL);
#elif FOLLOW_RED_FINAL_ARC_EXTEND
    assert(strstr(g_output,"HOME_TRACK_RED_FINAL_ARC_EXTEND v0.80")!=NULL);
#elif FOLLOW_RED_DIAGONAL_REACQUIRE
    assert(strstr(g_output,"HOME_TRACK_RED_DIAGONAL_REACQUIRE v0.79")!=NULL);
#elif FOLLOW_BLUE_RED_LATE_APPROACH
    assert(strstr(g_output,"HOME_TRACK_BLUE_RED_LATE_APPROACH v0.78")!=NULL);
#elif FOLLOW_BLUE_EXIT_EDGE_CONFIRM
    assert(strstr(g_output,"HOME_TRACK_BLUE_EXIT_EDGE_CONFIRM v0.77")!=NULL);
#elif FOLLOW_RED_FINAL_LEFT_BOOST
    assert(strstr(g_output,"HOME_TRACK_RED_FINAL_LEFT_BOOST v0.76")!=NULL);
#elif FOLLOW_BLUE_SECOND_LEFT_BOOST
    assert(strstr(g_output,"HOME_TRACK_BEND_POWER v0.75")!=NULL);
#elif FOLLOW_RED_SECOND_ARC_BOOST
    assert(strstr(g_output,"HOME_TRACK_RED_SECOND_ARC_BOOST v0.75")!=NULL);
#elif FOLLOW_RED_FIRST_LEFT_BOOST
    assert(strstr(g_output,"HOME_TRACK_RED_FIRST_LEFT_BOOST v0.74")!=NULL);
#elif FOLLOW_BLUE_EXIT_LEFT_BOOST
    assert(strstr(g_output,"HOME_TRACK_BLUE_EXIT_LEFT_BOOST v0.73")!=NULL);
#elif FOLLOW_BLUE_RED_BLEND_MARGIN
    assert(strstr(g_output,"HOME_TRACK_BLUE_RED_BLEND_MARGIN v0.72")!=NULL);
#elif FOLLOW_BLUE_RED_BLEND_GRACE
    assert(strstr(g_output,"HOME_TRACK_BLUE_RED_BLEND_GRACE v0.71")!=NULL);
#elif FOLLOW_BLUE_RED_LATE_CONFIRM
    assert(strstr(g_output,"HOME_TRACK_BLUE_RED_LATE_CONFIRM v0.70")!=NULL);
#elif FOLLOW_BLUE_EXIT_PIVOT
    assert(strstr(g_output,"HOME_TRACK_BLUE_EXIT_PIVOT v0.69")!=NULL);
#elif FOLLOW_RED_EXIT_TURN_HOLD
    assert(strstr(g_output,"HOME_TRACK_RED_EXIT_TURN_HOLD v0.68")!=NULL);
#elif FOLLOW_RED_EXIT_TURN_STRONG
    assert(strstr(g_output,"HOME_TRACK_RED_EXIT_TURN_STRONG v0.67")!=NULL);
#elif FOLLOW_RED_RETURN_GRACE
    assert(strstr(g_output,"HOME_TRACK_RED_RETURN_GRACE v0.66")!=NULL);
#elif FOLLOW_RED_EXIT_TURN_TIGHTER
    assert(strstr(g_output,"HOME_TRACK_RED_EXIT_TURN_TIGHTER v0.65")!=NULL);
#elif FOLLOW_RED_EXIT_TURN_EARLY
    assert(strstr(g_output,"HOME_TRACK_RED_EXIT_TURN_EARLY v0.64")!=NULL);
#elif FOLLOW_RED_EXIT_TURN
    assert(strstr(g_output,"HOME_TRACK_RED_EXIT_TURN v0.63")!=NULL);
#elif FOLLOW_RED_INSIDE_RELEASE
    assert(strstr(g_output,"HOME_TRACK_RED_INSIDE_RELEASE v0.62")!=NULL);
#elif FOLLOW_RED_BETWEEN_BENDS
    assert(strstr(g_output,"HOME_TRACK_RED_BETWEEN_BENDS v0.61")!=NULL);
#elif FOLLOW_RED_EXIT_ALIGN
    assert(strstr(g_output,"HOME_TRACK_RED_EXIT_ALIGN v0.60")!=NULL);
#elif FOLLOW_RED_HANDOFF_ALIGN
    assert(strstr(g_output,"HOME_TRACK_RED_HANDOFF_ALIGN v0.59")!=NULL);
#elif FOLLOW_BLUE_DIAGONAL_EXIT
    assert(strstr(g_output,"HOME_TRACK_BLUE_DIAGONAL_EXIT v0.58")!=NULL);
#elif FOLLOW_RED_SECOND_EXIT_REFINEMENT
    assert(strstr(g_output,"HOME_TRACK_RED_SECOND_EXIT_REFINEMENT v0.57")!=NULL);
#elif FOLLOW_BEND_CUE_REFINEMENT
    assert(strstr(g_output,"HOME_TRACK_BEND_CUE_REFINEMENT v0.56")!=NULL);
#elif FOLLOW_BLUE_RED_JOIN_REFINEMENT
    assert(strstr(g_output,"HOME_TRACK_BLUE_RED_JOIN_REFINEMENT v0.55")!=NULL);
#elif FOLLOW_RED_FIRST_BEND_TIGHTER
    assert(strstr(g_output,"HOME_TRACK_RED_FIRST_BEND_TIGHTER v0.54")!=NULL);
#elif FOLLOW_RED_FIRST_BEND_BOOST
    assert(strstr(g_output,"HOME_TRACK_RED_FIRST_BEND_BOOST v0.53")!=NULL);
#elif FOLLOW_RED_BEND_AFTER_HANDOFF
    assert(strstr(g_output,"HOME_TRACK_RED_BEND_AFTER_HANDOFF v0.52")!=NULL);
#elif FOLLOW_RED_SECOND_SLOW
    assert(strstr(g_output,"HOME_TRACK_RED_SECOND_SLOW v0.51")!=NULL);
#elif FOLLOW_RED_SECOND_PIVOT
    assert(strstr(g_output,"HOME_TRACK_RED_SECOND_PIVOT v0.50")!=NULL);
#elif FOLLOW_BLUE_RED_BEND_CUES
    assert(strstr(g_output,"HOME_TRACK_BLUE_RED_BEND_CUES v0.49")!=NULL);
#elif FOLLOW_BLUE_RED_BEND_TRIAL
    assert(strstr(g_output,"HOME_TRACK_BLUE_RED_BENDS v0.48")!=NULL);
#elif FOLLOW_RED_EXIT_EARLY
    assert(strstr(g_output,"HOME_TRACK_RED_EXIT_EARLY v0.47")!=NULL);
#elif FOLLOW_RED_EXIT_LEAD
    assert(strstr(g_output,"HOME_TRACK_RED_EXIT_LEAD v0.46")!=NULL);
#elif FOLLOW_BLUE_EXIT_AFTER_RECOVERY
    assert(strstr(g_output,"HOME_TRACK_BLUE_EXIT_AFTER_RECOVERY v0.45")!=NULL);
#elif FOLLOW_BLUE_RED_CONFIRM_GRACE
    assert(strstr(g_output,"HOME_TRACK_RED_CONFIRM v0.44")!=NULL);
#elif FOLLOW_BLUE_EXIT_TURN == 22
    assert(strstr(g_output,"HOME_TRACK_BLUE_EXIT_EARLY v0.43")!=NULL);
#elif FOLLOW_BLUE_EXIT_TURN == 18
    assert(strstr(g_output,"HOME_TRACK_BLUE_EXIT_TIGHT v0.42")!=NULL);
#elif FOLLOW_BLUE_EXIT_LEAD
    assert(strstr(g_output,"HOME_TRACK_BLUE_EXIT_LEAD v0.41")!=NULL);
#elif FOLLOW_BEND_TRACE
    assert(strstr(g_output,"HOME_TRACK_BEND_TRACE v0.40")!=NULL);
#elif FOLLOW_BLUE_CONFIRM_GRACE
    assert(strstr(g_output,"HOME_TRACK_BLUE_CONFIRM v0.39")!=NULL);
#elif FOLLOW_BLUE_BEND_SLOW
    assert(strstr(g_output,"HOME_TRACK_BLUE_BEND_SLOW v0.38")!=NULL);
#elif FOLLOW_BLUE_OUTER_RECOVERY
    assert(strstr(g_output,"HOME_TRACK_BLUE_OUTER_RECOVERY v0.37")!=NULL);
#elif FOLLOW_BLUE_OUTER_EDGE
    assert(strstr(g_output,"HOME_TRACK_BLUE_OUTER_EDGE v0.36")!=NULL);
#elif FOLLOW_BLUE_SECOND_BOOST
    assert(strstr(g_output,"HOME_TRACK_BLUE_SECOND_BOOST v0.35")!=NULL);
#elif FOLLOW_BLUE_RED_HANDOFF
    assert(strstr(g_output,"HOME_TRACK_BLUE_RED_HANDOFF v0.34")!=NULL);
#elif FOLLOW_STOP_TRACE
    assert(strstr(g_output,"HOME_TRACK_STOP_TRACE v0.33")!=NULL);
#elif FOLLOW_BLUE_STEER_WIDE_RANGE
    assert(strstr(g_output,"HOME_TRACK_BLUE_WIDE_RANGE v0.32")!=NULL);
#elif FOLLOW_BLUE_REVERSAL_DAMPING
    assert(strstr(g_output,"HOME_TRACK_BLUE_DAMPED v0.31")!=NULL);
#elif FOLLOW_YELLOW_LEAD_CUE
    assert(strstr(g_output,"HOME_TRACK_YELLOW_LEAD v0.30")!=NULL);
#elif FOLLOW_YELLOW_EXIT_CUE
    assert(strstr(g_output,"HOME_TRACK_YELLOW_APPROACH v0.29")!=NULL);
#elif FOLLOW_BLUE_STEER_RAMP && FOLLOW_BLUE_RAMP_START == 5
    assert(strstr(g_output,"HOME_TRACK_BLUE_GENTLE v0.27")!=NULL);
#elif FOLLOW_BLUE_STEER_RAMP
    assert(strstr(g_output,"HOME_TRACK_BLUE_RAMP v0.26")!=NULL);
#elif FOLLOW_HOME_TRACK
    assert(strstr(g_output,"HOME_TRACK_CLOCKWISE v0.25")!=NULL);
#elif FOLLOW_EARLY_BEND
    assert(strstr(g_output,"EARLY_BEND_CLOCKWISE v0.24")!=NULL);
#elif FOLLOW_LOOP_RECOVERY_DIRECTION == 1
    assert(strstr(g_output,"LOOP_CLOCKWISE v0.23")!=NULL);
#elif FOLLOW_LOOP_RECOVERY_DIRECTION == -1
    assert(strstr(g_output,"LOOP_ANTICLOCKWISE v0.23")!=NULL);
#elif FOLLOW_BLUE_BEND_RIGHT
    assert(strstr(g_output,"BLUE BEND RIGHT v0.22")!=NULL);
    assert(strcmp(FOLLOW_TRACK_NAME,"BLUE_BEND_RIGHT")==0);
#else
    assert(strstr(g_output,"CLIPPED LOOP v0.21")!=NULL);
    assert(strcmp(FOLLOW_TRACK_NAME,"CLIPPED_LOOP")==0);
#endif
    assert(strstr(g_output,"base=8 peak=14")!=NULL);
    assert(strstr(g_output,"6000ms")!=NULL);
    assert(strstr(g_output,"join_max=500ms")!=NULL);
#if FOLLOW_DUTY_HALF_STEP
    assert(strstr(g_output,"Wheel duty limit=27.5%")!=NULL);
#elif FOLLOW_DUTY_LIMIT_PCT == 30
    assert(strstr(g_output,"Wheel duty limit=30%")!=NULL);
#else
    assert(strstr(g_output,"Wheel duty limit=25%")!=NULL);
#endif
#elif FOLLOW_CORNER_DIRECTION != 0
    assert(strstr(g_output,"CORNER TEST v0.19")!=NULL);
    assert(strstr(g_output,FOLLOW_CORNER_NAME)!=NULL);
    assert(strstr(g_output,"Wheel duty limit=25%")!=NULL);
#elif FOLLOW_MULTICOLOUR_CORRIDOR
    assert(strstr(g_output,"MULTICOLOUR FOLLOW v0.18")!=NULL);
    assert(strstr(g_output,"join_max=500ms")!=NULL);
    assert(strstr(g_output,"RGB RED/YELLOW/BLUE required")!=NULL);
    assert(strstr(g_output,"Wheel duty limit=25%")!=NULL);
    assert(strcmp(FOLLOW_TRACK_NAME,"MULTICOLOUR_CORRIDOR")==0);
#elif FOLLOW_BLUE_CORRIDOR
    assert(strstr(g_output,"BLUE FOLLOW v0.16")!=NULL);
    assert(strstr(g_output,"RGB BLUE required")!=NULL);
    assert(strcmp(FOLLOW_TRACK_NAME,"BLUE_CORRIDOR")==0);
#elif FOLLOW_RED_CORRIDOR
    assert(strstr(g_output,"RED FOLLOW v0.15")!=NULL);
    assert(strstr(g_output,"RGB RED required")!=NULL);
    assert(strstr(g_output,"BLACK FOLLOW")==NULL);
#elif FOLLOW_YELLOW_CORRIDOR
    assert(strstr(g_output,"YELLOW FOLLOW v0.14")!=NULL);
    assert(strstr(g_output,"RGB YELLOW required")!=NULL);
    assert(strstr(g_output,"BLACK FOLLOW")==NULL);
#else
    assert(strstr(g_output,"BLACK FOLLOW v0.13.3")!=NULL);
    assert(strstr(g_output,"RGB BLACK required")!=NULL);
#endif
    for(unsigned int i=0U;i<8U;++i){assert(g_events[i]==i+1U);}
    g_event_count=0U;g_fail_rgb=true;
    result=setjmp(g_done);
    if(result==0){(void)usermain();}
    assert(result==1 && g_event_count==6U);
    for(unsigned int i=0U;i<6U;++i){assert(g_events[i]==i+1U);}
    puts("Follow startup: RGB ready before Motion; failed RGB never starts Motion PASS");
    return 0;
}
