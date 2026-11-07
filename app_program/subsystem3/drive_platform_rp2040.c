#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include "../app.h"
#pragma GCC diagnostic pop
#include "drive_service.h"

#if AAI05_REMOTE_APP
#include "cyw43_utk.h"
#include "mqtt_utk.h"

static motion_snapshot_t g_drive_platform_motion;
static ir_snapshot_t g_drive_platform_ir;
static bool g_drive_platform_has_motion;
static bool g_drive_platform_has_ir;

uint32_t drive_io_now(void)
{
    return app_now_ms();
}

void drive_io_snapshot(drive_input_t *p_input, uint32_t now_ms)
{
    motion_snapshot_t motion;
    ir_snapshot_t sensor;
    T_CYW43_UTK_STATUS radio;
    *p_input = (drive_input_t){0};
    if ((subsystem2_get_snapshot(&motion) == E_OK) && (motion.sequence != 0U))
    {
        g_drive_platform_motion = motion;
        g_drive_platform_has_motion = true;
    }
    if ((subsystem1_get_snapshot(&sensor) == E_OK) && (sensor.sequence != 0U))
    {
        g_drive_platform_ir = sensor;
        g_drive_platform_has_ir = true;
    }
    /* Reuse a briefly mutex-busy snapshot, but never beyond freshness. */
    now_ms = app_now_ms(); /* Snapshots may have advanced after entry. */
    const motion_snapshot_t *p_motion = &g_drive_platform_motion;
    const ir_snapshot_t *p_ir = &g_drive_platform_ir;
    p_input->motion.status = p_motion->motion;
    p_input->motion.is_active = p_motion->motor.active;
    p_input->motion.is_available = g_drive_platform_has_motion &&
        ((uint32_t)(now_ms - p_motion->sampled_ms) <= 100U) &&
        p_motion->capture_ready && p_motion->motor.ready &&
        !p_motion->count_saturated &&
        ((p_motion->motor.error == E_OK) || (p_motion->motor.error == E_TMOUT));
    p_input->is_ir_stale = !g_drive_platform_has_ir ||
        ((uint32_t)(now_ms - p_ir->sampled_ms) > 100U);
    p_input->is_ir_valid = !p_input->is_ir_stale && p_ir->valid &&
                           (p_ir->error == E_OK);
    p_input->left_raw = g_drive_platform_has_ir ? p_ir->left_raw : 0U;
    p_input->right_raw = g_drive_platform_has_ir ? p_ir->right_raw : 0U;
    cyw43_utk_get_status(&radio);
    p_input->is_wifi_up = radio.ready &&
                          (radio.link_status == CYW43_UTK_LINK_UP);
}

int32_t drive_io_publish(const char *p_topic, const uint8_t *p_data,
                          uint16_t length, bool is_reliable)
{
    return mqtt_utk_publish_binary(p_topic, p_data, length, is_reliable);
}
#endif
