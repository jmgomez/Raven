/**
 * HUI - core/jobs.h
 * Job system: a fixed pool of worker threads fed by one shared queue.
 *
 * A thread waiting for a group of jobs executes queued jobs itself instead of
 * sleeping, so jobs may submit and wait for other jobs without deadlock and
 * the pool also works with zero workers (everything then runs in the waiter).
 */

#ifndef HUI_JOBS_H
#define HUI_JOBS_H

#include "thread.h"

typedef void (*hui_job_fn)(void* arg);
typedef void (*hui_job_range_fn)(uint32_t begin, uint32_t end, void* context);

/** Counts the unfinished jobs submitted with it. Zero-initialize before use. */
typedef struct hui_job_group_t {
	hui_atomic_t pending;
} hui_job_group_t;

/** @param worker_count Number of threads to start; 0 runs every job in the caller. */
void hui_jobs_init(int worker_count);
void hui_jobs_shutdown(void);
int  hui_jobs_worker_count(void);

/** Queues a job. `group` may be NULL for fire-and-forget jobs. Thread-safe. */
void hui_jobs_submit(hui_job_group_t* group, hui_job_fn function, void* arg);

/** Returns when every job of the group is finished. Runs queued jobs meanwhile. */
void hui_jobs_wait(hui_job_group_t* group);

/**
 * Calls `function` for consecutive sub-ranges of [0, count), each holding at
 * most `grain` items, on the workers and on the calling thread.
 */
void hui_jobs_parallel_for(uint32_t count, uint32_t grain, hui_job_range_fn function, void* context);

#endif /* HUI_JOBS_H */
