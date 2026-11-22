#include "follow_route.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>

static follow_route_report_t report;

static void step(bool following, uint16_t epoch, uint32_t time,
                 int32_t left, int32_t right, const rgb_snapshot_t *p_rgb,
                 uint8_t colour)
{
    route_motion_sample_t motion = {time, 1U, left, right, true};
    follow_route_step(following, epoch, &motion, p_rgb, colour, &report);
}

static void ordered_sections_and_coast(void)
{
    const rgb_snapshot_t prestart = {.window_start_ms=100U, .window_end_ms=170U,
        .sequence=1U, .valid=true, .has_sample=true};
    const rgb_snapshot_t red_a = {.window_start_ms=110U, .window_end_ms=180U,
        .sequence=2U, .valid=true, .has_sample=true};
    const rgb_snapshot_t red_b = {.window_start_ms=200U, .window_end_ms=270U,
        .sequence=3U, .valid=true, .has_sample=true};
    const rgb_snapshot_t yellow_a = {.window_start_ms=290U, .window_end_ms=360U,
        .sequence=4U, .valid=true, .has_sample=true};
    const rgb_snapshot_t yellow_b = {.window_start_ms=380U, .window_end_ms=450U,
        .sequence=5U, .valid=true, .has_sample=true};

    step(false, 0U, 90U, 20, -20, NULL, ROUTE_UNKNOWN);
    assert(report.status == FOLLOW_ROUTE_IDLE && !report.finished);
    for (uint32_t time = 100U; time <= 570U; time += 10U)
    {
        const rgb_snapshot_t *p_sample = &prestart;
        uint8_t colour = ROUTE_RED;
        if (time >= 180U) {p_sample = &red_a;}
        if (time >= 270U) {p_sample = &red_b;}
        if (time >= 360U) {p_sample = &yellow_a; colour = ROUTE_YELLOW;}
        if (time >= 450U) {p_sample = &yellow_b;}
        bool running = time < 450U;
        /* Exactly 340 mm of travel, including 20 ms after drive stops. */
        uint32_t advance = (time > 470U ? 470U : time) - 100U;
        int32_t left = (int32_t)(4455U * advance / 370U);
        int32_t right = (int32_t)(4280U * advance / 370U);
        step(running, 1U, time, 20 - left, -20 + right, p_sample, colour);
        assert(report.status == ROUTE_OK && !report.finished);
        if (time == 170U || time == 180U) {assert(report.colour == ROUTE_UNKNOWN);}
        if (time == 190U) {assert(report.colour == ROUTE_RED);}
        if (time == 180U)
        {
            step(true, 1U, time, 19 - left, -20 + right, p_sample, colour);
            assert(report.status == ROUTE_OK); /* Ambiguous same-ms update. */
        }
        if (time == 460U)
        {
            assert(report.count == 1U && report.sections[0].colour == ROUTE_RED);
        }
    }
    step(false, 1U, 590U, 20 - 4455, -20 + 4280, &yellow_b, ROUTE_YELLOW);
    assert(report.finished && report.status == ROUTE_FINISHED);
    assert(report.path_um == 340000U && report.colour == ROUTE_YELLOW);
    assert(report.count == 2U);
    assert(report.sections[0].colour == ROUTE_RED);
    assert((report.sections[0].flags & ROUTE_PARTIAL_START) != 0U);
    assert(report.sections[1].colour == ROUTE_YELLOW);
    assert((report.sections[1].flags & ROUTE_PARTIAL_END) != 0U);
    assert(report.sections[0].length_um + report.sections[1].length_um +
           report.unassigned_um == report.path_um);
    step(false, 1U, 600U, 0, 0, NULL, ROUTE_UNKNOWN);
    assert(report.finished && report.path_um == 340000U && report.count == 2U);
}

static void fault_and_restart(void)
{
    step(true, 2U, 610U, 0, 0, NULL, ROUTE_UNKNOWN);
    assert(report.status == ROUTE_OK && !report.finished);
    assert(report.path_um == 0U && report.count == 0U);
    step(true, 2U, 620U, 10, -10, NULL, ROUTE_UNKNOWN);
    assert(report.status == ROUTE_REVERSE && report.finished);
    step(true, 2U, 630U, -10, 10, NULL, ROUTE_UNKNOWN);
    assert(report.status == ROUTE_REVERSE && report.finished);

    step(true, 3U, 640U, 0, 0, NULL, ROUTE_UNKNOWN);
    assert(report.status == ROUTE_OK && !report.finished);
    const route_motion_sample_t bad = {640U, 1U, 0, 0, false};
    follow_route_step(true, 3U, &bad, NULL, ROUTE_UNKNOWN, &report);
    assert(report.status == ROUTE_CAPTURE && report.finished);
}

static void unsettled_coast(void)
{
    rgb_snapshot_t rgb = {.valid=true, .has_sample=true};
    for (uint32_t time = 1000U; time <= 1500U; time += 10U)
    {
        bool running = time < 1100U;
        if ((time >= 1090U) && (time % 90U == 10U))
        {
            ++rgb.sequence;
            rgb.window_start_ms = time - 80U;
            rgb.window_end_ms = time;
        }
        step(running, 4U, time, 1000 - (int32_t)time, (int32_t)time - 1000,
             rgb.sequence == 0U ? NULL : &rgb, ROUTE_BLUE);
        if (time < 1500U) {assert(report.status == ROUTE_OK && !report.finished);}
    }
    assert(report.status == FOLLOW_ROUTE_COAST_TIMEOUT && report.finished);
    assert(report.count == 1U && report.sections[0].colour == ROUTE_BLUE);
    assert((report.sections[0].flags & ROUTE_PARTIAL_END) != 0U);
}

static void same_tick_motion_interrupts_coast_quiet(void)
{
    for (uint32_t time = 1600U; time <= 1820U; time += 10U)
    {
        step(time == 1600U, 5U, time, 0, 0, NULL, ROUTE_UNKNOWN);
        if (time == 1700U)
        {
            step(false, 5U, time, -1, 1, NULL, ROUTE_UNKNOWN);
        }
        if (time < 1820U) {assert(report.status == ROUTE_OK && !report.finished);}
    }
    assert(report.status == ROUTE_FINISHED && report.finished);
    assert(report.path_um == 0U);
}

static void preserves_first_sixteen_sections_on_overflow(void)
{
    rgb_snapshot_t rgb = {.valid=true, .has_sample=true};
    uint8_t colour = ROUTE_UNKNOWN;
    for (uint32_t elapsed = 0U; elapsed <= 3250U; elapsed += 10U)
    {
        uint32_t time = 2000U + elapsed;
        if (elapsed != 0U && elapsed % 90U == 0U)
        {
            ++rgb.sequence;
            rgb.window_start_ms = time - 80U;
            rgb.window_end_ms = time;
            colour = ((rgb.sequence - 1U) / 2U) % 2U == 0U ? ROUTE_RED : ROUTE_BLUE;
        }
        step(true, 6U, time, -(int32_t)elapsed, (int32_t)elapsed,
             rgb.sequence == 0U ? NULL : &rgb, colour);
        if (elapsed < 3250U) {assert(report.status == ROUTE_OK && !report.finished);}
    }
    assert(report.status == ROUTE_QUEUE_FULL && report.finished);
    assert(report.count == ROUTE_QUEUE_SIZE);
    uint32_t assigned = 0U;
    for (uint8_t i = 0U; i < report.count; ++i)
    {
        assert(report.sections[i].colour == (i % 2U == 0U ? ROUTE_RED : ROUTE_BLUE));
        assigned += report.sections[i].length_um;
    }
    assert(assigned + report.unassigned_um == report.path_um);
}

int main(void)
{
    ordered_sections_and_coast();
    fault_and_restart();
    unsettled_coast();
    same_tick_motion_interrupts_coast_quiet();
    preserves_first_sixteen_sections_on_overflow();
    puts("Follow route: timing, coast and fault checks PASS");
    return 0;
}
