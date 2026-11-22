#ifndef AAI05_FOLLOW_ROUTE_H
#define AAI05_FOLLOW_ROUTE_H

#include "../subsystem1/rgb_sensor.h"
#include "route_measurement.h"

#define FOLLOW_ROUTE_IDLE UINT8_MAX
#define FOLLOW_ROUTE_COAST_TIMEOUT 13U

typedef struct
{
    uint32_t length_um;
    uint8_t colour;
    uint8_t flags;
} follow_route_section_t;

typedef struct
{
    uint32_t path_um;
    uint32_t unassigned_um;
    uint8_t status; /* ROUTE_* or FOLLOW_ROUTE_* status. */
    uint8_t colour; /* Latest observation consumed in this run. */
    uint8_t count;
    bool finished;
    follow_route_section_t sections[ROUTE_QUEUE_SIZE];
} follow_route_report_t;

/* Call only from the motion owner and continue calling after drive stops.
 * Capture origin counts before the first drive command. A null motion defers
 * an ambiguous capture during a run and faults an ambiguous origin. */
void follow_route_step(bool following, uint16_t epoch,
                       const route_motion_sample_t *p_motion,
                       const rgb_snapshot_t *p_rgb, uint8_t colour,
                       follow_route_report_t *p_report);

#endif
