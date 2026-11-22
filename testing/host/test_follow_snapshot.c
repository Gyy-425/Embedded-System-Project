/* Real copied-snapshot store under concurrent producer/consumer access. */
#define AAI05_APP_H
#define AAI05_FOLLOW_APP 1
#define __TK_SMP_H__
#include <assert.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
typedef unsigned int UINT;
#define E_PAR (-17)
#define E_NOEXS (-42)
typedef pthread_mutex_t T_SPLOCK;
typedef struct {uint32_t sequence; uint32_t payload[64];} motion_snapshot_t;
static int InitSpinLock(T_SPLOCK *p_lock) {return pthread_mutex_init(p_lock,NULL);}
static int ISpinLock(T_SPLOCK *p_lock,UINT *p_saved)
{*p_saved=17U; return pthread_mutex_lock(p_lock);}
static int ISpinUnlock(T_SPLOCK *p_lock,UINT saved)
{assert(saved==17U);return pthread_mutex_unlock(p_lock);}
#include "../../app_program/subsystem2/follow_snapshot.c"
static void *writer(void *p_extra)
{
    (void)p_extra;
    for(uint32_t i=1U;i<=10000U;++i)
    {
        motion_snapshot_t value={.sequence=i};
        for(unsigned int j=0U;j<64U;++j){value.payload[j]=i;}
        follow_snapshot_publish(&value);
    }
    return NULL;
}
int main(void)
{
    motion_snapshot_t value;
    assert(follow_snapshot_read(&value)<0);
    assert(follow_snapshot_init()==0);
    assert(follow_snapshot_read(NULL)<0);
    pthread_t thread;assert(pthread_create(&thread,NULL,writer,NULL)==0);
    for(unsigned int i=0U;i<10000U;++i)
    {
        assert(follow_snapshot_read(&value)==0);
        for(unsigned int j=0U;j<64U;++j){assert(value.payload[j]==value.sequence);}
    }
    assert(pthread_join(thread,NULL)==0);
    puts("Follow snapshot: coherent copy under concurrent publication PASS");
    return 0;
}
