/* Compile the real network snapshot adapter with local subsystem substitutes. */
#define AAI05_APP_H
#define E_OK 0
#define E_TMOUT (-50)
#include "drive_service.h"
#include "motor_pulse.h"
#include "cyw43_utk.h"
#include "mqtt_utk.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    uint32_t sequence;
    uint32_t sampled_ms;
    motion_status_t motion;
    motor_pulse_status_t motor;
    int32_t capture_error;
    bool capture_ready;
    bool count_saturated;
} motion_snapshot_t;
typedef struct
{
    uint32_t sequence;
    uint32_t sampled_ms;
    uint16_t left_raw;
    uint16_t right_raw;
    int32_t error;
    bool valid;
} ir_snapshot_t;
static motion_snapshot_t source_motion;
static ir_snapshot_t source_ir;
static uint32_t now;
static bool busy;
uint32_t app_now_ms(void) {return now;}
int32_t subsystem2_get_snapshot(motion_snapshot_t *p_motion)
{
    if (busy) return E_TMOUT;
    *p_motion = source_motion;
    return E_OK;
}
int32_t subsystem1_get_snapshot(ir_snapshot_t *p_ir) {*p_ir = source_ir; return E_OK;}
void cyw43_utk_get_status(T_CYW43_UTK_STATUS *p_radio)
{
    *p_radio = (T_CYW43_UTK_STATUS){.ready = 1U, .link_status = CYW43_UTK_LINK_UP};
}
int32_t mqtt_utk_publish_binary(const char *p_topic, const uint8_t *p_data, uint16_t length, bool reliable)
{
    (void)p_topic; (void)p_data; (void)reliable;
    return length;
}
#include "../../app_program/subsystem3/drive_platform_rp2040.c"

int main(void)
{
    drive_input_t input;
    now = 100U;
    source_motion.sequence = 1U;
    source_motion.sampled_ms = now;
    source_motion.capture_ready = true;
    source_motion.motor.ready = true;
    source_ir = (ir_snapshot_t){.sequence = 1U, .sampled_ms = 100U, .valid = true};
    drive_io_snapshot(&input, now);
    assert(input.motion.is_available && input.is_wifi_up && input.is_ir_valid);
    const int32_t errors[] = {-50, -41, -57};
    for (unsigned int index = 0U; index < 3U; ++index)
    {
        source_motion.capture_error = errors[index];
        drive_io_snapshot(&input, now);
        assert(!input.motion.is_available);
    }
    busy = true;
    now = 201U;
    drive_io_snapshot(&input, now);
    assert(!input.motion.is_available && input.is_ir_stale);
    puts("PASS: capture runtime faults and stale cache cannot authorize remote motion");
    return 0;
}
