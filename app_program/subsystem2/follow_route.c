#include "follow_route.h"
#include <stddef.h>

enum { COAST_QUIET_MS = 120U, COAST_LIMIT_MS = 400U };

static const route_config_t calibration = {
    .left = {340000U, 4455U, -1},
    .right = {340000U, 4280U, 1},
    .max_count_step = 2048U,
    .max_motion_gap_ms = 100U,
    .max_colour_age_ms = 300U,
    .unknown_timeout_ms = 400U,
    .accepted_mask = (1U << ROUTE_RED) | (1U << ROUTE_YELLOW) | (1U << ROUTE_BLUE),
};

/* Only the motion owner may access these. Publish report copies to readers. */
static route_measurement_t measurement;
static follow_route_report_t report = {.status = FOLLOW_ROUTE_IDLE, .colour = ROUTE_UNKNOWN};
static route_motion_sample_t last_motion;
static uint32_t origin_ms, last_rgb_sequence, reported_um;
static uint32_t coast_started_ms, coast_quiet_since_ms;
static uint16_t run_epoch;
static bool was_following, coasting, has_rgb;

static bool time_after(uint32_t later, uint32_t earlier)
{
    uint32_t elapsed = later - earlier;
    return elapsed != 0U && elapsed <= INT32_MAX;
}

static void start_run(uint16_t epoch, const route_motion_sample_t *p_motion)
{
    run_epoch = epoch;
    coasting = false;
    has_rgb = false;
    reported_um = 0U;
    report = (follow_route_report_t){.colour = ROUTE_UNKNOWN};
    last_motion = p_motion != NULL ? *p_motion : (route_motion_sample_t){0};
    origin_ms = last_motion.sampled_ms;
    (void)route_measurement_init(&measurement, &calibration, &last_motion, false);
}

static void observe_rgb(const rgb_snapshot_t *p_rgb, uint8_t colour)
{
    if (p_rgb == NULL || !p_rgb->has_sample ||
        (has_rgb && p_rgb->sequence == last_rgb_sequence) ||
        !time_after(p_rgb->window_start_ms, origin_ms) ||
        !time_after(last_motion.sampled_ms, p_rgb->window_end_ms))
    {
        return;
    }

    if (!p_rgb->valid || (colour != ROUTE_RED && colour != ROUTE_YELLOW && colour != ROUTE_BLUE))
    {
        colour = ROUTE_UNKNOWN;
    }
    const route_colour_sample_t observation = {
        p_rgb->sequence, p_rgb->window_start_ms, p_rgb->window_end_ms, colour, true
    };
    (void)route_measurement_colour(&measurement, &observation);
    report.colour = colour;
    last_rgb_sequence = p_rgb->sequence;
    has_rgb = true;
}

static void update_report(bool coast_timeout)
{
    route_section_t section;
    while (report.count < ROUTE_QUEUE_SIZE && route_measurement_pop(&measurement, &section))
    {
        report.sections[report.count++] = (follow_route_section_t){
            .length_um = section.length_um, .colour = section.colour, .flags = section.flags,
        };
        reported_um += section.length_um;
    }

    route_summary_t summary = route_measurement_summary(&measurement);
    report.path_um = summary.path_um;
    report.unassigned_um = summary.path_um - reported_um;
    report.status = summary.status;
    report.finished = summary.finished;
    if (summary.queued != 0U)
    {
        report.status = ROUTE_QUEUE_FULL;
        report.finished = true;
    }
    else if (coast_timeout && summary.status == ROUTE_FINISHED)
    {
        report.status = FOLLOW_ROUTE_COAST_TIMEOUT;
    }
}

void follow_route_step(bool following, uint16_t epoch,
                       const route_motion_sample_t *p_motion,
                       const rgb_snapshot_t *p_rgb, uint8_t colour,
                       follow_route_report_t *p_report)
{
    if (following && (!was_following || epoch != run_epoch))
    {
        start_run(epoch, p_motion);
        update_report(false);
    }
    else if (report.status == ROUTE_OK && p_motion != NULL)
    {
        uint32_t now = p_motion->sampled_ms;
        if (!following && !coasting)
        {
            coasting = true;
            coast_started_ms = now;
            coast_quiet_since_ms = now;
        }

        /* Updated counts inside the same timestamp bucket cannot be timed. */
        bool moved = p_motion->left_count != last_motion.left_count ||
            p_motion->right_count != last_motion.right_count;
        if (now != last_motion.sampled_ms || !p_motion->valid ||
            p_motion->epoch != last_motion.epoch)
        {
            bool coast_timeout = false;
            if (route_measurement_motion(&measurement, p_motion) == ROUTE_OK)
            {
                if (coasting && moved)
                {
                    coast_quiet_since_ms = now;
                }
                last_motion = *p_motion;
                observe_rgb(p_rgb, colour);
                if (coasting)
                {
                    bool quiet = now - coast_quiet_since_ms >= COAST_QUIET_MS;
                    coast_timeout = now - coast_started_ms >= COAST_LIMIT_MS && !quiet;
                    if (quiet || coast_timeout)
                    {
                        (void)route_measurement_finish(&measurement, false);
                    }
                }
            }
            update_report(coast_timeout);
        }
        else if (coasting && moved)
        {
            /* The bucket is enough to rule out quiet even without an odometry point. */
            coast_quiet_since_ms = now;
        }
    }
    was_following = following;
    if (p_report != NULL)
    {
        *p_report = report;
    }
}
