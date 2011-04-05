#include <sys/time.h>
#include <time.h>

#include <stdlib.h>

#include "hpctimer.h"

static unsigned long long hpctimer_gettimeofday();
static unsigned long long hpctimer_gettsc();

hpctimer_t *hpctimer_init(hpctimer_type_t type)
{
    hpctimer_t *t;

    if ((t = malloc((hpctimer_t *)sizeof(hpctimer_t))) == NULL)
        return NULL;

    t->type = type;
    if (t->type == HPCTIMER_GETTIMEOFDAY)
        t->gettime = hpctimer_gettimeofday;
    else if (t->type == HPCTIMER_TSC)
        t->gettime = hpctimer_gettsc;
    else {
        free(t);
        return NULL;
    }

    return t;
}

void hpctimer_free(hpctimer_t *timer) 
{
    if (timer)
        free(timer);
}

double hpctimer_wtime(hpctimer_t *timer) 
{
    return timer->gettime() * 0.000001; /* convert to seconds */
}

static unsigned long long hpctimer_gettimeofday()
{
    struct timeval tv;
    time_t curtime;

    gettimeofday(&tv, NULL);
    curtime = tv.tv_sec * 1000000 + tv.tv_usec;

    return curtime;
}

static unsigned long long hpctimer_gettsc()
{
    unsigned long long low, high;

    __asm__ volatile(
        "rdtsc\n"
        "mov %%eax, %0\n"
        "mov %%edx, %1\n" 
        : "=m" (low), "=m" (high)
    );
    
    /* returns the number of ticks, needed to divide on frequerence cpu */
    return (unsigned long long) high << 32 | low;
}
