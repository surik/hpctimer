/*
 * hpctimer.c: High-resolution timers library.
 */

/** \mainpage High-Resolution timers library
 * \section Timers
 * - rdtsc
 * - gettimeofday
 * - clock_gettime
 * \section Builds
 * $ make
 * \section Clean
 * $make clean
 */

#ifndef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>

#include <sys/time.h>
#include <time.h>
#if defined(__gnu_linux__) || defined(linux)
#define __USE_GNU
#define _GNU_SOURCE
#include <sched.h>
#include <utmpx.h>
#endif

#include "hpctimer.h"

/** \privatesection */
struct hpctimer {
    hpctimer_type_t type;
    uint32_t flags;
    uint64_t overhead;      /* in ticks */
    uint64_t freq           /* in MHz */;
    uint64_t (*gettime) ();
#if defined(__gnu_linux__) || defined(linux)
    cpu_set_t cpuset;       /* for bind to CPU */ 
#endif
};

static const int usec = 1000000;
static uint32_t global_freq = 0; /* if use bind to cpu */

static void hpctimer_time_gettimeofday_init(hpctimer_t *timer);
static void hpctimer_tsctimer_init(hpctimer_t *timer);
static void hpctimer_mpiwtime_init(hpctimer_t *timer);
static void hpctimer_clockgettime_init(hpctimer_t *timer);
static uint64_t hpctimer_calc_freq();
static int hpctimer_set_cpuaffinity(hpctimer_t *timer);
static int hpctimer_restore_cpuaffinity(hpctimer_t *timer);
static __inline__ uint64_t hpctimer_time_gettimeofday();
static __inline__ uint64_t hpctimer_time_tsc();
static __inline__ uint64_t hpctimer_time_mpiwtime();
static __inline__ uint64_t hpctimer_time_clockgettime();

hpctimer_t *hpctimer_timer_create(hpctimer_type_t type, uint32_t flags)
{
    hpctimer_t *timer;

    if ((timer = malloc((hpctimer_t *)sizeof(hpctimer_t))) == NULL)
        return NULL;

    timer->flags = flags;
    if (timer->flags & HPCTIMER_BINDTOCPU) {
        hpctimer_set_cpuaffinity(timer);
        global_freq = hpctimer_calc_freq();    
    }
    timer->type = type;
    if (timer->type == HPCTIMER_GETTIMEOFDAY) {
        hpctimer_time_gettimeofday_init(timer);
        timer->gettime = hpctimer_time_gettimeofday;
    } else if (timer->type == HPCTIMER_TSC) {
        hpctimer_tsctimer_init(timer);
        timer->gettime = hpctimer_time_tsc;
    } else if (timer->type == HPCTIMER_MPIWTIME) {
        hpctimer_mpiwtime_init(timer);
        timer->gettime = hpctimer_time_mpiwtime;
    } else if (timer->type == HPCTIMER_CLOCKGETTIME) {
        hpctimer_clockgettime_init(timer);
        timer->gettime = hpctimer_time_clockgettime;
    } else {
        free(timer);
        return NULL;
    }

    return timer;
}

void hpctimer_timer_free(hpctimer_t *timer) 
{
    if (timer->flags & HPCTIMER_BINDTOCPU)
        hpctimer_restore_cpuaffinity(timer);

    if (timer)
        free(timer);
}

double hpctimer_timer_wtime(hpctimer_t *timer) 
{
    /* convert to seconds */
    return (double) timer->gettime() / timer->freq / usec;
}

uint64_t hpctimer_timer_get_overhead(hpctimer_t *timer) {
    return timer->overhead;
}

double hpctimer_wtime()
{
    return hpctimer_gettimeofday() / usec;
}

static uint64_t hpctimer_calc_freq()
{
    uint64_t start, stop, overhead;
    struct timeval tv1, tv2;
    int i, j;
   
    gettimeofday(&tv1, NULL);
    gettimeofday(&tv2, NULL);
    
    start = hpctimer_time_tsc();
    stop = hpctimer_time_tsc();
    overhead = stop - start;

    start = hpctimer_time_tsc();
    stop = hpctimer_time_tsc();
    overhead += stop - start;

    start = hpctimer_time_tsc();
    stop = hpctimer_time_tsc();
    overhead += stop - start;

    overhead /= 3;
    overhead = overhead > 0 ? overhead : 0;

    for (i = 0; i < 3; i++) {
        start = hpctimer_time_tsc();
        gettimeofday(&tv1, NULL);
        for (j = 0; j < 10000; j++)
            __asm__ __volatile__ ("nop");
        stop = hpctimer_time_tsc();
        gettimeofday(&tv2, NULL);    
    }

   /* return cpu frequence in ticks*/
    return (stop - start - overhead) /
           (tv2.tv_sec * usec + tv2.tv_usec - 
            tv1.tv_sec * usec - tv1.tv_usec);
}

static int hpctimer_set_cpuaffinity(hpctimer_t *timer)
{
#if defined(__gnu_linux__) || defined(linux)
    cpu_set_t newcpuset;
    int cpu;
	
    /* Save current cpu affinity */
    CPU_ZERO(&timer->cpuset);
    if (sched_getaffinity(0, sizeof(cpu_set_t), &timer->cpuset) == -1)
        return 0;
	
    /* Bind process to its current cpu */
    CPU_ZERO(&newcpuset);
    /* sched_getcpu() is available since glibc 2.6 */
    if ((cpu = sched_getcpu() == -1))
        return 0;
    printf("cpu: %d\n", cpu); 
    CPU_SET(cpu, &newcpuset);
    if (sched_setaffinity(0, sizeof(cpu_set_t), &newcpuset) == -1)
        return 0;
	
    return 1;
#else
#error "Unsupported platform"
#endif		
}

static int hpctimer_restore_cpuaffinity(hpctimer_t *timer)
{
#if defined(__gnu_linux__) || defined(linux)
	return (sched_setaffinity(0, sizeof(cpu_set_t), &timer->cpuset) != 1) ?
            1 : 0;
#else
    return 1;
#endif
}

static void hpctimer_time_gettimeofday_init(hpctimer_t *timer) 
{
    uint64_t freq, start, stop, overhead;

    if (global_freq && timer->flags & HPCTIMER_BINDTOCPU)
        freq = global_freq;
    else
        freq = hpctimer_calc_freq();

    start = hpctimer_time_gettimeofday();
    stop = hpctimer_time_gettimeofday();
    overhead = (stop - start) * freq;
     
    start = hpctimer_time_gettimeofday();
    stop = hpctimer_time_gettimeofday();
    overhead += ((stop - start) * freq);

    start = hpctimer_time_gettimeofday();
    stop = hpctimer_time_gettimeofday();
    overhead += ((stop - start) * freq);

    overhead /= 3;
    timer->overhead = overhead > 0 ? overhead : 0;
    timer->freq = 1;
}

static void hpctimer_tsctimer_init(hpctimer_t *timer)
{
    uint64_t start, stop, overhead;
    
    start = hpctimer_time_tsc();
    stop = hpctimer_time_tsc();
    overhead = stop - start;
    
    start = hpctimer_time_tsc();
    stop = hpctimer_time_tsc();
    overhead += stop - start;

    start = hpctimer_time_tsc();
    stop = hpctimer_time_tsc();
    overhead += stop - start;
    
    overhead /= 3;
    timer->overhead = overhead > 0 ? overhead : 0;

    if (global_freq)
        timer->freq = global_freq;
    else 
        timer->freq = hpctimer_calc_freq();
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
    
    start = hpctimer_time_clockgettime();
    stop = hpctimer_time_clockgettime();
    overhead = (stop - start) * freq;
     
    start = hpctimer_time_clockgettime();
    stop = hpctimer_time_clockgettime();
    overhead += ((stop - start) * freq);

    start = hpctimer_time_clockgettime();
    stop = hpctimer_time_clockgettime();
    overhead += ((stop - start) * freq);

    overhead /= 3;
    timer->overhead = overhead > 0 ? overhead : 0;

    timer->freq = 1;
}

static __inline__ uint64_t hpctimer_time_gettimeofday()
{
    struct timeval tv;
    time_t curtime;

    gettimeofday(&tv, NULL);
    curtime = tv.tv_sec * usec + tv.tv_usec;

    return curtime;
}

static __inline__ uint64_t hpctimer_time_tsc()
{
#if defined(__x86_64__)
	uint32_t low, high;
	
	__asm__ __volatile__(
		"xorl %%eax, %%eax\n"
		"cpuid\n"
		:::	"%rax", "%rbx", "%rcx", "%rdx"
	);
	__asm__ __volatile__(
		"rdtsc\n"
		: "=a" (low), "=d" (high)
	);
			
	return ((uint64_t)high << 32) | low;
#elif defined(__i386__)
	uint64_t val;
	
	__asm__ __volatile__(
		"xorl %%eax, %%eax\n"
		"cpuid\n"
		:::	"%eax", "%ebx", "%ecx", "%edx"
	);
	__asm__ __volatile__(
		"rdtsc\n"
        "xorl %%ecx, %%ecx\n"
		: "=A" (val)
	);
		
	return val;
#else
#	error "Unsupported platform"
#endif
}

static __inline__ uint64_t hpctimer_time_mpiwtime()
{
    /* return MPI_Wtime() */
}

static __inline__ uint64_t hpctimer_time_clockgettime()
{
#if (_POSIX_C_SOURCE >= 199309L)
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time); /* only CLOCK_MONOTONIC */

    return time.tv_sec * usec + time.tv_nsec / 1000;    
#else
    return 0;
#endif
}
