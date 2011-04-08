/*
 * hpctimer.h: high-resolution timers library.
 */

#ifndef HPCTIMER_H_ 
#define HPCTIMER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <sched.h>

#define uint64_t unsigned long long int
#define uint32_t unsigned int

#define HPCTIMER_BINDTOCPU          0x00000001

typedef enum {
    HPCTIMER_GETTIMEOFDAY   = 0,
    HPCTIMER_TSC            = 1,  
    HPCTIMER_MPIWTIME       = 2,
    HPCTIMER_CLOCKGETTIME   = 3
} hpctimer_type_t;
/*
typedef struct {
    hpctimer_type_t type;
    uint32_t flags;
    uint32_t clock_type;    * for CLOCKGETTIME *
    uint64_t overhead;      * in ticks /
    uint64_t freq           * in MHz *;
    uint64_t (*gettime) ();
    cpu_set_t cpuset; 
} hpctimer_t;
*/
typedef struct hpctimer hpctimer_t;

hpctimer_t *hpctimer_init(hpctimer_type_t type, uint32_t flags);

void hpctimer_free(hpctimer_t *timer);

double hpctimer_wtime(hpctimer_t *timer);

uint64_t hpctimer_get_overhead(hpctimer_t *timer);

#ifdef __cplusplus
}
#endif    

#endif // HPCTIMER_H
