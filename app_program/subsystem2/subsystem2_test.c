#include "subsystem2.h"
#if APP_TEST
#include "../test_report.h"
#include <limits.h>
EXPORT void subsystem2_test(void)
{
    encoder_state_t state;
    TEST_INIT();
    encoder_init(&state, 0U);
    encoder_update(&state, 1U);
    encoder_update(&state, 3U);
    encoder_update(&state, 2U);
    encoder_update(&state, 0U);
    TEST_CHECK("S2-U101", state.count == 4, "quadrature forward cycle");
    encoder_init(&state, 0U);
    encoder_update(&state, 3U);
    TEST_CHECK("S2-U102", (state.count == 0) && (state.invalid_transitions == 1U),
               "diagonal transition rejected");
    encoder_init(&state, 0U);
    state.count = INT32_MAX;
    encoder_update(&state, 1U);
    TEST_CHECK("S2-U103", (state.count == INT32_MAX) && state.count_saturated,
               "count overflow saturates");
    TEST_SUMMARY("subsystem2 decoder logic");
}
#endif
