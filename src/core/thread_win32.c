/**
 * HUI - core/thread_win32.c
 * Win32 implementation of core/thread.h (SRW locks, condition variables).
 */

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <process.h>

#include "thread.h"

typedef struct hui_thread_start_t {
	hui_thread_fn function;
	void* arg;
} hui_thread_start_t;

static unsigned __stdcall hui_thread_entry(void* param) {
	hui_thread_start_t start = *(hui_thread_start_t*)param;
	hui_free(param);
	start.function(start.arg);
	return 0;
}

int hui_thread_create(hui_thread_t* thread, hui_thread_fn function, void* arg, const char* name) {
	hui_thread_start_t* start = (hui_thread_start_t*)hui_calloc(sizeof(hui_thread_start_t));
	uintptr_t handle;
	HUI_UNUSED(name);
	start->function = function;
	start->arg = arg;
	handle = _beginthreadex(NULL, 0, hui_thread_entry, start, 0, NULL);
	if (!handle) {
		hui_free(start);
		return HUI_FALSE;
	}
	thread->handle = (void*)handle;
	return HUI_TRUE;
}

void hui_thread_join(hui_thread_t* thread) {
	if (!thread->handle) return;
	WaitForSingleObject((HANDLE)thread->handle, INFINITE);
	CloseHandle((HANDLE)thread->handle);
	thread->handle = NULL;
}

uint64_t hui_thread_id(void) {
	return (uint64_t)GetCurrentThreadId();
}

int hui_cpu_count(void) {
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return info.dwNumberOfProcessors > 0 ? (int)info.dwNumberOfProcessors : 1;
}

void hui_sleep_ms(int milliseconds) {
	if (milliseconds > 0) Sleep((DWORD)milliseconds);
}

double hui_clock_seconds(void) {
	static LARGE_INTEGER frequency;
	LARGE_INTEGER counter;
	if (frequency.QuadPart == 0) QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&counter);
	return (double)counter.QuadPart / (double)frequency.QuadPart;
}

void hui_mutex_init(hui_mutex_t* mutex) { InitializeSRWLock((PSRWLOCK)&mutex->handle); }
void hui_mutex_destroy(hui_mutex_t* mutex) { HUI_UNUSED(mutex); }
void hui_mutex_lock(hui_mutex_t* mutex) { AcquireSRWLockExclusive((PSRWLOCK)&mutex->handle); }
void hui_mutex_unlock(hui_mutex_t* mutex) { ReleaseSRWLockExclusive((PSRWLOCK)&mutex->handle); }

void hui_rwlock_init(hui_rwlock_t* lock) { InitializeSRWLock((PSRWLOCK)&lock->handle); }
void hui_rwlock_destroy(hui_rwlock_t* lock) { HUI_UNUSED(lock); }
void hui_rwlock_read_lock(hui_rwlock_t* lock) { AcquireSRWLockShared((PSRWLOCK)&lock->handle); }
void hui_rwlock_read_unlock(hui_rwlock_t* lock) { ReleaseSRWLockShared((PSRWLOCK)&lock->handle); }
void hui_rwlock_write_lock(hui_rwlock_t* lock) { AcquireSRWLockExclusive((PSRWLOCK)&lock->handle); }
void hui_rwlock_write_unlock(hui_rwlock_t* lock) { ReleaseSRWLockExclusive((PSRWLOCK)&lock->handle); }

void hui_cond_init(hui_cond_t* cond) { InitializeConditionVariable((PCONDITION_VARIABLE)&cond->handle); }
void hui_cond_destroy(hui_cond_t* cond) { HUI_UNUSED(cond); }

void hui_cond_wait(hui_cond_t* cond, hui_mutex_t* mutex) {
	SleepConditionVariableSRW((PCONDITION_VARIABLE)&cond->handle, (PSRWLOCK)&mutex->handle, INFINITE, 0);
}

int hui_cond_wait_timeout(hui_cond_t* cond, hui_mutex_t* mutex, double seconds) {
	DWORD milliseconds = seconds <= 0.0 ? 0 : (DWORD)(seconds * 1000.0 + 0.5);
	return SleepConditionVariableSRW((PCONDITION_VARIABLE)&cond->handle, (PSRWLOCK)&mutex->handle, milliseconds, 0) != 0;
}

void hui_cond_signal(hui_cond_t* cond) { WakeConditionVariable((PCONDITION_VARIABLE)&cond->handle); }
void hui_cond_broadcast(hui_cond_t* cond) { WakeAllConditionVariable((PCONDITION_VARIABLE)&cond->handle); }

#else
typedef int hui_thread_win32_unused_t;
#endif /* _WIN32 */
