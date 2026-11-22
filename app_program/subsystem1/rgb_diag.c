#include "rgb_diag.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include "../app.h"
#pragma GCC diagnostic pop
#if AAI05_RGB_DIAG || AAI05_FOLLOW_APP
#include "rgb_i2c_rp2040.h"
#include "rgb_classifier.h"
#if AAI05_FOLLOW_APP
#define RGB_DRIVE_TEXT "OWNER"
#else
#define RGB_DRIVE_TEXT "OFF"
#endif

static ID g_rgb_mutex;
static rgb_snapshot_t g_latest;
static bool g_available;

static void rgb_task(INT start_code, void *p_extra)
{
    rgb_sensor_t sensor;
    (void)start_code;
    (void)p_extra;
    rgb_sensor_init(&sensor);
    for (;;)
    {
        rgb_snapshot_t next;
        rgb_sensor_step(&sensor, rgb_rp2040_bus());
        (void)rgb_sensor_snapshot(&sensor, app_now_ms(), &next);
        if (tk_loc_mtx(g_rgb_mutex, TMO_FEVR) == E_OK)
        {
            g_latest = next;
            (void)tk_unl_mtx(g_rgb_mutex);
        }
        (void)tk_dly_tsk(10);
    }
}

int32_t rgb_diag_init(void)
{
    T_CMTX attributes = {0};
    attributes.mtxatr = TA_INHERIT;
    g_latest = (rgb_snapshot_t){.status = RGB_STARTING};
    g_rgb_mutex = tk_cre_mtx(&attributes);
    g_available = g_rgb_mutex >= E_OK;
    return g_rgb_mutex < E_OK ? g_rgb_mutex : E_OK;
}

int32_t rgb_diag_start(void)
{
    if (!g_available)
    {
        return E_NOEXS;
    }
    ID task = app_start_task((FP)rgb_task, APP_PRI_RGB, 0U, "rgb");
    if (task < E_OK)
    {
        g_available = false; /* Startup only, before report task exists. */
    }
    return task < E_OK ? task : E_OK;
}

bool rgb_diag_snapshot(rgb_snapshot_t *p_snapshot)
{
    if (p_snapshot == NULL)
    {
        return false;
    }
    if (!g_available)
    {
        *p_snapshot = (rgb_snapshot_t){.status = RGB_RESOURCE_ERROR, .errors = 1U};
        return true;
    }
    if (tk_loc_mtx(g_rgb_mutex, TMO_POL) != E_OK)
    {
        return false;
    }
    *p_snapshot = g_latest;
    (void)tk_unl_mtx(g_rgb_mutex);
    p_snapshot->valid = p_snapshot->valid && p_snapshot->has_sample &&
        ((uint32_t)(app_now_ms() - p_snapshot->sampled_ms) <= RGB_FRESH_MS);
    return true;
}

static void report_classification(const rgb_snapshot_t *p_sample)
{
    uint32_t now_ms = app_now_ms();
    rgb_classification_t result = rgb_classify(rgb_default_calibration(),
        p_sample, now_ms, RGB_GAIN, RGB_INTEGRATION_US);
    bool has_sample = (p_sample != NULL) && p_sample->has_sample;
    uint32_t age = has_sample ? now_ms - p_sample->sampled_ms : UINT32_MAX;
    tm_printf((UB *)"RGB_CLASS[t=%u sample=%u seq=%u class=%s reason=%s "
              "code=0x%02X read=%u age=%u model=%s DRIVE=" RGB_DRIVE_TEXT "]\n",
              (UINT)now_ms, (UINT)(has_sample ? p_sample->sampled_ms : 0U),
              (UINT)(has_sample ? p_sample->sequence : 0U),
              rgb_class_name(rgb_classification_class(result)),
              rgb_reason_name(rgb_classification_reason(result)), (UINT)result,
              (UINT)(p_sample != NULL), (UINT)age, rgb_calibration_name());
}

void rgb_diag_report(void)
{
    rgb_snapshot_t next;
    if (!rgb_diag_snapshot(&next))
    {
        tm_printf((UB *)"RGB[read=0 valid=0 status=SNAPSHOT_BUSY DRIVE=" RGB_DRIVE_TEXT "]\n");
        report_classification(NULL);
        return;
    }
    uint32_t age = next.has_sample ? app_now_ms() - next.sampled_ms : UINT32_MAX;
    tm_printf((UB *)"RGB[t=%u ready=%u id=0x%02X seq=%u R=%u G=%u B=%u C=%u "
              "has=%u valid=%u sat=%u age=%u status=%u errors=%u "
              "gain=%ux integration_us=%u DRIVE=" RGB_DRIVE_TEXT "]\n",
              (UINT)app_now_ms(), (UINT)next.ready, (UINT)next.device_id,
              (UINT)next.sequence, (UINT)next.red, (UINT)next.green,
              (UINT)next.blue, (UINT)next.clear, (UINT)next.has_sample,
              (UINT)next.valid, (UINT)next.saturated, (UINT)age,
              (UINT)next.status, (UINT)next.errors, (UINT)RGB_GAIN,
              (UINT)RGB_INTEGRATION_US);
    report_classification(&next);
}
#endif
