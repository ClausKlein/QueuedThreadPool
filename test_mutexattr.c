/*
 * trylock.c
 *
 * Demonstrate a simple use of pthread_mutex_trylock. The
 * counter_thread updates a shared counter at intervals, and a
 * monitor_thread occasionally reports the current value of the
 * counter -- but only if the mutex is not already locked by
 * counter_thread.
 */

#ifdef __linux__
#    include <features.h>
#    ifndef _POSIX_C_SOURCE
#        error "_POSIX_C_SOURCE not defined"
#    endif
#endif

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // _POSIX_MONOTONIC_CLOCK _POSIX_TIMEOUTS _POSIX_TIMERS _POSIX_THREADS ...

#ifdef __APPLE__
#    ifndef __DARWIN_C_LEVEL
#        error "__DARWIN_C_LEVEL not defined!"
#    endif
#endif

#define SPIN 1000

/*
 * Define a macro that can be used for diagnostic output from
 * examples. When compiled -DDEBUG, it results in calling printf
 * with the specified argument list. When DEBUG is not defined, it
 * expands to nothing.
 */
#ifndef NDEBUG
#    define DPRINTF(arg) printf arg
#else
#    define DPRINTF(arg)
#endif

/*
 * NOTE: the "do {" ... "} while (0);" bracketing around the macros
 * allows the err_abort and errno_abort macros to be used as if they
 * were function calls, even in contexts where a trailing ";" would
 * generate a null statement. For example,
 *
 *      if (status != 0)
 *          err_abort(status, "message");
 *      else
 *          return status;
 *
 * will not compile if err_abort is a macro ending with "}", because
 * C does not expect a ";" to follow the "}". Because C does expect
 * a ";" following the ")" in the do...while construct, err_abort and
 * errno_abort can be used as if they were function calls.
 */
#define err_abort(code, text) \
    do { \
        fprintf(stderr, "%s at \"%s\":%d: %s\n", text, __FILE__, __LINE__, \
            strerror(code)); \
        abort(); \
    } while (0)
#define errno_abort(text) \
    do { \
        fprintf(stderr, "%s at \"%s\":%d: %s\n", text, __FILE__, __LINE__, \
            strerror(errno)); \
        abort(); \
    } while (0)

//
// NOTE: static initializer not used (non portable)! CK
//
// pthread_mutex_t mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
// pthread_mutex_t mutex = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;

pthread_mutex_t mutex;
pthread_mutexattr_t attr;

static volatile long counter = 0;
static time_t end_time       = 0;
static int recursive         = 0; /* Whether to use use the mutex recursive */

/*
 * Thread start routine that repeatedly locks a mutex and
 * increments a counter.
 */
void* counter_thread(void* arg)
{
    int status;
    int spin;

    /*
     * Until end_time, increment the counter each
     * second. Instead of just incrementing the counter, it
     * sleeps for another second with the mutex locked, to give
     * monitor_thread a reasonable chance of running.
     */
    while (time(NULL) < end_time) {

        //#ifdef PTHREAD_MUTEX_RECURSIVE
        //#    warning "recursive mutex supported"
        //#endif
        if (recursive) {
            printf("recursive usage:\n");
            status = pthread_mutex_lock(&mutex);
            if (status != 0) {
                err_abort(status, "Lock mutex");
            }
        }

        status = pthread_mutex_lock(&mutex);
        if (status != 0) {
            DPRINTF(("Lock mutex failed! %s\n", strerror(status)));
            if (status != EDEADLK) {
                err_abort(status, "Lock mutex");
            }
        }

        for (spin = 0; spin < SPIN; spin++) {
            counter++;
        }

        status = pthread_mutex_unlock(&mutex);
        if (status != 0) {
            err_abort(status, "Unlock mutex");
        }

        if (recursive) {
            status = pthread_mutex_unlock(&mutex);
            if (status != 0) {
                DPRINTF(("Unlock mutex failed! %s\n", strerror(status)));
                if (status != EPERM) {
                    err_abort(status, "Unlock mutex");
                }
            }
        }

        usleep(SPIN);
    }

    printf("Counter Thread finished, counter is now %ld\n", counter / SPIN);
    return NULL;
}

/*
 * Thread start routine to "monitor" the counter. Every 1
 * seconds, try to lock the mutex and read the counter. If the
 * trylock fails, skip this cycle.
 */
void* monitor_thread(void* arg)
{
    int status;
    int misses = 0;

    /*
     * Loop until end_time, checking the counter every 1
     * seconds.
     */
    while (time(NULL) < end_time) {
        usleep(SPIN);
        status = pthread_mutex_trylock(&mutex);
        if (status != EBUSY) {
            if (status != 0) {
                err_abort(status, "Trylock mutex");
            }
            printf("Counter is %ld\n", counter / SPIN);
            status = pthread_mutex_unlock(&mutex);
            if (status != 0) {
                err_abort(status, "Unlock mutex");
            }
        } else {
            misses++; /* Count "misses" on the lock */
        }
    }
    printf("Monitor thread missed update %d times.\n", misses);
    return NULL;
}

int main(int argc, char* argv[])
{
    int status;
    pthread_t counter_thread_id;
    pthread_t monitor_thread_id;

    /*
     * If the first argument is absent, or zero, the program try
     * to avoid a deadlock. If the first argument is non zero,
     * the program will deadlock on a recursive lock
     */
    if (argc > 1) {
        recursive = atoi(argv[1]);
    }

    pthread_mutexattr_init(&attr);

    if (recursive > 0) {
        printf("errorcheck mutex is used!\n");
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
    } else if (recursive < 0) {
        printf("recursive mutex is used!\n");
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    } else {
        printf("normal mutex is used!\n");
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
    }

    memset(&mutex, 0, sizeof(mutex));
    pthread_mutex_init(&mutex, &attr);
    pthread_mutexattr_destroy(&attr);

    end_time = time(NULL) + 1; /* Run for 3 sec */
    status   = pthread_create(&counter_thread_id, NULL, counter_thread, NULL);
    if (status != 0) {
        err_abort(status, "Create counter thread");
    }
    status = pthread_create(&monitor_thread_id, NULL, monitor_thread, NULL);
    if (status != 0) {
        err_abort(status, "Create monitor thread");
    }

    status = pthread_join(counter_thread_id, NULL);
    if (status != 0) {
        err_abort(status, "Join counter thread");
    }
    status = pthread_join(monitor_thread_id, NULL);
    if (status != 0) {
        err_abort(status, "Join monitor thread");
    }
    return 0;
}
