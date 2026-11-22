#include "black_follow.h"
#include "../..//app_program/subsystem1/rgb_classifier.h"
#include <assert.h>
#include <stdio.h>
typedef struct
{
    uint32_t now_ms;
    uint16_t left_raw, right_raw;
    int8_t expected_sign;
    rgb_snapshot_t rgb;
} follow_vector_t;
#include "red_follow_vectors.h"
int main(void)
{
    unsigned int counts[3]={0U};
    for(size_t i=0U;i<sizeof(g_vectors)/sizeof(g_vectors[0]);++i)
    {
        const follow_vector_t *p=&g_vectors[i];
        rgb_classification_t result=rgb_classify(rgb_default_calibration(),
            &p->rgb,p->now_ms,4U,50400U);
        assert(result==RGB_CLASS_RED);
        black_follow_t state;
        black_follow_init(&state,p->now_ms-100U);
        state.status.phase=FOLLOW_RUNNING;
        state.run_started_ms=p->now_ms-100U;
        black_follow_input_t in={.now_ms=p->now_ms,.ir_ms=p->now_ms,
            .left_raw=p->left_raw,.right_raw=p->right_raw,.has_ir=true,
            .ir_valid=true,.rgb_target_matched=true,.hardware_ready=true};
        black_follow_output_t out=black_follow_step(&state,&in);
        assert(out.drive);
        if(p->expected_sign==0){assert(out.turn_pct==0);++counts[0];}
        else if(p->expected_sign>0){assert(out.turn_pct>0);++counts[1];}
        else{assert(out.turn_pct<0);++counts[2];}
    }
    assert(counts[0]==39U && counts[1]==53U && counts[2]==50U);
    printf("Actual red-corridor replay: centred=%u robot-left=%u robot-right=%u PASS\n",
           counts[0],counts[1],counts[2]);
    return 0;
}
