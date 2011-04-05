#ifndef HPCTIMER_H_ 
#define HPCTIMER_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HPCTIMER_GETTIMEOFDAY   = 0,
    HPCTIMER_TSC            = 1  
} hpctimer_type_t;

typedef struct {
    hpctimer_type_t type;
    unsigned long long (*gettime) (); 
} hpctimer_t;


hpctimer_t *hpctimer_init(hpctimer_type_t type);

void hpctimer_free(hpctimer_t *timer);

double hpctimer_wtime(hpctimer_t *timer);

#ifdef __cplusplus
}
#endif    

#endif // HPCTIMER_H
