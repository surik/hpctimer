/*
 * hpctimer.h: high-resolution timers library.
 */

#ifndef HPCTIMER_H_ 
#define HPCTIMER_H_

#ifdef __cplusplus
extern "C" {
#endif

#define uint64_t unsigned long long int
#define uint32_t unsigned int

typedef enum {
    HPCTIMER_GETTIMEOFDAY   = 0,
    HPCTIMER_TSC            = 1  
} hpctimer_type_t;

typedef struct {
    hpctimer_type_t type;
    uint64_t overhead;
    uint64_t freq;
    uint64_t (*gettime) (); 
} hpctimer_t;


hpctimer_t *hpctimer_init(hpctimer_type_t type);

void hpctimer_free(hpctimer_t *timer);

double hpctimer_wtime(hpctimer_t *timer);

#ifdef __cplusplus
}
#endif    

#endif // HPCTIMER_H
