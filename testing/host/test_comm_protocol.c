/* Prepared boundary checks; execution is a separate validation step. */
#include "comm_protocol.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void make_command(uint8_t *p_frame, uint8_t opcode, uint8_t target,
                         uint16_t sequence, uint32_t session)
{
    p_frame[0] = (uint8_t)(((uint32_t)opcode << 4U) |
                          ((uint32_t)target << 1U));
    p_frame[1] = (uint8_t)((uint32_t)sequence >> 8U);
    p_frame[2] = (uint8_t)sequence;
    p_frame[3] = (uint8_t)(session >> 24U);
    p_frame[4] = (uint8_t)(session >> 16U);
    p_frame[5] = (uint8_t)(session >> 8U);
    p_frame[6] = (uint8_t)session;
    p_frame[7] = comm_protocol_crc(p_frame, 7U);
}

static void check_rejection(void)
{
    uint8_t frame[COMM_COMMAND_SIZE];
    comm_state_t state = {0};
    comm_command_t command = {0};

    assert(0xF4U == comm_protocol_crc((const uint8_t *)"123456789", 9U));
    assert(COMM_RESULT_BAD_FRAME == comm_protocol_decode(NULL, 8U,
                                                        &command));
    make_command(frame, COMM_OP_TARGET, 2U, 1U, 0x01020304U);
    assert(COMM_RESULT_NOT_READY == comm_protocol_apply(&state, frame, 8U));
    comm_protocol_reset(&state, 0x01020304U, true);
    assert(COMM_RESULT_BAD_FRAME == comm_protocol_apply(&state, frame, 7U));
    frame[7] ^= 1U;
    assert(COMM_RESULT_BAD_FRAME == comm_protocol_apply(&state, frame, 8U));
    make_command(frame, COMM_OP_TARGET, 7U, 1U, state.session);
    assert(COMM_RESULT_BAD_TARGET == comm_protocol_apply(&state, frame, 8U));
    make_command(frame, COMM_OP_CLEAR, 0U, 1U, state.session);
    assert(COMM_RESULT_BAD_TARGET == comm_protocol_apply(&state, frame, 8U));
    make_command(frame, 3U, 7U, 1U, state.session);
    assert(COMM_RESULT_BAD_FRAME == comm_protocol_apply(&state, frame, 8U));
    make_command(frame, COMM_OP_TARGET, 2U, 1U, state.session + 1U);
    assert(COMM_RESULT_SESSION == comm_protocol_apply(&state, frame, 8U));
    assert(7U == comm_protocol_target(&state));
    assert(0U == state.sequence);
}

static void check_sequence(void)
{
    uint8_t frame[COMM_COMMAND_SIZE];
    comm_state_t state = {0};

    comm_protocol_reset(&state, 0x01020304U, true);
    make_command(frame, COMM_OP_TARGET, 6U, 0xFFFEU, state.session);
    assert(COMM_RESULT_OK == comm_protocol_apply(&state, frame, 8U));
    assert(COMM_RESULT_DUPLICATE == comm_protocol_apply(&state, frame, 8U));
    make_command(frame, COMM_OP_TARGET, 5U, 0xFFFEU, state.session);
    assert(COMM_RESULT_CONFLICT == comm_protocol_apply(&state, frame, 8U));
    assert(6U == comm_protocol_target(&state));
    make_command(frame, COMM_OP_CLEAR, 7U, 0xFFFFU, state.session);
    assert(COMM_RESULT_OK == comm_protocol_apply(&state, frame, 8U));
    make_command(frame, COMM_OP_TARGET, 0U, 0U, state.session);
    assert(COMM_RESULT_OK == comm_protocol_apply(&state, frame, 8U));
    make_command(frame, COMM_OP_TARGET, 2U, 0xFFFFU, state.session);
    assert(COMM_RESULT_STALE == comm_protocol_apply(&state, frame, 8U));
    make_command(frame, COMM_OP_TARGET, 2U, 0x8000U, state.session);
    assert(COMM_RESULT_STALE == comm_protocol_apply(&state, frame, 8U));
    make_command(frame, COMM_OP_STATUS, 7U, 123U, 0U);
    assert(COMM_RESULT_OK == comm_protocol_apply(&state, frame, 8U));
    assert(0U == state.sequence);
    assert(0U == comm_protocol_target(&state));
    comm_protocol_reset(&state, 0U, false);
    assert(!comm_protocol_is_ready(&state));
    assert(7U == comm_protocol_target(&state));
}

static void print_vector(const char *p_name, const uint8_t *p_frame,
                         uint16_t length)
{
    printf("%s ", p_name);
    for (uint16_t index = 0U; index < length; ++index)
    {
        printf("%02x", (unsigned int)p_frame[index]);
    }
    printf("\n");
}

static void check_encoding(void)
{
    uint8_t frame[COMM_TELEMETRY_SIZE];
    comm_state_t state = {0};
    comm_sample_t sample = {0};
    const uint8_t ack_body[8] = {0x4EU, 0x98U, 0U, 0U, 1U, 2U, 3U, 4U};

    comm_protocol_reset(&state, 0x01020304U, true);
    comm_protocol_encode_ack(frame, &state, COMM_OP_STATUS, COMM_RESULT_OK);
    assert(0 == memcmp(frame, ack_body, sizeof(ack_body)));
    assert(frame[8] == comm_protocol_crc(frame, 8U));
    print_vector("ACK", frame, COMM_ACK_SIZE);
    sample.session = state.session;
    sample.uptime_ms = 0xFFFFFFFFU;
    sample.sequence = 0xFFFFU;
    sample.selection = 0x17U; /* target2, detectedUNKNOWN7 */
    sample.flags = 0x93U; /* target/rawIR, valid, ready */
    sample.left_raw = 0xABCU;
    sample.right_raw = 0x123U;
    assert(comm_protocol_encode_telemetry(frame, &sample));
    assert(0x97U == frame[0]);
    assert(0xABU == frame[12]);
    assert(0xC1U == frame[13]);
    assert(0x23U == frame[14]);
    assert(frame[15] == comm_protocol_crc(frame, 15U));
    print_vector("TELEMETRY", frame, COMM_TELEMETRY_SIZE);
    sample.left_raw = 4096U;
    assert(!comm_protocol_encode_telemetry(frame, &sample));
    comm_protocol_tx_drop(&state, true);
    assert(comm_protocol_has_tx_drop(&state));
    comm_protocol_tx_drop(&state, false);
    assert(!comm_protocol_has_tx_drop(&state));
}

int main(void)
{
    check_rejection();
    check_sequence();
    check_encoding();
    puts("Communication protocol checks passed.");
    return 0;
}
