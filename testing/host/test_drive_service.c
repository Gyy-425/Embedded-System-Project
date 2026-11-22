#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "drive_service.h"
#include "comm_protocol.h"
#include "motion_runtime.h"

static uint32_t g_now;
static uint32_t g_seed = 123U;
static drive_input_t g_input;
static uint8_t g_ack[DRIVE_ACK_SIZE];
static uint8_t g_telemetry[DRIVE_TELEMETRY_SIZE];
static uint32_t g_arm_calls;
static uint32_t g_stop_calls;
static uint32_t g_submit_calls;
static uint16_t g_arm_epoch;
static motion_result_t g_arm_result;
static motion_result_t g_submit_result;
static int32_t g_publish_error;
static motion_command_t g_applied;
#if AAI05_FOLLOW_MQTT
#include "follow_remote.h"
#define TEST_START DRIVE_OP_FOLLOW
#define TEST_DUTY 0
static uint8_t g_route[118];
static uint16_t g_route_length;
static bool g_straight;
#else
#define TEST_START DRIVE_OP_START
#define TEST_DUTY 50
#endif

/* Colour reporting has no authority to arm or submit movement. */
void colour_service_link(uint32_t session) {(void)session;}
void colour_service_poll(void) {}

uint32_t drive_io_now(void) { return g_now; }
uint32_t drive_io_seed(void) { return ++g_seed; }
void drive_io_snapshot(drive_input_t *p_input, uint32_t now_ms)
{
    assert(now_ms == g_now);
    *p_input = g_input;
}
int32_t drive_io_publish(const char *p_topic, const uint8_t *p_data,
                          uint16_t length, bool is_reliable)
{
    assert(p_data[length - 1U] == comm_protocol_crc(p_data,
                                                   (uint16_t)(length - 1U)));
    if (strcmp(p_topic, DRIVE_TOPIC_ACK) == 0)
    {
        assert(is_reliable && length == DRIVE_ACK_SIZE);
        memcpy(g_ack, p_data, length);
    }
#if AAI05_FOLLOW_MQTT
    else if (strcmp(p_topic, DRIVE_TOPIC_ROUTE) == 0)
    {
        assert(!is_reliable && length <= sizeof(g_route));
        memcpy(g_route, p_data, length);
        g_route_length = length;
    }
#endif
    else
    {
        assert(strcmp(p_topic, DRIVE_TOPIC_TELEMETRY) == 0);
        assert(!is_reliable && length == DRIVE_TELEMETRY_SIZE);
        memcpy(g_telemetry, p_data, length);
    }
    return g_publish_error;
}
#if AAI05_FOLLOW_MQTT
motion_result_t subsystem2_follow_arm(uint16_t *p_epoch, bool straight)
{
    g_straight = straight;
#else
motion_result_t subsystem2_motion_arm(uint16_t *p_epoch)
{
#endif
    ++g_arm_calls;
    *p_epoch = ++g_arm_epoch;
    return g_arm_result;
}
motion_result_t subsystem2_motion_stop(void)
{
    ++g_stop_calls;
    return MOTION_ACCEPTED;
}
motion_result_t subsystem2_motion_submit(const motion_command_t *p_command)
{
    ++g_submit_calls;
    g_applied = *p_command;
    return g_submit_result;
}

static uint32_t test_word(const uint8_t *p_data)
{
    return ((uint32_t)p_data[0] << 24U) | ((uint32_t)p_data[1] << 16U) |
           ((uint32_t)p_data[2] << 8U) | p_data[3];
}
static uint8_t test_result(void)
{
    return (uint8_t)(((g_ack[0] & 1U) << 3U) | (g_ack[1] >> 5U));
}
static void test_send(uint8_t opcode, uint16_t sequence, uint32_t session,
                        uint32_t grant, int8_t left, bool is_retained)
{
    uint8_t frame[14] = {0};
    uint32_t packed = UINT32_C(2) << 20U | (uint32_t)opcode << 17U |
                      UINT32_C(7) << 14U |
                      ((uint32_t)(uint8_t)left & 127U) << 7U;
    frame[0] = (uint8_t)(packed >> 16U);
    frame[1] = (uint8_t)(packed >> 8U);
    frame[2] = (uint8_t)packed;
    frame[3] = (uint8_t)((uint32_t)sequence >> 8U);
    frame[4] = (uint8_t)sequence;
    for (uint32_t index = 0U; index < 4U; ++index)
    {
        frame[5U + index] = (uint8_t)(session >> (24U - index * 8U));
        frame[9U + index] = (uint8_t)(grant >> (24U - index * 8U));
    }
    frame[13] = comm_protocol_crc(frame, 13U);
    mqtt_drive_receive(frame, 14U, is_retained);
}

static void test_reset(void)
{
    g_now = 0U;
    g_input = (drive_input_t){.motion = {.status = {
        .phase = MOTION_STOPPED, .reason = MOTION_REASON_REQUEST},
        .is_available = true}, .is_ir_valid = true, .is_wifi_up = true};
#if AAI05_FOLLOW_MQTT
    g_input.can_follow = true;
    g_straight = false;
#endif
    g_arm_calls = 0U;
    g_stop_calls = 0U;
    g_submit_calls = 0U;
    g_arm_epoch = 0U;
    g_arm_result = MOTION_ACCEPTED;
    g_submit_result = MOTION_ACCEPTED;
    g_publish_error = 0;
    mqtt_app_link_changed(true);
    test_send(DRIVE_OP_STATUS, 0U, 0U, 0U, 0, false);
    assert(test_result() == DRIVE_OK);
    assert(g_stop_calls == 1U && g_arm_calls == 0U && g_submit_calls == 0U);
}

static void test_retained_lost_ack_and_owner_acknowledgement(void)
{
    test_reset();
    uint32_t session = test_word(&g_ack[7]);
    uint32_t grant = test_word(&g_ack[11]);
    test_send(TEST_START, 1U, session, grant, TEST_DUTY, true);
    assert(test_result() == DRIVE_RETAINED && g_arm_calls == 0U);
    g_publish_error = -1;
    test_send(TEST_START, 1U, session, grant, TEST_DUTY, false);
    assert(test_result() == DRIVE_OK && g_arm_calls == 1U);
    assert((g_ack[2] & 7U) == DRIVE_ARMING && g_submit_calls == 0U);
    g_now = 10U;
    g_publish_error = 0;
#if AAI05_FOLLOW_MQTT
    g_input.can_follow = false; /* An accepted FOLLOW retry still gets DUPLICATE. */
#endif
    test_send(TEST_START, 1U, session, grant, TEST_DUTY, false);
    assert(test_result() == DRIVE_DUPLICATE && g_arm_calls == 1U);
    assert((g_ack[1] & DRIVE_FLAG_ACTIVE) == 0U);
    g_input.motion.status.epoch = g_arm_epoch;
    g_input.motion.status.phase = MOTION_READY;
    g_now = 20U;
    mqtt_app_poll();
    assert(g_submit_calls == 1U && g_applied.left_pct == TEST_DUTY);
    assert(g_applied.right_pct == 0 && g_applied.issued_ms == 20U);
    g_input.motion.status.phase = MOTION_RUNNING;
    g_input.motion.status.left_pct = 50;
    g_input.motion.is_active = true;
    g_now = 40U;
    test_send(DRIVE_OP_STOP, 0U, session, 0U, 0, false);
    assert(test_result() == DRIVE_OK && (g_ack[2] & 7U) == DRIVE_STOPPED);
    assert((g_ack[1] & DRIVE_FLAG_ACTIVE) != 0U); /* STOP requested, not off yet. */
    g_input.motion.status.phase = MOTION_STOPPED;
    g_input.motion.status.left_pct = 0;
    g_input.motion.is_active = false;
    g_now = 60U;
    test_send(DRIVE_OP_STATUS, 0U, 0U, 0U, 0, false);
    assert((g_ack[1] & DRIVE_FLAG_ACTIVE) == 0U);
}

static void test_expiry_link_loss_and_no_automatic_restart(void)
{
    test_reset();
    uint32_t session = test_word(&g_ack[7]);
    uint32_t grant = test_word(&g_ack[11]);
    test_send(TEST_START, 1U, session, grant, TEST_DUTY, false);
    g_input.motion.status.epoch = g_arm_epoch;
    g_input.motion.status.phase = MOTION_READY;
    g_now = 20U;
    mqtt_app_poll();
    g_input.motion.status.phase = MOTION_RUNNING;
    g_input.motion.is_active = true;
    for (g_now = 40U; g_now < 760U; g_now += 20U)
    {
        mqtt_app_poll();
    }
    mqtt_app_poll(); /* First poll at/after the750ms deadline. */
    uint32_t submitted = g_submit_calls;
    assert(g_stop_calls == 2U);
    mqtt_app_poll();
    assert(g_submit_calls == submitted); /* No rearm/restart after expiry. */
    test_send(DRIVE_OP_STATUS, 0U, 0U, 0U, 0, false);
    assert((g_ack[2] & 7U) == DRIVE_STOPPED);
    mqtt_app_link_changed(false);
    test_send(TEST_START, 2U, session, grant, TEST_DUTY, false);
    assert(test_result() == DRIVE_NOT_READY);
    mqtt_app_link_changed(true);
    test_send(DRIVE_OP_STATUS, 0U, 0U, 0U, 0, false);
    assert(test_word(&g_ack[7]) != session);
    assert(g_arm_calls == 1U && g_submit_calls == submitted);
    session = test_word(&g_ack[7]);
    grant = test_word(&g_ack[11]);
    g_arm_result = MOTION_UNAVAILABLE;
    test_send(TEST_START, 1U, session, grant, TEST_DUTY, false);
    assert(test_result() == DRIVE_MOTION_FAILURE);
    assert((g_ack[2] & 7U) == DRIVE_STOPPED);
}

static void test_stale_view_cannot_override_motion_stop(void)
{
    test_reset();
    test_send(TEST_START, 1U, test_word(&g_ack[7]), test_word(&g_ack[11]),
                 TEST_DUTY, false);
    g_input.motion.status.epoch = g_arm_epoch;
    g_input.motion.status.phase = MOTION_READY;
    g_now = 20U;
    mqtt_app_poll();
    g_input.motion.status.phase = MOTION_RUNNING;
    g_input.motion.is_active = true;
    g_submit_result = MOTION_LATCHED; /* GP21 can win after snapshot read. */
    g_now = 40U;
    mqtt_app_poll();
    uint32_t submitted = g_submit_calls;
    assert(g_stop_calls == 2U);
    g_submit_result = MOTION_ACCEPTED;
    g_now = 60U;
    mqtt_app_poll();
    assert(g_submit_calls == submitted && g_arm_calls == 1U);
    test_send(DRIVE_OP_STATUS, 0U, 0U, 0U, 0, false);
    assert((g_ack[2] & 7U) == DRIVE_STOPPED);
}

#if AAI05_FOLLOW_MQTT
static void test_follow_only_and_route_reporting(void)
{
    test_reset();
    uint32_t session = test_word(&g_ack[7]);
    uint32_t grant = test_word(&g_ack[11]);
    test_send(DRIVE_OP_START, 1U, session, grant, 25, false);
    assert(test_result() == DRIVE_BAD_VALUE && g_arm_calls == 0U);
    test_send(DRIVE_OP_FOLLOW, 1U, session, grant, 1, false);
    assert(test_result() == DRIVE_BAD_VALUE && g_arm_calls == 0U);
    g_input.can_follow = false;
    test_send(DRIVE_OP_FOLLOW, 1U, session, grant, 0, false);
    assert(test_result() == DRIVE_NOT_READY && g_arm_calls == 0U);

    g_input.follow_phase = 2U;
    g_input.follow_reason = 12U;
    g_input.route_epoch = 5U;
    g_input.route = (follow_route_report_t){
        .status = ROUTE_FINISHED, .colour = ROUTE_BLUE, .finished = true,
        .count = 1U, .path_um = 340000U, .unassigned_um = 20000U,
        .sections = {{.colour = ROUTE_BLUE, .flags = ROUTE_PARTIAL_END, .length_um = 320000U}}
    };
    mqtt_app_poll();
    assert(g_route_length == 28U && g_route[0] == 0xd0U);
    assert(g_route[1] == 2U && g_route[2] == 12U && g_route[3] == ROUTE_BLUE);
    assert(g_route[4] == ROUTE_FINISHED && g_route[5] == 1U);
    assert(test_word(&g_route[6]) == session && g_route[10] == 0U && g_route[11] == 5U);
    assert(test_word(&g_route[12]) == 340000U && test_word(&g_route[16]) == 20000U);
    assert(g_route[20] == 1U && g_route[21] == ROUTE_BLUE && g_route[22] == ROUTE_PARTIAL_END);
    assert(test_word(&g_route[23]) == 320000U);
}

static void test_straight_authorizes_only_the_straight_policy(void)
{
    test_reset();
    uint32_t session = test_word(&g_ack[7]);
    uint32_t grant = test_word(&g_ack[11]);
    test_send(DRIVE_OP_STRAIGHT, 1U, session, grant, 1, false);
    assert(test_result() == DRIVE_BAD_VALUE && g_arm_calls == 0U);
    test_send(DRIVE_OP_STRAIGHT, 1U, session, grant, 0, true);
    assert(test_result() == DRIVE_RETAINED && g_arm_calls == 0U);
    test_send(DRIVE_OP_STRAIGHT, 1U, session, grant, 0, false);
    assert(test_result() == DRIVE_OK && g_arm_calls == 1U && g_straight);
    assert(((g_ack[0] >> 1U) & 7U) == DRIVE_OP_STRAIGHT);
    test_send(DRIVE_OP_FOLLOW, 2U, session, test_word(&g_ack[11]), 0, false);
    assert(test_result() == DRIVE_BAD_STATE && g_arm_calls == 1U && g_straight);
}
#endif

int main(void)
{
    test_retained_lost_ack_and_owner_acknowledgement();
    test_expiry_link_loss_and_no_automatic_restart();
    test_stale_view_cannot_override_motion_stop();
#if AAI05_FOLLOW_MQTT
    test_follow_only_and_route_reporting();
    test_straight_authorizes_only_the_straight_policy();
#endif
    puts("PASS: actual MQTT service hooks, owner ACK, retained/duplicate, "
         "expiry, failed ARM and reconnect without restart");
    return 0;
}
