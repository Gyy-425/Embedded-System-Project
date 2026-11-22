/* Single-owner, allocation-free measurement; no motor or kernel access. */
#ifndef AAI05_ROUTE_MEASUREMENT_H
#define AAI05_ROUTE_MEASUREMENT_H
#include <stdbool.h>
#include <stdint.h>

#define ROUTE_HISTORY_SIZE 64U
#define ROUTE_QUEUE_SIZE 16U
#define ROUTE_COLOUR_COUNT 7U
#define ROUTE_UNKNOWN 7U
/* IDs preserve the RGB classifier IDs. BLUE5 is reserved, not calibrated. */
#define ROUTE_RED 4U
#define ROUTE_YELLOW 2U
#define ROUTE_BLUE 5U

#define ROUTE_OK 0U
#define ROUTE_DUPLICATE 1U
#define ROUTE_CONFIG 2U
#define ROUTE_CAPTURE 3U
#define ROUTE_ORDER 4U
#define ROUTE_JUMP 5U
#define ROUTE_REVERSE 6U
#define ROUTE_OVERFLOW 7U
#define ROUTE_STALE 8U
#define ROUTE_HISTORY 9U
#define ROUTE_UNKNOWN_TIMEOUT 10U
#define ROUTE_QUEUE_FULL 11U
#define ROUTE_FINISHED 12U

#define ROUTE_PARTIAL_START 1U
#define ROUTE_PARTIAL_END 2U
#define ROUTE_UNCERTAIN 4U
#define ROUTE_ABORTED 8U

typedef struct
{
    uint32_t reference_um; /* Actual measured ground distance. Zero is invalid. */
    uint32_t reference_counts; /* Magnitude, not a guessed CPR. */
    int8_t forward_sign; /* -1 or+1, established physically. */
} route_wheel_calibration_t;

typedef struct
{
    route_wheel_calibration_t left;
    route_wheel_calibration_t right;
    uint32_t max_count_step; /* Max magnitude per wheel per motion update. */
    uint16_t max_motion_gap_ms;
    uint16_t max_colour_age_ms;
    uint16_t unknown_timeout_ms;
    uint8_t accepted_mask; /* Class0..6; UNKNOWN7 cannot be enabled. */
} route_config_t;

typedef struct
{
    uint32_t sampled_ms;
    uint32_t epoch; /* Change on encoder reset/reinitialization. */
    int32_t left_count;
    int32_t right_count;
    bool valid; /* Caller combines capture health, PIO warnings and saturation. */
} route_motion_sample_t;

typedef struct
{
    uint32_t sequence; /* Only distinct, ordered acquisition cycles count. */
    uint32_t window_start_ms;
    uint32_t window_end_ms;
    uint8_t colour;
    bool valid;
} route_colour_sample_t;

typedef struct
{
    uint32_t length_um; /* Estimate, not accuracy or a guaranteed tape length. */
    uint32_t minimum_um;
    uint32_t maximum_um;
    uint32_t start_um;
    uint32_t end_um;
    uint8_t colour;
    uint8_t flags;
} route_section_t;

typedef struct
{
    uint32_t path_um;
    uint32_t assigned_um;
    uint32_t unassigned_um;
    uint32_t by_colour_um[ROUTE_COLOUR_COUNT]; /* Closed sections only. */
    uint8_t status;
    uint8_t queued;
    bool finished;
} route_summary_t;

typedef struct {uint32_t sampled_ms, path_um;} route_point_t;
typedef struct {uint32_t estimate, lower, upper;} route_boundary_t;

/* Private fields exposed for static allocation only. Access via APIs below.
 * All functions must execute under ONE owner; no concurrent report access. */
typedef struct
{
    route_config_t config;
    route_motion_sample_t motion;
    route_colour_sample_t previous_colour;
    route_point_t history[ROUTE_HISTORY_SIZE];
    route_section_t queue[ROUTE_QUEUE_SIZE];
    route_summary_t summary;
    route_boundary_t section_start;
    route_boundary_t last_stable;
    route_boundary_t candidate_boundary;
    int64_t left_remainder, right_remainder;
    int8_t centre_remainder;
    uint32_t origin_ms;
    uint32_t unknown_since_ms;
    uint8_t history_head, history_count, queue_head;
    uint8_t current_colour, candidate_colour, candidate_count, section_flags;
    bool has_colour, has_previous_colour, unknown_active, known_start_boundary;
} route_measurement_t;

/* Configuration is copied. No calibration means CONFIG, never invented mm. */
uint8_t route_measurement_init(route_measurement_t *p_state,
    const route_config_t *p_config, const route_motion_sample_t *p_origin,
    bool known_start_boundary);
uint8_t route_measurement_motion(route_measurement_t *p_state,
                                  const route_motion_sample_t *p_sample);
/* Window is acquisition uncertainty, NOT terminal printing time. Caller must
 * supply the true bounded window. Current RGB freeze timestamp alone is not
 * an exact physical colour boundary. Two fresh matching cycles confirm. */
uint8_t route_measurement_colour(route_measurement_t *p_state,
                                  const route_colour_sample_t *p_sample);
uint8_t route_measurement_finish(route_measurement_t *p_state,
                                  bool known_finish_boundary);
bool route_measurement_pop(route_measurement_t *p_state, route_section_t *p_section);
route_summary_t route_measurement_summary(const route_measurement_t *p_state);
#endif
