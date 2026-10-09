/**
 * HUI - core/thread_posix.c
 * pthread implementation of core/thread.h (Linux, BSD, macOS).
 */

#if !defined(_WIN32)

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* pthread_setname_np */
#endif

#include "thread.h"

#include <errno.h>
#include <time.h>
#include <unistd.h>

typedef struct hui_thread_start_t {
	hui_thread_fn function;
	void* arg;
	char name[16];
} hui_thread_start_t;

static void* hui_thread_entry(void* param) {
	hui_thread_start_t start = *(hui_thread_start_t*)param;
	hui_free(param);
#if defined(__linux__)
	if (start.name[0]) pthread_setname_np(pthread_self(), start.name);
#elif defined(__APPLE__)
	if (start.name[0]) pthread_setname_np(start.name);
#endif
	start.function(start.arg);
	return NULL;
}

int hui_thread_create(hui_thread_t* thread, hui_thread_fn function, void* arg, const char* name) {
	hui_thread_start_t* start = (hui_thread_start_t*)hui_calloc(sizeof(hui_thread_start_t));
	start->function = function;
	start->arg = arg;
	if (name) {
		strncpy(start->name, name, sizeof(start->name) - 1);
	}
	if (pthread_create(&thread->handle, NULL, hui_thread_entry, start) != 0) {
		hui_free(start);
		return HUI_FALSE;
	}
	return HUI_TRUE;
}

void hui_thread_join(hui_thread_t* thread) {
	pthread_join(thread->handle, NULL);
}

uint64_t hui_thread_id(void) {
	/* the address of a thread-local object is unique per live thread */
	static _Thread_local char marker;
	return (uint64_t)(uintptr_t)&marker;
}

int hui_cpu_count(void) {
	long count = sysconf(_SC_NPROCESSORS_ONLN);
	return count > 0 ? (int)count : 1;
}

void hui_sleep_ms(int milliseconds) {
	struct timespec ts;
	if (milliseconds <= 0) return;
	ts.tv_sec = milliseconds / 1000;
	ts.tv_nsec = (long)(milliseconds % 1000) * 1000000L;
	while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {}
}

double hui_clock_seconds(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

void hui_mutex_init(hui_mutex_t* mutex) { pthread_mutex_init(&mutex->handle, NULL); }
void hui_mutex_destroy(hui_mutex_t* mutex) { pthread_mutex_destroy(&mutex->handle); }
void hui_mutex_lock(hui_mutex_t* mutex) { pthread_mutex_lock(&mutex->handle); }
void hui_mutex_unlock(hui_mutex_t* mutex) { pthread_mutex_unlock(&mutex->handle); }

void hui_rwlock_init(hui_rwlock_t* lock) { pthread_rwlock_init(&lock->handle, NULL); }
void hui_rwlock_destroy(hui_rwlock_t* lock) { pthread_rwlock_destroy(&lock->handle); }
void hui_rwlock_read_lock(hui_rwlock_t* lock) { pthread_rwlock_rdlock(&lock->handle); }
void hui_rwlock_read_unlock(hui_rwlock_t* lock) { pthread_rwlock_unlock(&lock->handle); }
void hui_rwlock_write_lock(hui_rwlock_t* lock) { pthread_rwlock_wrlock(&lock->handle); }
void hui_rwlock_write_unlock(hui_rwlock_t* lock) { pthread_rwlock_unlock(&lock->handle); }

/* macOS has no pthread_condattr_setclock: timed waits use the realtime clock there */
#if defined(__APPLE__)
	#define HUI_COND_CLOCK CLOCK_REALTIME
#else
	#define HUI_COND_CLOCK CLOCK_MONOTONIC
#endif

void hui_cond_init(hui_cond_t* cond) {
#if defined(__APPLE__)
	pthread_cond_init(&cond->handle, NULL);
#else
	pthread_condattr_t attr;
	pthread_condattr_init(&attr);
	pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
	pthread_cond_init(&cond->handle, &attr);
	pthread_condattr_destroy(&attr);
#endif
}

void hui_cond_destroy(hui_cond_t* cond) { pthread_cond_destroy(&cond->handle); }
void hui_cond_wait(hui_cond_t* cond, hui_mutex_t* mutex) { pthread_cond_wait(&cond->handle, &mutex->handle); }

int hui_cond_wait_timeout(hui_cond_t* cond, hui_mutex_t* mutex, double seconds) {
	struct timespec ts;
	double whole;
	if (seconds < 0.0) seconds = 0.0;
	clock_gettime(HUI_COND_CLOCK, &ts);
	whole = floor(seconds);
	ts.tv_sec += (time_t)whole;
	ts.tv_nsec += (long)((seconds - whole) * 1e9);
	if (ts.tv_nsec >= 1000000000L) {
		ts.tv_sec += 1;
		ts.tv_nsec -= 1000000000L;
	}
	return pthread_cond_timedwait(&cond->handle, &mutex->handle, &ts) != ETIMEDOUT;
}

void hui_cond_signal(hui_cond_t* cond) { pthread_cond_signal(&cond->handle); }
void hui_cond_broadcast(hui_cond_t* cond) { pthread_cond_broadcast(&cond->handle); }

#else
typedef int hui_thread_posix_unused_t;
#endif /* !_WIN32 */
