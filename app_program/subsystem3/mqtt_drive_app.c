#include "drive_service.h"
#include "../subsystem2/motion_runtime.h"
#include "colour_service.h"
#if AAI05_FOLLOW_MQTT
#include "comm_protocol.h"
#include "../subsystem2/follow_remote.h"
#endif

#if AAI05_REMOTE_APP
static drive_control_t g_drive_service_control;
static uint32_t g_drive_service_generation;
static uint32_t g_drive_service_previous_session;
static uint32_t g_drive_service_published_ms;
static uint16_t g_drive_service_telemetry_sequence;
static bool g_drive_service_tx_drop;

const char *mqtt_app_command_topic(void)
{
    return DRIVE_TOPIC_COMMAND;
}

void mqtt_app_link_changed(bool is_ready)
{
    uint32_t session = 0U;
    uint32_t grant = 0U;
    uint32_t now_ms = drive_io_now();

    (void)subsystem2_motion_stop();
    if (is_ready)
    {
        ++g_drive_service_generation;
        session = drive_io_seed() ^
                  (g_drive_service_generation * UINT32_C(0x9E3779B9));
        if ((session == 0U) || (session == g_drive_service_previous_session))
        {
            session = g_drive_service_previous_session + 1U;
            if (session == 0U)
            {
                session = 1U;
            }
        }
        g_drive_service_previous_session = session;
        grant = drive_io_seed();
        if (grant == 0U)
        {
            grant = 1U;
        }
    }
    drive_control_reset(&g_drive_service_control, session, grant, now_ms,
                          is_ready);
#if !AAI05_FOLLOW_MQTT
    colour_service_link(session);
#endif
    g_drive_service_telemetry_sequence = 0U;
    g_drive_service_published_ms = now_ms - DRIVE_TELEMETRY_MS;
    g_drive_service_tx_drop = false;
}

static drive_result_t drive_service_apply(const drive_action_t *p_action,
                                           uint32_t now_ms)
{
    if (p_action->kind == DRIVE_ACTION_ARM)
    {
        uint16_t epoch = 0U;
#if AAI05_FOLLOW_MQTT
        motion_result_t result = subsystem2_follow_arm(&epoch, p_action->straight);
#else
        motion_result_t result = subsystem2_motion_arm(&epoch);
#endif
        drive_result_t remote_result = drive_control_arm_result(
            &g_drive_service_control, result, epoch, now_ms);
        if (remote_result != DRIVE_OK)
        {
            (void)subsystem2_motion_stop();
        }
        return remote_result;
    }
    if (p_action->kind == DRIVE_ACTION_STOP)
    {
        (void)subsystem2_motion_stop();
    }
    else if (p_action->kind == DRIVE_ACTION_SUBMIT)
    {
        if (subsystem2_motion_submit(&p_action->command) != MOTION_ACCEPTED)
        {
            drive_control_halt(&g_drive_service_control, now_ms, false);
            (void)subsystem2_motion_stop();
            return DRIVE_MOTION_FAILURE;
        }
    }
    return DRIVE_OK;
}

static void drive_service_fill(drive_status_t *p_status,
                                const drive_input_t *p_input,
                                uint32_t now_ms)
{
    const motion_status_t *p_motion = &p_input->motion.status;

    p_status->session = g_drive_service_control.session;
    p_status->grant = g_drive_service_control.grant;
    p_status->uptime_ms = now_ms;
    p_status->mutation_sequence = g_drive_service_control.has_sequence ?
        g_drive_service_control.previous.sequence : 0U;
    p_status->target = g_drive_service_control.target;
    p_status->phase = g_drive_service_control.phase;
    p_status->lease_ms = drive_control_lease(&g_drive_service_control, now_ms);
    p_status->motion_phase = p_motion->phase;
    p_status->motion_reason = p_motion->reason;
    p_status->left_pct = p_motion->left_pct;
    p_status->right_pct = p_motion->right_pct;
    p_status->left_raw = p_input->left_raw;
    p_status->right_raw = p_input->right_raw;
    p_status->flags = (uint8_t)(
        (g_drive_service_control.is_ready && p_input->is_wifi_up &&
         p_input->motion.is_available ? DRIVE_FLAG_READY : 0U) |
        (p_input->motion.is_active ? DRIVE_FLAG_ACTIVE : 0U) |
        (p_input->is_ir_valid ? DRIVE_FLAG_IR_VALID : 0U) |
        (p_input->is_ir_stale ? DRIVE_FLAG_IR_STALE : 0U) |
        (g_drive_service_tx_drop ? DRIVE_FLAG_TX_DROP : 0U));
}

static void drive_service_step(uint32_t now_ms, const drive_input_t *p_input)
{
    drive_action_t action;
    drive_control_poll(&g_drive_service_control, now_ms, p_input->is_wifi_up,
                         &p_input->motion, &action);
    (void)drive_service_apply(&action, now_ms);
}

#if AAI05_FOLLOW_MQTT
static void route_write_word(uint8_t *p_output, uint32_t value)
{
    p_output[0] = (uint8_t)(value >> 24U);
    p_output[1] = (uint8_t)(value >> 16U);
    p_output[2] = (uint8_t)(value >> 8U);
    p_output[3] = (uint8_t)value;
}

static void drive_publish_route(const drive_input_t *p_input)
{
    const follow_route_report_t *p_route = &p_input->route;
    uint8_t payload[22U + 6U * ROUTE_QUEUE_SIZE];
    if (p_route->count > ROUTE_QUEUE_SIZE)
    {
        return;
    }
    payload[0] = 0xD0U;
    payload[1] = p_input->follow_phase;
    payload[2] = p_input->follow_reason;
    payload[3] = p_route->colour;
    payload[4] = p_route->status;
    payload[5] = p_route->finished ? 1U : 0U;
    route_write_word(&payload[6], g_drive_service_control.session);
    payload[10] = (uint8_t)(p_input->route_epoch >> 8U);
    payload[11] = (uint8_t)p_input->route_epoch;
    route_write_word(&payload[12], p_route->path_um);
    route_write_word(&payload[16], p_route->unassigned_um);
    payload[20] = p_route->count;
    uint16_t offset = 21U;
    for (uint8_t i = 0U; i < p_route->count; ++i)
    {
        payload[offset] = p_route->sections[i].colour;
        payload[offset + 1U] = p_route->sections[i].flags;
        route_write_word(&payload[offset + 2U], p_route->sections[i].length_um);
        offset += 6U;
    }
    payload[offset] = comm_protocol_crc(payload, offset);
    (void)drive_io_publish(DRIVE_TOPIC_ROUTE, payload, (uint16_t)(offset + 1U), false);
}
#endif

void mqtt_drive_receive(const uint8_t *p_data, uint16_t length,
                         bool is_retained)
{
    drive_command_t command = {0};
    drive_status_t status = {0};
    drive_action_t action = {0};
    drive_input_t input;
    uint8_t acknowledgement[DRIVE_ACK_SIZE];
    uint32_t now_ms = drive_io_now();
    drive_result_t result = drive_protocol_decode(p_data, length, &command);

    drive_io_snapshot(&input, now_ms);
    if (result == DRIVE_OK)
    {
#if AAI05_FOLLOW_MQTT
        if (((command.opcode == DRIVE_OP_FOLLOW) || (command.opcode == DRIVE_OP_STRAIGHT)) &&
            ((g_drive_service_control.phase == DRIVE_IDLE) ||
             (g_drive_service_control.phase == DRIVE_STOPPED)) &&
            (!input.is_wifi_up || !input.can_follow))
#else
        if ((command.opcode == DRIVE_OP_START) &&
            (!input.is_wifi_up || !input.motion.is_available))
#endif
        {
            result = DRIVE_NOT_READY;
        }
        else
        {
            result = drive_control_receive(&g_drive_service_control,
                &command, is_retained, now_ms, &action);
            drive_result_t applied = drive_service_apply(&action, now_ms);
            if ((result == DRIVE_OK) && (applied != DRIVE_OK))
            {
                result = applied;
            }
        }
    }
    drive_service_step(now_ms, &input);
    drive_service_fill(&status, &input, now_ms);
    status.opcode = command.opcode;
    status.echo = command.sequence;
    status.result = result;
    uint16_t encoded = drive_protocol_encode(acknowledgement, &status, false);
    g_drive_service_tx_drop = (encoded == 0U) ||
        (drive_io_publish(DRIVE_TOPIC_ACK, acknowledgement, encoded, true) != 0);
}

void mqtt_app_poll(void)
{
    drive_input_t input;
    drive_status_t status = {0};
    uint8_t telemetry[DRIVE_TELEMETRY_SIZE];
    uint32_t now_ms = drive_io_now();

    drive_io_snapshot(&input, now_ms);
    drive_service_step(now_ms, &input);
    /* Optional reporting follows motion STOP/lease work on the same owner. */
#if !AAI05_FOLLOW_MQTT
    colour_service_poll();
#endif
    if (!g_drive_service_control.is_ready ||
        ((uint32_t)(now_ms - g_drive_service_published_ms) <
         DRIVE_TELEMETRY_MS))
    {
        return;
    }
    g_drive_service_published_ms = now_ms;
    drive_service_fill(&status, &input, now_ms);
    status.echo = g_drive_service_telemetry_sequence;
    uint16_t encoded = drive_protocol_encode(telemetry, &status, true);
    g_drive_service_tx_drop = (encoded == 0U) ||
        (drive_io_publish(DRIVE_TOPIC_TELEMETRY, telemetry, encoded, false) != 0);
    if (!g_drive_service_tx_drop)
    {
        ++g_drive_service_telemetry_sequence;
#if AAI05_FOLLOW_MQTT
        drive_publish_route(&input);
#endif
    }
}
#endif
