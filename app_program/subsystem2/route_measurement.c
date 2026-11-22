#include "route_measurement.h"
#include <limits.h>
#include <stddef.h>

static bool calibration_ok(const route_wheel_calibration_t *p_cal)
{
    return (p_cal->reference_um > 0U) && (p_cal->reference_um <= 1000000000U) &&
        (p_cal->reference_counts > 0U) && (p_cal->reference_counts <= INT32_MAX) &&
        ((p_cal->forward_sign == -1) || (p_cal->forward_sign == 1));
}

static void append_point(route_measurement_t *p_state)
{
    p_state->history[p_state->history_head] = (route_point_t){
        p_state->motion.sampled_ms, p_state->summary.path_um};
    p_state->history_head = (uint8_t)((p_state->history_head + 1U) % ROUTE_HISTORY_SIZE);
    if (p_state->history_count < ROUTE_HISTORY_SIZE) {++p_state->history_count;}
}

static bool position_at(const route_measurement_t *p_state, uint32_t time,
                         uint32_t *p_position)
{
    uint8_t oldest = (uint8_t)((p_state->history_head + ROUTE_HISTORY_SIZE -
                               p_state->history_count) % ROUTE_HISTORY_SIZE);
    route_point_t before = p_state->history[oldest];
    uint32_t age = p_state->motion.sampled_ms - time;
    if ((age > INT32_MAX) ||
        (age > p_state->motion.sampled_ms - before.sampled_ms)) {return false;}
    if (time == before.sampled_ms) {*p_position=before.path_um; return true;}
    for (uint8_t i=1U; i<p_state->history_count; ++i)
    {
        route_point_t after = p_state->history[(oldest + i) % ROUTE_HISTORY_SIZE];
        uint32_t span = after.sampled_ms - before.sampled_ms;
        uint32_t offset = time - before.sampled_ms;
        if (offset <= span)
        {
            *p_position = before.path_um + (uint32_t)(
                (uint64_t)(after.path_um - before.path_um) * offset / span);
            return true;
        }
        before=after;
    }
    return false;
}

static bool close_section(route_measurement_t *p_state, route_boundary_t end,
                            uint8_t flags)
{
    if (!p_state->has_colour) {return true;}
    if (p_state->summary.queued >= ROUTE_QUEUE_SIZE) {return false;}
    route_boundary_t start = p_state->section_start;
    if (end.estimate < start.estimate) {end.estimate=start.estimate;}
    uint32_t length = end.estimate - start.estimate;
    route_section_t section = {.length_um=length,
        .minimum_um=end.lower > start.upper ? end.lower-start.upper : 0U,
        .maximum_um=end.upper > start.lower ? end.upper-start.lower : 0U,
        .start_um=start.estimate, .end_um=end.estimate,
        .colour=p_state->current_colour,
        .flags=(uint8_t)(p_state->section_flags | flags)};
    if ((start.lower != start.upper) || (end.lower != end.upper))
    {section.flags |= ROUTE_UNCERTAIN;}
    uint8_t index = (uint8_t)((p_state->queue_head+p_state->summary.queued) % ROUTE_QUEUE_SIZE);
    p_state->queue[index]=section;
    ++p_state->summary.queued;
    p_state->summary.assigned_um+=length; /* Closed estimates partition path. */
    p_state->summary.by_colour_um[p_state->current_colour]+=length;
    p_state->has_colour=false;
    return true;
}

static uint8_t fault(route_measurement_t *p_state, uint8_t cause)
{
    if (p_state->summary.status != ROUTE_OK) {return p_state->summary.status;}
    /* End at the last confirmed observation, not unobserved later travel. */
    if (!close_section(p_state,p_state->last_stable,
                       ROUTE_PARTIAL_END | ROUTE_UNCERTAIN | ROUTE_ABORTED))
    {cause=ROUTE_QUEUE_FULL;}
    p_state->has_colour=false;
    p_state->summary.status=cause;
    p_state->summary.finished=true;
    return cause;
}

uint8_t route_measurement_init(route_measurement_t *p_state,
    const route_config_t *p_config, const route_motion_sample_t *p_origin,
    bool known_start_boundary)
{
    if (p_state == NULL) {return ROUTE_CONFIG;}
    *p_state=(route_measurement_t){0};
    p_state->summary.status=ROUTE_CONFIG;
    if ((p_config == NULL) || (p_origin == NULL) ||
        !calibration_ok(&p_config->left) || !calibration_ok(&p_config->right) ||
        (p_config->max_count_step == 0U) || (p_config->max_count_step > 1000000U) ||
        (p_config->max_motion_gap_ms == 0U) || (p_config->max_motion_gap_ms > 1000U) ||
        (p_config->max_colour_age_ms == 0U) || (p_config->max_colour_age_ms > 1000U) ||
        (p_config->unknown_timeout_ms == 0U) || (p_config->unknown_timeout_ms > 1000U) ||
        (p_config->accepted_mask == 0U) || ((p_config->accepted_mask & 0x80U) != 0U))
    {return ROUTE_CONFIG;}
    p_state->config=*p_config;
    p_state->motion=*p_origin;
    p_state->origin_ms=p_origin->sampled_ms;
    p_state->known_start_boundary=known_start_boundary;
    p_state->summary.status=ROUTE_OK;
    if (!p_origin->valid) {return fault(p_state,ROUTE_CAPTURE);}
    append_point(p_state);
    return ROUTE_OK;
}

static int64_t wheel_distance(int64_t count, const route_wheel_calibration_t *p_cal,
                               int64_t *p_remainder)
{
    int64_t numerator = count * p_cal->forward_sign * p_cal->reference_um + *p_remainder;
    int64_t result = numerator / p_cal->reference_counts;
    *p_remainder = numerator % p_cal->reference_counts;
    return result;
}

static uint64_t common_divisor(uint64_t left, uint64_t right)
{
    while (right != 0U)
    {uint64_t remainder=left%right; left=right; right=remainder;}
    return left;
}

static bool balanced_pivot(int64_t left, int64_t right, const route_config_t *p_config)
{
    left*=p_config->left.forward_sign;
    right*=p_config->right.forward_sign;
    if ((left == 0) && (right == 0)) {return true;}
    if (((left >= 0) && (right >= 0)) || ((left <= 0) && (right <= 0))) {return false;}
    uint64_t a=(uint64_t)(left < 0 ? -left : left);
    uint64_t b=(uint64_t)(right < 0 ? -right : right);
    uint64_t count_divisor=common_divisor(a,b);
    /* Products <=1e9*INT32_MAX: reduce ratios BEFORE multiplying by steps. */
    uint64_t left_units=(uint64_t)p_config->right.reference_um*p_config->left.reference_counts;
    uint64_t right_units=(uint64_t)p_config->left.reference_um*p_config->right.reference_counts;
    uint64_t scale_divisor=common_divisor(left_units,right_units);
    return (a/count_divisor == left_units/scale_divisor) &&
           (b/count_divisor == right_units/scale_divisor);
}

static uint8_t watchdog(route_measurement_t *p_state)
{
    uint32_t now=p_state->motion.sampled_ms;
    if (p_state->unknown_active &&
        (now-p_state->unknown_since_ms >= p_state->config.unknown_timeout_ms))
    {return fault(p_state,ROUTE_UNKNOWN_TIMEOUT);}
    uint32_t last = p_state->has_previous_colour ? p_state->previous_colour.window_end_ms :
        p_state->origin_ms;
    if (now-last > p_state->config.max_colour_age_ms) {return fault(p_state,ROUTE_STALE);}
    return ROUTE_OK;
}

uint8_t route_measurement_motion(route_measurement_t *p_state,
                                  const route_motion_sample_t *p_sample)
{
    if (p_state == NULL) {return ROUTE_CONFIG;}
    if (p_state->summary.status != ROUTE_OK) {return p_state->summary.status;}
    if ((p_sample == NULL) || !p_sample->valid || (p_sample->epoch != p_state->motion.epoch))
    {return fault(p_state,ROUTE_CAPTURE);}
    uint32_t elapsed=p_sample->sampled_ms-p_state->motion.sampled_ms;
    int64_t left=(int64_t)p_sample->left_count-p_state->motion.left_count;
    int64_t right=(int64_t)p_sample->right_count-p_state->motion.right_count;
    if (elapsed == 0U)
    {return (left == 0 && right == 0) ? ROUTE_DUPLICATE : fault(p_state,ROUTE_ORDER);}
    if (elapsed > p_state->config.max_motion_gap_ms) {return fault(p_state,ROUTE_ORDER);}
    int64_t limit=p_state->config.max_count_step;
    if ((left < -limit) || (left > limit) || (right < -limit) || (right > limit))
    {return fault(p_state,ROUTE_JUMP);}
    int64_t centre=0;
    if (!balanced_pivot(left,right,&p_state->config))
    {
        int64_t sum=wheel_distance(left,&p_state->config.left,&p_state->left_remainder)+
            wheel_distance(right,&p_state->config.right,&p_state->right_remainder)+
            p_state->centre_remainder;
        centre=sum/2;
        p_state->centre_remainder=(int8_t)(sum%2);
    }
    if (centre < 0) {return fault(p_state,ROUTE_REVERSE);}
    if ((uint64_t)centre > UINT32_MAX-p_state->summary.path_um)
    {return fault(p_state,ROUTE_OVERFLOW);}
    p_state->summary.path_um+=(uint32_t)centre;
    p_state->motion=*p_sample;
    append_point(p_state);
    return watchdog(p_state);
}

static bool same_observation(const route_colour_sample_t *p_a,
                              const route_colour_sample_t *p_b)
{
    return (p_a->window_start_ms == p_b->window_start_ms) &&
        (p_a->window_end_ms == p_b->window_end_ms) && (p_a->colour == p_b->colour) &&
        (p_a->valid == p_b->valid);
}

uint8_t route_measurement_colour(route_measurement_t *p_state,
                                  const route_colour_sample_t *p_sample)
{
    if (p_state == NULL) {return ROUTE_CONFIG;}
    if (p_state->summary.status != ROUTE_OK) {return p_state->summary.status;}
    if ((p_sample == NULL) || !p_sample->valid || (p_sample->colour > ROUTE_UNKNOWN))
    {return fault(p_state,ROUTE_CAPTURE);}
    uint32_t now=p_state->motion.sampled_ms;
    if ((p_sample->window_end_ms-p_sample->window_start_ms > p_state->config.max_colour_age_ms) ||
        (now-p_sample->window_end_ms > p_state->config.max_colour_age_ms))
    {return fault(p_state,ROUTE_STALE);}
    if (p_state->has_previous_colour)
    {
        uint32_t delta=p_sample->sequence-p_state->previous_colour.sequence;
        if (delta == 0U)
        {return same_observation(p_sample,&p_state->previous_colour) ? ROUTE_DUPLICATE : fault(p_state,ROUTE_ORDER);}
        if ((delta > INT32_MAX) ||
            (p_sample->window_end_ms == p_state->previous_colour.window_end_ms) ||
            (p_sample->window_end_ms-p_state->previous_colour.window_end_ms > INT32_MAX))
        {return fault(p_state,ROUTE_ORDER);}
        if (delta != 1U) {p_state->section_flags |= ROUTE_UNCERTAIN;}
    }
    route_boundary_t observed;
    if (!position_at(p_state,p_sample->window_start_ms,&observed.lower) ||
        !position_at(p_state,p_sample->window_end_ms,&observed.upper))
    {return fault(p_state,ROUTE_HISTORY);}
    observed.estimate=observed.lower+(observed.upper-observed.lower)/2U;
    p_state->previous_colour=*p_sample;
    p_state->has_previous_colour=true;
    bool accepted=(p_sample->colour < ROUTE_COLOUR_COUNT) &&
        ((p_state->config.accepted_mask & (1U<<p_sample->colour)) != 0U);
    if (!accepted)
    {
        if (!p_state->unknown_active)
        {p_state->unknown_since_ms=p_sample->window_end_ms; p_state->unknown_active=true;}
        p_state->section_flags |= ROUTE_UNCERTAIN;
        p_state->candidate_count=0U;
        return watchdog(p_state);
    }
    if (p_state->has_colour && (p_sample->colour == p_state->current_colour))
    {
        if (p_state->candidate_count != 0U) {p_state->section_flags |= ROUTE_UNCERTAIN;}
        p_state->candidate_count=0U;
        p_state->unknown_active=false;
        p_state->last_stable=observed;
        return ROUTE_OK;
    }
    if ((p_state->candidate_count == 0U) || (p_state->candidate_colour != p_sample->colour))
    {
        if (p_state->candidate_count != 0U) {p_state->section_flags |= ROUTE_UNCERTAIN;}
        p_state->candidate_colour=p_sample->colour;
        p_state->candidate_count=1U;
        p_state->candidate_boundary=observed;
        if (p_state->has_colour)
        {
            p_state->candidate_boundary.lower=p_state->last_stable.lower;
            p_state->candidate_boundary.estimate=p_state->candidate_boundary.lower+
                (observed.upper-p_state->candidate_boundary.lower)/2U;
            if (p_state->candidate_boundary.estimate < p_state->section_start.estimate)
            {p_state->candidate_boundary.estimate=p_state->section_start.estimate;}
        }
        return watchdog(p_state);
    }
    route_boundary_t start=p_state->candidate_boundary;
    if (p_state->has_colour)
    {
        if (!close_section(p_state,start,0U)) {return fault(p_state,ROUTE_QUEUE_FULL);}
        p_state->section_flags=0U;
    }
    else if (p_state->known_start_boundary && (start.lower == 0U))
    {start=(route_boundary_t){0};}
    else {p_state->section_flags |= ROUTE_PARTIAL_START;}
    p_state->current_colour=p_sample->colour;
    p_state->has_colour=true;
    p_state->section_start=start;
    p_state->last_stable=observed;
    p_state->candidate_count=0U;
    p_state->unknown_active=false;
    return ROUTE_OK;
}

uint8_t route_measurement_finish(route_measurement_t *p_state, bool known_finish_boundary)
{
    if (p_state == NULL) {return ROUTE_CONFIG;}
    if (p_state->summary.status != ROUTE_OK) {return p_state->summary.status;}
    route_boundary_t end={p_state->summary.path_um,p_state->summary.path_um,p_state->summary.path_um};
    uint8_t flags=known_finish_boundary ? 0U : ROUTE_PARTIAL_END;
    if (p_state->unknown_active || (p_state->candidate_count != 0U))
    {end=p_state->last_stable; flags |= ROUTE_PARTIAL_END | ROUTE_UNCERTAIN;}
    if (!close_section(p_state,end,flags)) {return fault(p_state,ROUTE_QUEUE_FULL);}
    p_state->summary.finished=true;
    p_state->summary.status=ROUTE_FINISHED;
    return ROUTE_OK;
}

bool route_measurement_pop(route_measurement_t *p_state, route_section_t *p_section)
{
    if ((p_state == NULL) || (p_section == NULL) || (p_state->summary.queued == 0U)) {return false;}
    *p_section=p_state->queue[p_state->queue_head];
    p_state->queue_head=(uint8_t)((p_state->queue_head+1U) % ROUTE_QUEUE_SIZE);
    --p_state->summary.queued;
    return true;
}

route_summary_t route_measurement_summary(const route_measurement_t *p_state)
{
    if (p_state == NULL) {return (route_summary_t){.status=ROUTE_CONFIG};}
    route_summary_t summary=p_state->summary;
    /* While running this includes open, not-yet-committed sections. */
    summary.unassigned_um=summary.path_um-summary.assigned_um;
    return summary;
}
