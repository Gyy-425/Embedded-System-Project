#ifndef AAI05_DRIVE_PROTOCOL_H
#define AAI05_DRIVE_PROTOCOL_H
#include <stdbool.h>
#include <stdint.h>

#define DRIVE_COMMAND_SIZE 14U
#define DRIVE_ACK_SIZE 23U
#define DRIVE_TELEMETRY_SIZE 26U
#define DRIVE_UNKNOWN 7U
#define DRIVE_LEASE_MS 750U
#define DRIVE_GRANT_MS 1000U
#define DRIVE_FEED_MS 20U
#define DRIVE_TELEMETRY_MS 250U

#define DRIVE_OP_STATUS 0U
#define DRIVE_OP_TARGET 1U
#define DRIVE_OP_CLEAR 2U
#define DRIVE_OP_START 3U
#define DRIVE_OP_KEEPALIVE 4U
#define DRIVE_OP_STOP 5U
#define DRIVE_OP_FOLLOW 6U
#define DRIVE_OP_STRAIGHT 7U
#if AAI05_FOLLOW_MQTT
#define DRIVE_OP_MAX DRIVE_OP_STRAIGHT
#else
#define DRIVE_OP_MAX DRIVE_OP_STOP
#endif

#define DRIVE_OK 0U
#define DRIVE_DUPLICATE 1U
#define DRIVE_BAD_FRAME 2U
#define DRIVE_BAD_VALUE 3U
#define DRIVE_SESSION 4U
#define DRIVE_STALE_SEQUENCE 5U
#define DRIVE_CONFLICT 6U
#define DRIVE_NOT_READY 7U
#define DRIVE_STALE_GRANT 8U
#define DRIVE_BAD_STATE 9U
#define DRIVE_RETAINED 10U
#define DRIVE_MOTION_FAILURE 11U
#define DRIVE_BUSY 12U
typedef uint8_t drive_result_t;

#define DRIVE_IDLE 0U
#define DRIVE_ARMING 1U
#define DRIVE_RUNNING 2U
#define DRIVE_STOPPED 3U
#define DRIVE_FAULT 4U

#define DRIVE_FLAG_READY 0x01U
#define DRIVE_FLAG_ACTIVE 0x02U
#define DRIVE_FLAG_IR_VALID 0x04U
#define DRIVE_FLAG_IR_STALE 0x08U
#define DRIVE_FLAG_TX_DROP 0x10U

typedef struct
{
    uint32_t session;
    uint32_t grant;
    uint16_t sequence;
    uint8_t opcode;
    uint8_t target;
    int8_t left_pct;
    int8_t right_pct;
} drive_command_t;

typedef struct
{
    uint32_t session;
    uint32_t grant;
    uint32_t uptime_ms;
    uint16_t echo;
    uint16_t mutation_sequence;
    uint16_t lease_ms;
    uint16_t left_raw;
    uint16_t right_raw;
    int8_t left_pct;
    int8_t right_pct;
    uint8_t opcode;
    uint8_t result;
    uint8_t target;
    uint8_t flags;
    uint8_t phase;
    uint8_t motion_phase;
    uint8_t motion_reason;
} drive_status_t;

drive_result_t drive_protocol_decode(const uint8_t *p_data, uint16_t length,
                                      drive_command_t *p_command);
uint16_t drive_protocol_encode(uint8_t *p_output,
                                const drive_status_t *p_status,
                                bool is_telemetry);
#endif
