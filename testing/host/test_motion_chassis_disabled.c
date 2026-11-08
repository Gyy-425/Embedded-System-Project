/* A logical request cannot enable motor output in a non-Motion build. */
#include <assert.h>
#include <stdio.h>
#define AAI05_APP_H
#define AAI05_MOTION_APP 0
#include "../../app_program/subsystem2/motion_runtime.c"

int main(void)
{
    motion_chassis_command_t request = {0U, 1U, 40U, 10};
    assert(subsystem2_motion_submit_chassis(&request) == MOTION_UNAVAILABLE);
    assert(subsystem2_motion_submit_chassis(NULL) == MOTION_BAD_COMMAND);
    request.forward_pct = 51U;
    assert(subsystem2_motion_submit_chassis(&request) == MOTION_BAD_COMMAND);
    puts("PASS: logical motion remains unavailable in non-Motion builds");
    return 0;
}
