/* Shared harness: real owner, steering, classifier and motion policies;
 * fake IR/RGB acquisition and motor driver. Include once per test. */
#ifndef AAI05_BLACK_FOLLOW_OWNER_HARNESS_H
#define AAI05_BLACK_FOLLOW_OWNER_HARNESS_H
#define AAI05_APP_H
#define AAI05_FOLLOW_APP 1
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "black_follow.h"
#include "motion_control.h"
#include "rgb_classifier.h"
#include "../..//app_program/subsystem1/rgb_diag.h"
#include "motor_pulse.h"
#include "../../config/follow_config.h"
#if FOLLOW_DUTY_HALF_STEP
#define EXPECT_STRAIGHT 27
#define EXPECT_INNER 19
#elif FOLLOW_DUTY_LIMIT_PCT == 25
#define EXPECT_STRAIGHT 25
#if FOLLOW_TURN_LIMIT_PCT == 8
#define EXPECT_INNER 18
#else
#define EXPECT_INNER 10
#endif
#elif FOLLOW_DUTY_LIMIT_PCT == 30
#define EXPECT_STRAIGHT 30
#if FOLLOW_TURN_LIMIT_PCT == 8
#define EXPECT_INNER 21
#else
#define EXPECT_INNER 12
#endif
#elif FOLLOW_DUTY_LIMIT_PCT == 45
#define EXPECT_STRAIGHT 45
#if FOLLOW_TURN_LIMIT_PCT == 8
#define EXPECT_INNER 32
#else
#define EXPECT_INNER 18
#endif
#elif FOLLOW_DUTY_LIMIT_PCT == 40
#define EXPECT_STRAIGHT 40
#if FOLLOW_TURN_LIMIT_PCT == 8
#define EXPECT_INNER 28
#else
#define EXPECT_INNER 16
#endif
#elif FOLLOW_DUTY_LIMIT_PCT == 35
#define EXPECT_STRAIGHT 35
#if FOLLOW_TURN_LIMIT_PCT == 8
#define EXPECT_INNER 25
#else
#define EXPECT_INNER 14
#endif
#elif FOLLOW_DUTY_LIMIT_PCT == 50
#define EXPECT_STRAIGHT 50
#if FOLLOW_TURN_LIMIT_PCT == 8
#define EXPECT_INNER 36
#else
#define EXPECT_INNER 21
#endif
#else
#error No expected wheel outputs for this duty
#endif
#define EXPECT_CLIPPED_DUTY(raw) ((raw) * (2 * EXPECT_STRAIGHT + FOLLOW_DUTY_HALF_STEP) / 100)
typedef struct
{
    uint32_t sequence, sampled_ms;
    uint16_t left_raw, right_raw;
    int32_t error;
    bool valid;
} ir_snapshot_t;
static uint32_t g_now;
static bool g_ir_busy, g_rgb_busy, g_rgb_valid = true;
static bool g_active, g_wrong_target;
#if FOLLOW_MULTICOLOUR_CORRIDOR
static uint8_t g_lane_colour = RGB_CLASS_BLUE;
typedef struct
{
    uint16_t red, green, blue, clear;
} rgb_reading_t;
static const rgb_reading_t *g_join_reading;
static uint32_t g_rgb_sequence;
static uint32_t g_rgb_age_ms;
#if FOLLOW_CORNER_DIRECTION == 0 && !FOLLOW_CLIPPED_LOOP
static const rgb_reading_t g_join_unknown = {355U, 329U, 267U, 828U};
static const rgb_reading_t g_join_background = {317U, 308U, 267U, 773U};
static const rgb_reading_t g_join_ambiguous = {307U, 306U, 272U, 760U};
static const rgb_reading_t g_join_black = {298U, 300U, 272U, 745U};
#endif
static const rgb_reading_t g_dark_black = {200U, 211U, 195U, 522U};
#if FOLLOW_CLIPPED_LOOP
#if AAI05_RGB_HOME_PROFILE
/* Valid UNKNOWN/NO_MATCH from the approach to blue on the home course. */
static const rgb_reading_t g_trial_join = {205U, 223U, 226U, 536U};
#else
static const rgb_reading_t g_trial_join = {355U, 329U, 267U, 828U};
#endif
#endif
#endif
#if FOLLOW_HOME_TRACK
#define CENTRE_LEFT 339U
#define CENTRE_RIGHT 313U
#elif FOLLOW_BLUE_CORRIDOR || FOLLOW_MULTICOLOUR_CORRIDOR
#define CENTRE_LEFT 331U
#define CENTRE_RIGHT 315U
#elif FOLLOW_BORDERED_CORRIDOR
#define CENTRE_LEFT 255U
#define CENTRE_RIGHT 236U
#else
#define CENTRE_LEFT 268U
#define CENTRE_RIGHT 304U
#endif
static uint16_t g_left = CENTRE_LEFT, g_right = CENTRE_RIGHT;
static int8_t g_applied_left, g_applied_right;
static uint32_t g_lease;
static uint32_t g_cutoff_count;
static unsigned int g_drive_calls, g_arm_calls;
static int32_t g_drive_error, g_rearm_error;
static uint32_t g_apply_delay_ms;
uint32_t app_now_ms(void) {return g_now;}
int32_t subsystem1_get_snapshot(ir_snapshot_t *p_value)
{
    if (g_ir_busy) {return -50;}
    *p_value = (ir_snapshot_t){.sequence=1U, .sampled_ms=g_now,
        .left_raw=g_left, .right_raw=g_right, .valid=true};
    return 0;
}
bool rgb_diag_snapshot(rgb_snapshot_t *p_value)
{
    if (g_rgb_busy) {return false;}
#if FOLLOW_YELLOW_CORRIDOR
    bool black_sample = g_wrong_target;
#else
    bool black_sample = !g_wrong_target;
#endif
    *p_value = (rgb_snapshot_t){.red=200U, .green=211U, .blue=195U,
        .clear=522U, .sampled_ms=g_now, .device_id=0x44U, .ready=true,
        .has_sample=true, .valid=g_rgb_valid, .status=RGB_OK};
    if (!black_sample)
    {
        p_value->red=413U; p_value->green=372U; p_value->blue=281U;
        p_value->clear=910U; /* Actual corridor row missed by R2. */
    }
#if FOLLOW_MULTICOLOUR_CORRIDOR
    switch (g_wrong_target ? RGB_CLASS_BLACK : g_lane_colour)
    {
        case RGB_CLASS_BLUE:
#if AAI05_RGB_HOME_PROFILE
            p_value->red=170U; p_value->green=192U; p_value->blue=199U;
            p_value->clear=479U;
#else
            p_value->red=218U; p_value->green=249U; p_value->blue=255U;
            p_value->clear=614U;
#endif
            break;
        case RGB_CLASS_RED:
#if AAI05_RGB_HOME_PROFILE
            p_value->red=266U; p_value->green=211U; p_value->blue=208U;
            p_value->clear=560U;
#else
            p_value->red=360U; p_value->green=259U; p_value->blue=250U;
            p_value->clear=706U;
#endif
            break;
        case RGB_CLASS_YELLOW:
#if AAI05_RGB_HOME_PROFILE
            p_value->red=276U; p_value->green=257U; p_value->blue=210U;
            p_value->clear=638U;
#else
            p_value->red=413U; p_value->green=372U; p_value->blue=281U;
            p_value->clear=910U;
#endif
            break;
        case RGB_CLASS_BLACK:
            p_value->red=200U; p_value->green=211U; p_value->blue=195U;
            p_value->clear=522U;
            break;
        default:
            p_value->red=0U; p_value->green=0U; p_value->blue=0U;
            p_value->clear=0U;
            break;
    }
    if (g_join_reading != NULL)
    {
        p_value->red=g_join_reading->red; p_value->green=g_join_reading->green;
        p_value->blue=g_join_reading->blue; p_value->clear=g_join_reading->clear;
    }
    p_value->sequence=g_rgb_sequence;
    p_value->sampled_ms=g_now-g_rgb_age_ms;
#elif FOLLOW_BLUE_CORRIDOR
    if (!g_wrong_target)
    {
        p_value->red=218U; p_value->green=249U; p_value->blue=255U;
        p_value->clear=614U;
    }
#elif FOLLOW_RED_CORRIDOR
    if (!g_wrong_target)
    {
        p_value->red=360U; p_value->green=259U; p_value->blue=250U;
        p_value->clear=706U;
    }
    else
    {
        p_value->red=413U; p_value->green=372U; p_value->blue=281U;
        p_value->clear=910U; /* YELLOW must not grant RED movement. */
    }
#endif
    return true;
}
uint32_t motor_drive_now_us(void) {return g_now * 1000U;}
int32_t motor_drive_set_until(int8_t left, int8_t right, uint32_t deadline_us)
{
    ++g_drive_calls;
    if (g_drive_error != 0) {return g_drive_error;}
    g_now += g_apply_delay_ms;
    uint32_t remaining=deadline_us-g_now*1000U;
    if(remaining==0U || remaining>100000U){return -50;}
    g_active = true; g_applied_left=left; g_applied_right=right; g_lease=remaining/1000U;
    return 0;
}
#if FOLLOW_MOTOR_HALF_PCT
int32_t motor_drive_set_until_half_pct(int8_t left, int8_t right, uint32_t deadline_us)
{
    return motor_drive_set_until((int8_t)(left / 2), (int8_t)(right / 2), deadline_us);
}
#endif
int32_t motor_drive_rearm(void)
{
    ++g_arm_calls;
    assert(!g_active);
    return g_rearm_error;
}
void motor_pulse_stop(void)
{g_active=false; g_applied_left=0; g_applied_right=0; g_lease=0U;}

#include "../../app_program/subsystem2/black_follow_owner.c"
#endif
