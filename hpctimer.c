/*
 * hpctimer.c: high-resolution timers library.
 */

#include <sys/time.h>
#include <time.h>

#include <stdlib.h>

#include "hpctimer.h"

static const int usec = 1000000;
static uint32_t global_freq = 0; /* if use bind to cpu */

static void hpctimer_gettimeofday_init(hpctimer_t *timer);
static void hpctimer_tsctimer_init(hpctimer_t *timer);
static void hpctimer_mpiwtime_init(hpctimer_t *timer);
static void hpctimer_clockgettime_init(hpctimer_t *timer);
static uint64_t hpctimer_calc_freq();
static __inline__ uint64_t hpctimer_gettimeofday();
static __inline__ uint64_t hpctimer_gettsc();
static __inline__ uint64_t hpctimer_getmpiwtime();
static __inline__ uint64_t hpctimer_getclockgettime();

hpctimer_t *hpctimer_init(hpctimer_type_t type, uint32_t flags)
{
    hpctimer_t *timer;

    if ((timer = malloc((hpctimer_t *)sizeof(hpctimer_t))) == NULL)
        return NULL;

    timer->flags = flags;
    if (timer->flags & HPCTIMER_BINDTOCPU) {
        /* cpuaffinity */
        global_freq = hpctimer_calc_freq();    
    }

    timer->type = type;
    if (timer->type == HPCTIMER_GETTIMEOFDAY) {
        hpctimer_gettimeofday_init(timer);
        timer->gettime = hpctimer_gettimeofday;
    } else if (timer->type == HPCTIMER_TSC) {
        hpctimer_tsctimer_init(timer);
        timer->gettime = hpctimer_gettsc;
    } else if (timer->type == HPCTIMER_MPIWTIME) {
        hpctimer_mpiwtime_init(timer);
        timer->gettime = hpctimer_getmpiwtime;
    } else if (timer->type == HPCTIMER_CLOCKGETTIME) {
        hpctimer_clockgettime_init(timer);
        timer->gettime = hpctimer_getclockgettime;
    } else {
        free(timer);
        return NULL;
    }

    return timer;
}

void hpctimer_free(hpctimer_t *timer) 
{
    if (timer)
        free(timer);
}

double hpctimer_wtime(hpctimer_t *timer) 
{
    /* convert to seconds */
    return (double) timer->gettime() / timer->freq / usec;
}

uint64_t hpctimer_get_overhead(hpctimer_t *timer) {
    return timer->overhead;
}

static uint64_t hpctimer_calc_freq()
{
    uint64_t start, stop, overhead;
    struct timeval tv1, tv2;
    int i, j;
    
    start = hpctimer_gettsc();
    stop = hpctimer_gettsc();
    overhead = stop - start;

    start = hpctimer_gettsc();
    stop = hpctimer_gettsc();
    overhead += stop - start;

    start = hpctimer_gettsc();
    stop = hpctimer_gettsc();
    overhead += stop - start;

    overhead /= 3;
    overhead = overhead > 0 ? overhead : 0;

    for (i = 0; i < 3; i++) {
        start = hpctimer_gettsc();
        gettimeofday(&tv1, NULL);
        for (j = 0; j < 10000; j++)
            __asm__ __volatile__ ("nop");
        stop = hpctimer_gettsc();
        gettimeofday(&tv2, NULL);    
    }

    /* return cpu frequence in ticks*/
    return (stop - start - overhead) /
           (tv2.tv_sec * usec + tv2.tv_usec - 
            tv1.tv_sec * usec - tv1.tv_usec);
}

static void hpctimer_gettimeofday_init(hpctimer_t *timer) 
{
    uint64_t freq, start, stop, overhead;

    if (global_freq && timer->flags & HPCTIMER_BINDTOCPU)
        freq = global_freq;
    else
        freq = hpctimer_calc_freq();

    start = hpctimer_gettimeofday();
    stop = hpctimer_gettimeofday();
    overhead = (stop - start) * freq;
     
    start = hpctimer_gettimeofday();
    stop = hpctimer_gettimeofday();
    overhead += ((stop - start) * freq);

    start = hpctimer_gettimeofday();
    stop = hpctimer_gettimeofday();
    overhead += ((stop - start) * freq);

    overhead /= 3;
    timer->overhead = overhead > 0 ? overhead : 0;
    timer->freq = 1;
}

static void hpctimer_tsctimer_init(hpctimer_t *timer)
{
    uint64_t start, stop, overhead;

    start = hpctimer_gettsc();
    stop = hpctimer_gettsc();
    overhead = stop - start;

    start = hpctimer_gettsc();
    stop = hpctimer_gettsc();
    overhead += stop - start;

    start = hpctimer_gettsc();
    stop = hpctimer_gettsc();
    overhead += stop - start;

    overhead /= 3;
    timer->overhead = overhead > 0 ? overhead : 0;

    if (global_freq)
        timer->freq = global_freq;
    else 
        timer->freq = hpctimer_calc_freq();
/*
    timer->freq = (stop - start - timer->overhead) /
                  (tv2.tv_sec * usec + tv2.tv_usec - 
                   tv1.tv_sec * usec - tv1.tv_usec);*/
}

static void hpctimer_mpiwtime_init(hpctimer_t *timer)
{
    /* calculate overhead for MPI_Wtime() and freq */
}

static void hpctimer_clockgettime_init(hpctimer_t *timer)
{
    uint64_t start, stop, overhead, freq;
    
    if (global_freq)
        freq = global_freq;
    else
        freq = hpctimer_calc_freq();
    
    start = hpctimer_getclockgettime();
    stop = hpctimer_getclockgettime();
    overhead = (stop - start) * freq;
     
    start = hpctimer_getclockgettime();
    stop = hpctimer_getclockgettime();
    overhead += ((stop - start) * freq);

    start = hpctimer_getclockgettime();
    stop = hpctimer_getclockgettime();
    overhead += ((stop - start) * freq);

    overhead /= 3;
    timer->overhead = overhead > 0 ? overhead : 0;

    timer->freq = 1;
}

static __inline__ uint64_t hpctimer_gettimeofday()
{
    struct timeval tv;
    time_t curtime;

    gettimeofday(&tv, NULL);
    curtime = tv.tv_sec * usec + tv.tv_usec;

    return curtime;
}

static __inline__ uint64_t hpctimer_gettsc()
{
	uint32_t low, high;
	
	__asm__ __volatile__(
		"xorl %%eax, %%eax\n"
		"cpuid\n"
        "rdtsc\n"
		: "=a" (low), "=d" (high)
	);
			
	return ((uint64_t)high << 32) | low;
}

static __inline__ uint64_t hpctimer_getmpiwtime()
{
    /* return MPI_Wtime() */
}

static __inline__ uint64_t hpctimer_getclockgettime()
{
#if (_POSIX_C_SOURCE >= 199309L)
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time); /* only CLOCK_MONOTONIC */

    return time.tv_sec * usec + time.tv_nsec / 1000;    
#else
    return 0;
#endif
}
