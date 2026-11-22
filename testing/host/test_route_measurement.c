#include "route_measurement.h"
#include <assert.h>
#include <stdio.h>

static route_config_t config(void)
{
    return (route_config_t){.left={1000U,10U,-1},.right={1000U,10U,1},
        .max_count_step=1000U,.max_motion_gap_ms=100U,
        .max_colour_age_ms=200U,.unknown_timeout_ms=250U,
        .accepted_mask=(1U<<ROUTE_RED)|(1U<<ROUTE_YELLOW)|(1U<<ROUTE_BLUE)};
}


static void initialize(route_measurement_t *p_state, bool known)
{
    route_config_t cfg=config();
    route_motion_sample_t origin={.valid=true,.epoch=42U};
    assert(route_measurement_init(p_state,&cfg,&origin,known)==ROUTE_OK);
}

static void motion(route_measurement_t *p_state, uint32_t t, int32_t counts)
{
    route_motion_sample_t sample={.sampled_ms=t,.epoch=42U,
        .left_count=-counts,.right_count=counts,.valid=true};
    assert(route_measurement_motion(p_state,&sample)==ROUTE_OK);
}

static uint8_t colour(route_measurement_t *p_state,uint32_t seq,uint32_t t,uint8_t id)
{
    route_colour_sample_t sample={.sequence=seq,.window_start_ms=t,
        .window_end_ms=t,.colour=id,.valid=true};
    return route_measurement_colour(p_state,&sample);
}

static void initial_red(route_measurement_t *p_state)
{
    initialize(p_state,true);
    assert(colour(p_state,0U,0U,ROUTE_RED)==ROUTE_OK);
    motion(p_state,10U,0);
    assert(colour(p_state,1U,10U,ROUTE_RED)==ROUTE_OK);
}

static void section_replay(void)
{
    route_measurement_t state;
    initialize(&state,true);
    assert(colour(&state,0U,0U,ROUTE_RED)==ROUTE_OK);
    for(uint32_t t=10U;t<=230U;t+=10U)
    {
        /* Synthetic100mm/s and100ms colour intervals, not hardware timing. */
        motion(&state,t*10U,(int32_t)t*10);
        uint8_t id=t<100U ? ROUTE_RED : t==100U ? ROUTE_UNKNOWN :
                   t<150U ? ROUTE_YELLOW : t==150U ? ROUTE_UNKNOWN : ROUTE_BLUE;
        assert(colour(&state,t/10U,t*10U,id)==ROUTE_OK);
    }
    assert(route_measurement_finish(&state,true)==ROUTE_OK);
    assert(route_measurement_finish(&state,true)==ROUTE_FINISHED);
    const uint32_t expected[3]={100000U,50000U,80000U};
    const uint32_t minimum[3]={90000U,30000U,70000U};
    const uint32_t maximum[3]={110000U,70000U,90000U};
    const uint8_t ids[3]={ROUTE_RED,ROUTE_YELLOW,ROUTE_BLUE};
    route_section_t section;
    for(unsigned int i=0U;i<3U;++i)
    {
        assert(route_measurement_pop(&state,&section));
        assert(section.colour==ids[i] && section.length_um==expected[i]);
        assert(section.minimum_um==minimum[i] && section.maximum_um==maximum[i]);
        assert((section.flags & (ROUTE_PARTIAL_START|ROUTE_PARTIAL_END))==0U);

    }
    assert(!route_measurement_pop(&state,&section));
    route_summary_t summary=route_measurement_summary(&state);
    assert(summary.path_um==230000U && summary.assigned_um==230000U && summary.unassigned_um==0U);
    assert(summary.by_colour_um[ROUTE_RED]==100000U && summary.by_colour_um[ROUTE_YELLOW]==50000U &&
           summary.by_colour_um[ROUTE_BLUE]==80000U);

}

static void calibration_and_motion(void)
{
    route_measurement_t state;
    route_config_t cfg=config();
    route_motion_sample_t sample={.valid=true,.epoch=42U};
    cfg.left.reference_counts=0U;
    assert(route_measurement_init(&state,&cfg,&sample,true)==ROUTE_CONFIG);
    cfg=config(); cfg.right.forward_sign=0;
    assert(route_measurement_init(&state,&cfg,&sample,true)==ROUTE_CONFIG);
    cfg=config(); cfg.accepted_mask=0x80U;
    assert(route_measurement_init(&state,&cfg,&sample,true)==ROUTE_CONFIG);
    initialize(&state,true);
    sample=(route_motion_sample_t){.sampled_ms=10U,.epoch=42U,.left_count=-100,.right_count=-100,.valid=true};
    assert(route_measurement_motion(&state,&sample)==ROUTE_OK); /* Pure pivot. */
    assert(route_measurement_summary(&state).path_um==0U);
    assert(route_measurement_motion(&state,&sample)==ROUTE_DUPLICATE);
    sample.left_count=-99;
    assert(route_measurement_motion(&state,&sample)==ROUTE_ORDER);
    initialize(&state,true); sample=(route_motion_sample_t){.sampled_ms=10U,.epoch=43U,.valid=true};
    assert(route_measurement_motion(&state,&sample)==ROUTE_CAPTURE);
    initialize(&state,true); sample=(route_motion_sample_t){.sampled_ms=10U,.epoch=42U,.left_count=1,.right_count=-1,.valid=true};
    assert(route_measurement_motion(&state,&sample)==ROUTE_REVERSE);
    initialize(&state,true); sample.left_count=-1001;sample.right_count=1001;
    assert(route_measurement_motion(&state,&sample)==ROUTE_JUMP);
    initialize(&state,true); sample=(route_motion_sample_t){.sampled_ms=101U,.epoch=42U,.valid=true};
    assert(route_measurement_motion(&state,&sample)==ROUTE_ORDER);
    /* Carry fractions: three counts equal one um, no per-update rounding loss. */
    cfg=config();cfg.left=(route_wheel_calibration_t){1U,3U,-1};
    cfg.right=(route_wheel_calibration_t){1U,3U,1};
    sample=(route_motion_sample_t){.epoch=42U,.valid=true};
    assert(route_measurement_init(&state,&cfg,&sample,true)==ROUTE_OK);
    for(uint32_t i=1U;i<=6U;++i){motion(&state,i,(int32_t)i);}
    assert(route_measurement_summary(&state).path_um==2U);
    /* Fractional history cannot turn a mathematically balanced pivot into travel. */
    cfg=config();cfg.left=(route_wheel_calibration_t){1U,10U,1};
    cfg.right=(route_wheel_calibration_t){1U,10U,1};
    sample=(route_motion_sample_t){.epoch=42U,.valid=true};
    assert(route_measurement_init(&state,&cfg,&sample,true)==ROUTE_OK);
    sample.sampled_ms=1U;sample.left_count=9;sample.right_count=9;
    assert(route_measurement_motion(&state,&sample)==ROUTE_OK);
    sample.sampled_ms=2U;sample.left_count=21;sample.right_count=-3;
    assert(route_measurement_motion(&state,&sample)==ROUTE_OK);
    assert(route_measurement_summary(&state).path_um==0U);
    cfg=config();cfg.left.reference_um=1000000000U;cfg.left.reference_counts=1U;
    cfg.right.reference_um=1000000000U;cfg.right.reference_counts=1U;
    sample=(route_motion_sample_t){.epoch=42U,.valid=true};
    assert(route_measurement_init(&state,&cfg,&sample,true)==ROUTE_OK);
    motion(&state,10U,4);sample=(route_motion_sample_t){.sampled_ms=20U,.epoch=42U,.left_count=-5,.right_count=5,.valid=true};
    assert(route_measurement_motion(&state,&sample)==ROUTE_OVERFLOW);
}

static void time_and_colour(void)
{
    route_measurement_t state;
    initial_red(&state);
    for(uint32_t t=20U;t<=120U;t+=10U){motion(&state,t,(int32_t)t*10);}
    assert(colour(&state,2U,90U,ROUTE_RED)==ROUTE_OK);
    route_colour_sample_t sample={.sequence=3U,.window_start_ms=95U,.window_end_ms=105U,.colour=ROUTE_YELLOW,.valid=true};
    assert(route_measurement_colour(&state,&sample)==ROUTE_OK);
    assert(route_measurement_colour(&state,&sample)==ROUTE_DUPLICATE);
    sample=(route_colour_sample_t){.sequence=4U,.window_start_ms=110U,.window_end_ms=120U,.colour=ROUTE_YELLOW,.valid=true};
    assert(route_measurement_colour(&state,&sample)==ROUTE_OK);
    route_section_t section;assert(route_measurement_pop(&state,&section));
    assert(section.length_um==97500U && section.minimum_um==90000U && section.maximum_um==105000U);
    /* A new sequence with the same acquisition time is not a fresh confirmation. */
    initial_red(&state);
    assert(colour(&state,2U,10U,ROUTE_YELLOW)==ROUTE_ORDER);
    initial_red(&state);
    assert(colour(&state,0U,10U,ROUTE_RED)==ROUTE_ORDER);
    initial_red(&state);motion(&state,20U,100);
    assert(colour(&state,2U,20U,ROUTE_BLUE)==ROUTE_OK);
    motion(&state,30U,200);assert(colour(&state,3U,30U,ROUTE_RED)==ROUTE_OK);
    assert(route_measurement_finish(&state,false)==ROUTE_OK);
    assert(route_measurement_pop(&state,&section) && section.colour==ROUTE_RED &&
           (section.flags & ROUTE_UNCERTAIN)!=0U && (section.flags & ROUTE_PARTIAL_END)!=0U);
    assert(route_measurement_summary(&state).by_colour_um[ROUTE_BLUE]==0U);
    /* Missing acquisition cycles must not leave a confidently continuous section. */
    initial_red(&state);motion(&state,20U,100);
    assert(colour(&state,5U,20U,ROUTE_RED)==ROUTE_OK);
    assert(route_measurement_finish(&state,true)==ROUTE_OK);
    assert(route_measurement_pop(&state,&section) && (section.flags & ROUTE_UNCERTAIN)!=0U);
    /* Initial observations after movement cannot prove the colour at origin. */
    initialize(&state,true);motion(&state,10U,100);
    assert(colour(&state,0U,10U,ROUTE_RED)==ROUTE_OK);
    motion(&state,20U,200);assert(colour(&state,1U,20U,ROUTE_RED)==ROUTE_OK);
    assert(route_measurement_finish(&state,false)==ROUTE_OK);
    assert(route_measurement_pop(&state,&section) && (section.flags & ROUTE_PARTIAL_START)!=0U);
    assert(route_measurement_summary(&state).unassigned_um==10000U);
    /* Rolling history must not hide the absence of any colour observations. */
    initialize(&state,true);
    for(uint32_t t=1U;t<=200U;++t){motion(&state,t,0);}
    route_motion_sample_t m={.sampled_ms=201U,.epoch=42U,.valid=true};
    assert(route_measurement_motion(&state,&m)==ROUTE_STALE);
}

static void faults_and_capacity(void)
{
    route_measurement_t state;
    initial_red(&state);motion(&state,20U,100);
    assert(colour(&state,2U,20U,ROUTE_UNKNOWN)==ROUTE_OK);
    for(uint32_t t=30U;t<270U;t+=10U)
    {
        motion(&state,t,(int32_t)t*10);
        if(t==100U || t==200U){assert(colour(&state,t,t,ROUTE_UNKNOWN)==ROUTE_OK);}
    }
    route_motion_sample_t m={.sampled_ms=270U,.epoch=42U,.left_count=-2700,.right_count=2700,.valid=true};
    assert(route_measurement_motion(&state,&m)==ROUTE_UNKNOWN_TIMEOUT);
    assert(route_measurement_summary(&state).unassigned_um>0U);
    initial_red(&state);route_colour_sample_t bad={.sequence=2U,.valid=false};
    assert(route_measurement_colour(&state,&bad)==ROUTE_CAPTURE);
    initial_red(&state);
    for(uint32_t t=11U;t<=100U;++t){motion(&state,t,0);}
    route_colour_sample_t old={.sequence=2U,.window_start_ms=10U,.window_end_ms=20U,.valid=true,.colour=ROUTE_RED};
    assert(route_measurement_colour(&state,&old)==ROUTE_HISTORY);
    /* A full event queue faults, retaining its earlier records. */
    initial_red(&state);
    uint32_t t=10U,seq=1U;uint8_t result=ROUTE_OK;
    for(unsigned int i=0U;i<17U;++i)
    {
        uint8_t id=(i%2U)==0U ? ROUTE_YELLOW : ROUTE_RED;
        t+=10U;motion(&state,t,(int32_t)t);
        assert(colour(&state,++seq,t,id)==ROUTE_OK);
        t+=10U;motion(&state,t,(int32_t)t);
        result=colour(&state,++seq,t,id);
        if(result!=ROUTE_OK){break;}
    }
    assert(result==ROUTE_QUEUE_FULL && route_measurement_summary(&state).queued==16U);
    /* Unsigned timestamp and colour sequence wrap remain ordered. */
    route_config_t cfg=config();m=(route_motion_sample_t){.sampled_ms=UINT32_MAX-30U,.epoch=42U,.valid=true};
    assert(route_measurement_init(&state,&cfg,&m,true)==ROUTE_OK);
    assert(colour(&state,UINT32_MAX,UINT32_MAX-30U,ROUTE_RED)==ROUTE_OK);
    m.sampled_ms=UINT32_MAX-10U;assert(route_measurement_motion(&state,&m)==ROUTE_OK);
    assert(colour(&state,0U,UINT32_MAX-10U,ROUTE_RED)==ROUTE_OK);
    m.sampled_ms=9U;m.left_count=-10;m.right_count=10;
    assert(route_measurement_motion(&state,&m)==ROUTE_OK);
    assert(colour(&state,1U,9U,ROUTE_RED)==ROUTE_OK);
    assert(route_measurement_finish(&state,true)==ROUTE_OK);
    route_section_t section;assert(route_measurement_pop(&state,&section) && section.length_um==1000U);
}

int main(void)
{
    section_replay();
    calibration_and_motion();
    time_and_colour();
    faults_and_capacity();
    puts("Route measurement: synthetic route, calibration/signs/remainders, pivots, windows/confirmation, faults/capacity/wrap PASS");
    return 0;
}
