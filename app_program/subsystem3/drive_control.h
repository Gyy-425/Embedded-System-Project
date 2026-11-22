#ifndef AAI05_DRIVE_CONTROL_H
#define AAI05_DRIVE_CONTROL_H
#include "drive_protocol.h"
#include "../subsystem2/motion_control.h"

#define DRIVE_ACTION_NONE 0U
#define DRIVE_ACTION_ARM 1U
#define DRIVE_ACTION_STOP 2U
#define DRIVE_ACTION_SUBMIT 3U

typedef struct
{
    motion_status_t status;
    bool is_available;
    bool is_active;
} drive_motion_view_t;

typedef struct
{
    motion_command_t command;
    uint8_t kind;
#if AAI05_FOLLOW_MQTT
    bool straight;
#endif
} drive_action_t;

typedef struct
{
    drive_command_t previous;
    uint32_t session;
    uint32_t grant;
    uint32_t grant_ms;
    uint32_t renewed_ms;
    uint32_t fed_ms;
    uint16_t expected_epoch;
    uint16_t motion_sequence;
    int8_t left_pct;
    int8_t right_pct;
    uint8_t target;
    uint8_t phase;
    bool is_ready;
    bool has_sequence;
    bool is_arm_queued;
} drive_control_t;

void drive_control_reset(drive_control_t *p_control, uint32_t session,
                          uint32_t grant, uint32_t now_ms, bool is_ready);
drive_result_t drive_control_receive(drive_control_t *p_control,
                                      const drive_command_t *p_command,
                                      bool is_retained, uint32_t now_ms,
                                      drive_action_t *p_action);
drive_result_t drive_control_arm_result(drive_control_t *p_control,
                                        motion_result_t result,
                                        uint16_t epoch, uint32_t now_ms);
void drive_control_poll(drive_control_t *p_control, uint32_t now_ms,
                         bool is_wifi_up, const drive_motion_view_t *p_motion,
                         drive_action_t *p_action);
void drive_control_halt(drive_control_t *p_control, uint32_t now_ms,
                         bool is_fault);
uint16_t drive_control_lease(const drive_control_t *p_control, uint32_t now_ms);
#endif
