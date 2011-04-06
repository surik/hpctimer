/*
 * hpctimer.c: high-resolution timers library.
 */

#include <sys/time.h>
#include <time.h>

#include <stdlib.h>

#include "hpctimer.h"

static const int usec = 1000000;

static void hpctimer_gettimeofday_init(hpctimer_t *timer);
static void hpctimer_tsctimer_init(hpctimer_t *timer);
static __inline__ uint64_t hpctimer_gettimeofday();
static __inline__ uint64_t hpctimer_gettsc();

hpctimer_t *hpctimer_init(hpctimer_type_t type)
{
    hpctimer_t *timer;

    if ((timer = malloc((hpctimer_t *)sizeof(hpctimer_t))) == NULL)
        return NULL;

    timer->type = type;
    if (timer->type == HPCTIMER_GETTIMEOFDAY) {
        hpctimer_gettimeofday_init(timer);
        timer->gettime = hpctimer_gettimeofday;
    } else if (timer->type == HPCTIMER_TSC) {
        hpctimer_tsctimer_init(timer);
        timer->gettime = hpctimer_gettsc;
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

static void hpctimer_gettimeofday_init(hpctimer_t *timer)
{
    uint64_t start, stop, freq;
    double overhead;
    struct timeval tv1, tv2;
    int i, j;

    for (i = 0; i < 3; i++) {
        start = hpctimer_gettsc();
        gettimeofday(&tv1, NULL);
        for (j = 0; j < 10000; j++)
            __asm__ __volatile__ ("nop");
        stop = hpctimer_gettsc();
        gettimeofday(&tv2, NULL);    
    }

    freq = (stop - start - timer->overhead) /
           (tv2.tv_sec * usec + tv2.tv_usec - tv1.tv_sec * usec - tv1.tv_usec);

    start = hpctimer_gettimeofday();
    stop = hpctimer_gettimeofday();
    overhead = (stop - start) * freq;
     
    start = hpctimer_gettimeofday();
    stop = hpctimer_gettimeofday();
    overhead = (stop - start) * freq;

    start = hpctimer_gettimeofday();
    stop = hpctimer_gettimeofday();
    overhead = (stop - start) * freq;
    printf("%.20f\n", overhead);

    timer->overhead = overhead > 0 ? overhead : 0;
    timer->freq = 1;
}

static void hpctimer_tsctimer_init(hpctimer_t *timer)
{
    uint64_t start, stop, overhead;
    struct timeval tv1, tv2;
    int i, j;

    start = hpctimer_gettsc();
    stop = hpctimer_gettsc();
    overhead = stop - start;

    start = hpctimer_gettsc();
    stop = hpctimer_gettsc();
    overhead = stop - start;

    start = hpctimer_gettsc();
    stop = hpctimer_gettsc();
    overhead = stop - start;

    timer->overhead = overhead > 0 ? overhead : 0;

    for (i = 0; i < 3; i++) {
        start = hpctimer_gettsc();
        gettimeofday(&tv1, NULL);
        for (j = 0; j < 10000; j++)
            __asm__ __volatile__ ("nop");
        stop = hpctimer_gettsc();
        gettimeofday(&tv2, NULL);    
    }

    timer->freq = (stop - start - timer->overhead) /
                  (tv2.tv_sec * usec + tv2.tv_usec - 
                   tv1.tv_sec * usec - tv1.tv_usec);

    printf("freq = %Ld Hz\n", timer->freq);
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
