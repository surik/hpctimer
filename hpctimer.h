/*
 * hpctimer.h: High-resolution timers library.
 *
 */

#ifndef HPCTIMER_H 
#define HPCTIMER_H

#ifdef __cplusplus
extern "C" {
#endif

#define uint64_t unsigned long int
#define uint32_t unsigned int

/** Flag to binding timer to cpu */
#define HPCTIMER_BINDTOCPU 0x00000001

/** type of timers */
typedef enum {
    HPCTIMER_GETTIMEOFDAY = 0,
    HPCTIMER_TSC          = 1,  
    HPCTIMER_MPIWTIME     = 2,
    HPCTIMER_CLOCKGETTIME = 3
} hpctimer_type_t;

typedef struct hpctimer hpctimer_t;

/** 
 * \function hpctimer_timer_create
 * \breif initialize timer
 *
 * \param type type of timer
 * \param flags flags for timer
 * \return pointer to hpctimer_t
 */
hpctimer_t *hpctimer_timer_create(hpctimer_type_t type, uint32_t flags);

/**
 * \function hpctimer_timer_free
 * \breif is free descriptor of timer
 * 
 * \param timer discriptor of timer
 */
void hpctimer_timer_free(hpctimer_t *timer);

/**
 * \function hpctimer_timer_wtime
 * \breif is return time from target timer
 *
 * \param timer discriptor of timer
 * \return time in s.ms format
 */
double hpctimer_timer_wtime(hpctimer_t *timer);

/**
 * \function hpctimer_timer_get_overhead
 * \breif is return overhead ticks
 *
 * \param timer discriptor of timer
 * \return overhead time in ticks
 */
uint64_t hpctimer_timer_get_overhead(hpctimer_t *timer);

/**
 * \function hpctimer_wtime
 * \breif is return time standart timer
 *
 * \return time in s.ms format
 */
double hpctimer_wtime();

#ifdef __cplusplus
}
#endif    

#endif // HPCTIMER_H
