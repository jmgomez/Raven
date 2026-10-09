/**
 * HUI - core/thread.h
 * Portable threads, locks, condition variables, atomics and clock.
 * Implemented by thread_posix.c and thread_win32.c.
 */

#ifndef HUI_THREAD_H
#define HUI_THREAD_H

#include "base.h"

#if defined(_WIN32)
	/* SRWLOCK and CONDITION_VARIABLE are both a single pointer: keeping them
	 * opaque avoids including <windows.h> from every source file. */
	typedef struct hui_mutex_t { void* handle; } hui_mutex_t;
	typedef struct hui_rwlock_t { void* handle; } hui_rwlock_t;
	typedef struct hui_cond_t { void* handle; } hui_cond_t;
	typedef struct hui_thread_t { void* handle; } hui_thread_t;
#else
	#include <pthread.h>
	typedef struct hui_mutex_t { pthread_mutex_t handle; } hui_mutex_t;
	typedef struct hui_rwlock_t { pthread_rwlock_t handle; } hui_rwlock_t;
	typedef struct hui_cond_t { pthread_cond_t handle; } hui_cond_t;
	typedef struct hui_thread_t { pthread_t handle; } hui_thread_t;
#endif

typedef void (*hui_thread_fn)(void* arg);

int      hui_thread_create(hui_thread_t* thread, hui_thread_fn function, void* arg, const char* name);
void     hui_thread_join(hui_thread_t* thread);
/** Identifier of the calling thread, never 0. */
uint64_t hui_thread_id(void);
int      hui_cpu_count(void);
void     hui_sleep_ms(int milliseconds);
/** Monotonic clock, in seconds. */
double   hui_clock_seconds(void);

void hui_mutex_init(hui_mutex_t* mutex);
void hui_mutex_destroy(hui_mutex_t* mutex);
void hui_mutex_lock(hui_mutex_t* mutex);
void hui_mutex_unlock(hui_mutex_t* mutex);

void hui_rwlock_init(hui_rwlock_t* lock);
void hui_rwlock_destroy(hui_rwlock_t* lock);
void hui_rwlock_read_lock(hui_rwlock_t* lock);
void hui_rwlock_read_unlock(hui_rwlock_t* lock);
void hui_rwlock_write_lock(hui_rwlock_t* lock);
void hui_rwlock_write_unlock(hui_rwlock_t* lock);

void hui_cond_init(hui_cond_t* cond);
void hui_cond_destroy(hui_cond_t* cond);
void hui_cond_wait(hui_cond_t* cond, hui_mutex_t* mutex);
/** Returns HUI_FALSE when the wait timed out. */
int  hui_cond_wait_timeout(hui_cond_t* cond, hui_mutex_t* mutex, double seconds);
void hui_cond_signal(hui_cond_t* cond);
void hui_cond_broadcast(hui_cond_t* cond);

/* ---------------------------------------------------------------------------
 *  Atomic 32-bit integer
 * ------------------------------------------------------------------------- */

#if defined(_MSC_VER) && !defined(__clang__)
	#include <intrin.h>
	typedef struct hui_atomic_t { volatile long value; } hui_atomic_t;
	HUI_INLINE int hui_atomic_load(hui_atomic_t* a) { return (int)_InterlockedCompareExchange(&a->value, 0, 0); }
	HUI_INLINE void hui_atomic_store(hui_atomic_t* a, int v) { _InterlockedExchange(&a->value, (long)v); }
	/** Adds and returns the new value. */
	HUI_INLINE int hui_atomic_add(hui_atomic_t* a, int v) { return (int)_InterlockedExchangeAdd(&a->value, (long)v) + v; }
	HUI_INLINE int hui_atomic_exchange(hui_atomic_t* a, int v) { return (int)_InterlockedExchange(&a->value, (long)v); }
	/** Atomic pointer. */
	typedef struct hui_atomic_ptr_t { void* volatile value; } hui_atomic_ptr_t;
	HUI_INLINE void* hui_atomic_ptr_load(hui_atomic_ptr_t* a) { return _InterlockedCompareExchangePointer(&a->value, NULL, NULL); }
	HUI_INLINE void hui_atomic_ptr_store(hui_atomic_ptr_t* a, void* v) { _InterlockedExchangePointer(&a->value, v); }
#else
	#include <stdatomic.h>
	typedef struct hui_atomic_t { atomic_int value; } hui_atomic_t;
	HUI_INLINE int hui_atomic_load(hui_atomic_t* a) { return atomic_load(&a->value); }
	HUI_INLINE void hui_atomic_store(hui_atomic_t* a, int v) { atomic_store(&a->value, v); }
	/** Adds and returns the new value. */
	HUI_INLINE int hui_atomic_add(hui_atomic_t* a, int v) { return atomic_fetch_add(&a->value, v) + v; }
	HUI_INLINE int hui_atomic_exchange(hui_atomic_t* a, int v) { return atomic_exchange(&a->value, v); }
	/** Atomic pointer. */
	typedef struct hui_atomic_ptr_t { _Atomic(void*) value; } hui_atomic_ptr_t;
	HUI_INLINE void* hui_atomic_ptr_load(hui_atomic_ptr_t* a) { return atomic_load(&a->value); }
	HUI_INLINE void hui_atomic_ptr_store(hui_atomic_ptr_t* a, void* v) { atomic_store(&a->value, v); }
#endif

#endif /* HUI_THREAD_H */
