#ifndef AAI05_DRIVE_SERVICE_H
#define AAI05_DRIVE_SERVICE_H
#include "drive_control.h"
#include "../../config/comm_config.h"
#ifndef AAI05_REMOTE_APP
#define AAI05_REMOTE_APP 0
#endif

#define DRIVE_TOPIC_COMMAND "aai05/v2/" COMM_DEVICE_ID "/command"
#define DRIVE_TOPIC_ACK "aai05/v2/" COMM_DEVICE_ID "/ack"
#define DRIVE_TOPIC_TELEMETRY "aai05/v2/" COMM_DEVICE_ID "/telemetry"
#if AAI05_FOLLOW_MQTT
#include "../subsystem2/follow_route.h"
#define DRIVE_TOPIC_ROUTE "aai05/v2/" COMM_DEVICE_ID "/route"
#endif

typedef struct
{
    drive_motion_view_t motion;
    uint16_t left_raw;
    uint16_t right_raw;
    bool is_ir_valid;
    bool is_ir_stale;
    bool is_wifi_up;
#if AAI05_FOLLOW_MQTT
    follow_route_report_t route;
    uint16_t route_epoch;
    uint8_t follow_phase;
    uint8_t follow_reason;
    bool can_follow;
#endif
} drive_input_t;

/* Platform boundary, called only by the existing network-owner task. */
uint32_t drive_io_now(void);
uint32_t drive_io_seed(void);
void drive_io_snapshot(drive_input_t *p_input, uint32_t now_ms);
int32_t drive_io_publish(const char *p_topic, const uint8_t *p_data,
                          uint16_t length, bool is_reliable);

/* Network-owner hooks selected by the opt-in REMOTE profile. */
const char *mqtt_app_command_topic(void);
void mqtt_app_link_changed(bool is_ready);
void mqtt_drive_receive(const uint8_t *p_data, uint16_t length,
                         bool is_retained);
void mqtt_app_poll(void);
#endif
