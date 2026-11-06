/* All state and callbacks below belong to the one radio/lwIP service task. */
/* The supplied kernel uses unprototyped generic FP/FUNCP typedefs. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include "../app.h"
#pragma GCC diagnostic pop

#if AAI05_COMMS_APP

#include "comm_protocol.h"
#include "comm_config.h"
#include "mqtt_utk.h"

static comm_state_t g_mqtt_app_state;
static uint32_t g_mqtt_app_generation;
static uint32_t g_mqtt_app_previous_session;
static uint32_t g_mqtt_app_last_publish_ms;
static uint16_t g_mqtt_app_telemetry_sequence;

const char *mqtt_app_command_topic(void)
{
    return COMM_TOPIC_COMMAND;
}

void mqtt_app_link_changed(bool is_ready)
{
    uint32_t session = 0U;

    if (is_ready)
    {
        /* Non-secret connection tag, not a security/authentication token. */
        ++g_mqtt_app_generation;
        session = app_now_ms() ^ (uint32_t)in_w(TIMER_TIMERAWL) ^
                  (g_mqtt_app_generation * UINT32_C(0x9E3779B9));
        if ((0U == session) || (session == g_mqtt_app_previous_session))
        {
            session = g_mqtt_app_previous_session + 1U;
            if (0U == session)
            {
                session = 1U;
            }
        }
        g_mqtt_app_previous_session = session;
    }
    comm_protocol_reset(&g_mqtt_app_state, session, is_ready);
    g_mqtt_app_telemetry_sequence = 0U;
    g_mqtt_app_last_publish_ms = app_now_ms() - COMM_TELEMETRY_PERIOD_MS;
    tm_printf((UB *)"comm: %s; target unset; motor output disabled\n",
              is_ready ? "command subscription ready" : "link unavailable");
}

void mqtt_app_receive(const uint8_t *p_data, uint16_t length)
{
    uint8_t acknowledgement[COMM_ACK_SIZE];
    comm_command_t command = {0};
    comm_result_t decoded;
    comm_result_t result;
    uint8_t opcode = COMM_OP_STATUS;
    int32_t sent;

    decoded = comm_protocol_decode(p_data, length, &command);
    if (COMM_RESULT_OK == decoded)
    {
        opcode = (uint8_t)(((uint32_t)command.control >> 4U) & 0x03U);
    }
    result = comm_protocol_apply(&g_mqtt_app_state, p_data, length);
    comm_protocol_encode_ack(acknowledgement, &g_mqtt_app_state,
                              opcode, result);
    sent = mqtt_utk_publish_binary(COMM_TOPIC_ACK, acknowledgement,
                                   COMM_ACK_SIZE, true);
    comm_protocol_tx_drop(&g_mqtt_app_state, 0 != sent);
    tm_printf((UB *)"comm: opcode=%u result=%u target=%u; selection only\n",
              (UINT)opcode, (UINT)result,
              (UINT)comm_protocol_target(&g_mqtt_app_state));
    g_mqtt_app_last_publish_ms = app_now_ms() - COMM_TELEMETRY_PERIOD_MS;
}

static void mqtt_app_fill_sample(comm_sample_t *p_sample, uint32_t now_ms)
{
    ir_snapshot_t sensor = {0};
    ER error = subsystem1_get_snapshot(&sensor);
    bool is_available = (E_OK == error) && (0U != sensor.sequence);
    bool is_stale = !is_available ||
                    ((uint32_t)(now_ms - sensor.sampled_ms) >
                     COMM_IR_MAX_AGE_MS);
    bool is_valid = is_available && sensor.valid && (E_OK == sensor.error) &&
                    !is_stale;
    uint32_t flags = COMM_CAP_TARGET | COMM_CAP_RAW_IR | COMM_FLAG_READY;

    if (is_valid)
    {
        flags |= COMM_FLAG_IR_VALID;
    }
    if (is_stale)
    {
        flags |= COMM_FLAG_IR_STALE;
    }
    if (comm_protocol_has_tx_drop(&g_mqtt_app_state))
    {
        flags |= COMM_FLAG_TX_DROP;
    }
    p_sample->flags = (uint8_t)flags;
    p_sample->selection = (uint8_t)(
        ((uint32_t)comm_protocol_target(&g_mqtt_app_state) << 3U) |
        COMM_COLOUR_UNKNOWN);
    p_sample->sequence = g_mqtt_app_telemetry_sequence;
    p_sample->session = g_mqtt_app_state.session;
    p_sample->uptime_ms = now_ms;
    p_sample->left_raw = is_available ? sensor.left_raw : 0U;
    p_sample->right_raw = is_available ? sensor.right_raw : 0U;
}

void mqtt_app_poll(void)
{
    uint32_t now_ms = app_now_ms();
    uint8_t payload[COMM_TELEMETRY_SIZE];
    comm_sample_t sample = {0};
    int32_t sent;

    if (!comm_protocol_is_ready(&g_mqtt_app_state) ||
        ((uint32_t)(now_ms - g_mqtt_app_last_publish_ms) <
         COMM_TELEMETRY_PERIOD_MS))
    {
        return;
    }
    g_mqtt_app_last_publish_ms = now_ms;
    mqtt_app_fill_sample(&sample, now_ms);
    if (!comm_protocol_encode_telemetry(payload, &sample))
    {
        comm_protocol_tx_drop(&g_mqtt_app_state, true);
        return;
    }
    sent = mqtt_utk_publish_binary(COMM_TOPIC_TELEMETRY, payload,
                                   COMM_TELEMETRY_SIZE, false);
    comm_protocol_tx_drop(&g_mqtt_app_state, 0 != sent);
    if (0 == sent)
    {
        ++g_mqtt_app_telemetry_sequence;
    }
}

#endif /* AAI05_COMMS_APP */
