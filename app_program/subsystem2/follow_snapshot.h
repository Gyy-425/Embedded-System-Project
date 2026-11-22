#ifndef AAI05_FOLLOW_SNAPSHOT_H
#define AAI05_FOLLOW_SNAPSHOT_H
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include "../app.h"
#pragma GCC diagnostic pop
int32_t follow_snapshot_init(void);
void follow_snapshot_publish(const motion_snapshot_t *p_value);
int32_t follow_snapshot_read(motion_snapshot_t *p_value);
#endif
