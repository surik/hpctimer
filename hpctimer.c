/*
 * hpctimer.c: High-resolution timers library.
 */

/** \mainpage High-Resolution timers library
 * \section Timers
 * - rdtsc
 * - gettimeofday
 * - clock_gettime
 * \section Install
 * $ ./configure
 * $ make
 * $ sudo make install
 * \section Unistall
 * $ sudo make uninstall
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>

#include <sys/time.h>
#include <time.h>
#ifdef HAVE_SCHED_GETCPU
#define __USE_GNU
#define _GNU_SOURCE
#include <sched.h>
#include <utmpx.h>
#endif

#include "hpctimer.h"

#ifdef HAVE_SCHED_GETCPU
#define DEFAULT_FLAGS HPCTIMER_BINDTOCPU
#else
#define DEFAULT_FLAGS 0
#endif /* HAVE_SCHED_GETCPU*/

/** \privatesection */
struct hpctimer {
    hpctimer_type_t type;
    uint32_t flags;
    uint64_t overhead;      /* in ticks */
    uint64_t freq           /* in MHz */;
    uint64_t (*gettime) ();
#if HAVE_SCHED_GETCPU
    cpu_set_t cpuset;       /* for bind to CPU */ 
#endif
};

static const int usec = 1000000;

/* using Thread-local storage */
static __thread uint32_t global_freq = 0; /* if use bind to cpu */
static __thread hpctimer_t *global_timer = NULL;
static __thread int global_timer_init = 0;

static void hpctimer_time_gettimeofday_init(hpctimer_t *timer);
static void hpctimer_tsctimer_init(hpctimer_t *timer);
static void hpctimer_clockgettime_init(hpctimer_t *timer);
static uint64_t hpctimer_cpufreq_calc();
static int hpctimer_set_cpuaffinity(hpctimer_t *timer);
static int hpctimer_restore_cpuaffinity(hpctimer_t *timer);
static __inline__ uint64_t hpctimer_time_gettimeofday();
static __inline__ uint64_t hpctimer_time_tsc();
static __inline__ uint64_t hpctimer_time_clockgettime();

hpctimer_t *hpctimer_timer_create(hpctimer_type_t type, uint32_t flags)
{
    hpctimer_t *timer;

    if ((timer = malloc((hpctimer_t *)sizeof(hpctimer_t))) == NULL)
        return NULL;

    timer->flags = flags;
#ifdef HAVE_SCHED_GETCPU
    if (timer->flags & HPCTIMER_BINDTOCPU) {
        hpctimer_set_cpuaffinity(timer);
        global_freq = hpctimer_cpufreq_calc();    
    }
#endif /* HAVE_SCHED_GETCPU */
    
    timer->type = type;
    
    if (timer->type == HPCTIMER_GETTIMEOFDAY) {
#ifdef HAVE_GETTIMEOFDAY
        hpctimer_time_gettimeofday_init(timer);
        return timer;
#else
        free(timer);
        return NULL;
#endif
    } 
    if (timer->type == HPCTIMER_TSC) {
#ifdef HAVE_TSC
        hpctimer_tsctimer_init(timer);
        return timer;
#else
        free(timer);
        return NULL;
#endif
    }     
    if (timer->type == HPCTIMER_CLOCKGETTIME) {
#ifdef HAVE_CLOCKGETTIME
        hpctimer_clockgettime_init(timer);
        return timer;
#else
        free(timer);
        return timer;
#endif
    } 
    
    free(timer);
    return NULL;
}

void hpctimer_timer_free(hpctimer_t *timer) 
{
#ifdef HAVE_SCHED_GETCPU
    if (timer->flags & HPCTIMER_BINDTOCPU)
        hpctimer_restore_cpuaffinity(timer);
#endif /* HAVE_SHED_GETCPU */

    if (timer)
        free(timer);
}

double hpctimer_timer_wtime(hpctimer_t *timer) 
{
    /* convert to seconds */
    return (double)timer->gettime() / timer->freq / usec;
}

double hpctimer_timer_get_overhead(hpctimer_t *timer) {
    if (timer->type == HPCTIMER_TSC)
        return (double) timer->overhead / (timer->freq * usec);
    else {
        if (!global_freq) global_freq = hpctimer_cpufreq_calc();
        return (double) timer->overhead / (global_freq * usec);
    }
}

double hpctimer_wtime()
{
    /* choose the best timer */
    if (!global_timer_init) {
        global_timer_init = 1;
#ifdef HAVE_TSC
        global_timer = hpctimer_timer_create(HPCTIMER_TSC, DEFAULT_FLAGS);
#elif defined(HAVE_CLOCKGETTIME)
        global_timer = hpctimer_timer_create(HPCTIMER_CLOCKGETTIME, 
                                             DEFAULT_FLAGS);
#elif defined(HAVE_GETTIMEOFDAY)
        global_timer = hpctimer_timer_create(HPCTIMER_GETTIMEOFDAY, 
                                             DEFAULT_FLAGS);
#endif
    }

    if (global_timer_init)
        return hpctimer_timer_wtime(global_timer);
  
    return -1;
}

double hpctimer_get_overhead()
{
    /* initialization of global timer if need */
    if (!global_timer_init) 
        hpctimer_wtime();

    return hpctimer_timer_get_overhead(global_timer);
}

static uint64_t hpctimer_cpufreq_calc()
{
#if defined(HAVE_TSC) && defined(HAVE_GETTIMEOFDAY)
    uint64_t start, stop, overhead;
    struct timeval tv1, tv2;
    int i, j;
   
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
#else
#if defined(__gnu_linux__) || defined(linux)
    /* read /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq */
    FILE *fd;
    int khz;
        
    fd = fopen("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", "r");
    fscanf(fd, "%d", &khz);
    fclose(fd);
    
    return (uint64_t)khz / 1000;
#else
    return -1;
#endif /* defined(__gnu_linux)) || defined(linux)*/
#endif /* HAVE_TSC && HAVE_GETTIMEOFDAY */
}

#ifdef HAVE_SCHED_GETCPU
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
    
    CPU_SET(cpu, &newcpuset);
    if (sched_setaffinity(0, sizeof(cpu_set_t), &newcpuset) == -1)
        return 0;
	
    return 1;
#else
#   error "Unsupported platform"
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
#endif /* HAVE_SCHED_GETCOU */

#ifdef HAVE_GETTIMEOFDAY
static void hpctimer_time_gettimeofday_init(hpctimer_t *timer) 
{
    uint64_t freq, start, stop, overhead;

    if (global_freq && timer->flags & HPCTIMER_BINDTOCPU)
        freq = global_freq;
    else
        freq = hpctimer_cpufreq_calc();

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
    timer->gettime = hpctimer_time_gettimeofday;
}
#endif /* HAVE_GETTIMEOFDAY */

#ifdef HAVE_TSC
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
    timer->gettime = hpctimer_time_tsc;

    if (global_freq)
        timer->freq = global_freq;
    else 
        timer->freq = hpctimer_cpufreq_calc();
}
#endif /* HAVE_TSC */

#ifdef HAVE_CLOCKGETTIME
static void hpctimer_clockgettime_init(hpctimer_t *timer)
{
    uint64_t start, stop, overhead, freq;
    
    if (global_freq)
        freq = global_freq;
    else
        freq = hpctimer_cpufreq_calc();
    
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
    timer->gettime = hpctimer_time_clockgettime;
}
#endif /* HAVE_CLOCKGETTIME */

#ifdef HAVE_GETTIMEOFDAY
static __inline__ uint64_t hpctimer_time_gettimeofday()
{
    struct timeval tv;
    time_t curtime;

    gettimeofday(&tv, NULL);
    curtime = tv.tv_sec * usec + tv.tv_usec;

    return curtime;
}
#endif /* HAVE_GETTIMEROFDAY */

#ifdef HAVE_TSC
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
#endif /* HAVE_TSC */

#ifdef HAVE_CLOCKGETTIME
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
#endif /* HAVE_CLOCKGETTIME */
