#ifndef AAI05_SUBSYSTEM1_H
#define AAI05_SUBSYSTEM1_H
#include "../app.h"
#include "colour_runtime.h"
/* Task-context readers: nonblocking, false on busy/no sample/no event.
 * Historical events retain sample-time results; latest rechecks freshness.
 * One network consumer owns pop/begin/take. begin atomically clears old
 * queued events and reads latest; take prefers events over idle snapshots.
 * No model mutation after task start. */
bool subsystem1_colour_latest(colour_report_t *p_report);
bool subsystem1_colour_pop(colour_report_t *p_report);
bool subsystem1_colour_begin(colour_report_t *p_report);
bool subsystem1_colour_take(colour_report_t *p_report, bool *p_is_snapshot,
                             bool allow_snapshot);
#endif
