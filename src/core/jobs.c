/**
 * HUI - core/jobs.c
 * Worker pool with a mutex-protected ring queue.
 */

#include "jobs.h"

#define HUI_JOBS_MAX_WORKERS 64

typedef struct hui_job_t {
	hui_job_fn function;
	void* arg;
	hui_job_group_t* group;
} hui_job_t;

typedef struct hui_jobs_state_t {
	hui_mutex_t mutex;
	/** Signalled when a job is queued (workers sleep on it). */
	hui_cond_t work_cv;
	/** Broadcast when a group completes or a job is queued (waiters sleep on it). */
	hui_cond_t done_cv;
	hui_job_t* queue;
	uint32_t queue_cap;
	uint32_t queue_head;
	uint32_t queue_count;
	uint32_t waiters;
	hui_thread_t workers[HUI_JOBS_MAX_WORKERS];
	int worker_count;
	int stop;
	int initialized;
} hui_jobs_state_t;

static hui_jobs_state_t g_jobs;

/* Caller holds the mutex. */
static int hui_jobs_pop(hui_job_t* job) {
	if (g_jobs.queue_count == 0) return HUI_FALSE;
	*job = g_jobs.queue[g_jobs.queue_head];
	g_jobs.queue_head = (g_jobs.queue_head + 1) % g_jobs.queue_cap;
	g_jobs.queue_count--;
	return HUI_TRUE;
}

/* Caller does not hold the mutex. */
static void hui_jobs_execute(const hui_job_t* job) {
	job->function(job->arg);
	if (job->group && hui_atomic_add(&job->group->pending, -1) == 0) {
		/* Taking the mutex orders this broadcast after the waiter's check of
		 * `pending`, so the wake-up cannot be lost. */
		hui_mutex_lock(&g_jobs.mutex);
		hui_cond_broadcast(&g_jobs.done_cv);
		hui_mutex_unlock(&g_jobs.mutex);
	}
}

static void hui_jobs_worker(void* arg) {
	HUI_UNUSED(arg);
	hui_mutex_lock(&g_jobs.mutex);
	for (;;) {
		hui_job_t job;
		if (hui_jobs_pop(&job)) {
			hui_mutex_unlock(&g_jobs.mutex);
			hui_jobs_execute(&job);
			hui_mutex_lock(&g_jobs.mutex);
			continue;
		}
		if (g_jobs.stop) break;
		hui_cond_wait(&g_jobs.work_cv, &g_jobs.mutex);
	}
	hui_mutex_unlock(&g_jobs.mutex);
}

void hui_jobs_init(int worker_count) {
	int i;
	memset(&g_jobs, 0, sizeof(g_jobs));
	hui_mutex_init(&g_jobs.mutex);
	hui_cond_init(&g_jobs.work_cv);
	hui_cond_init(&g_jobs.done_cv);
	g_jobs.queue_cap = 256;
	g_jobs.queue = (hui_job_t*)hui_alloc(sizeof(hui_job_t) * g_jobs.queue_cap);
	g_jobs.initialized = HUI_TRUE;
	worker_count = hui_clampi(worker_count, 0, HUI_JOBS_MAX_WORKERS);
	for (i = 0; i < worker_count; i++) {
		char name[32];
		snprintf(name, sizeof(name), "hui-job-%d", i);
		if (!hui_thread_create(&g_jobs.workers[g_jobs.worker_count], hui_jobs_worker, NULL, name)) break;
		g_jobs.worker_count++;
	}
}

void hui_jobs_shutdown(void) {
	int i;
	hui_job_t job;
	if (!g_jobs.initialized) return;
	hui_mutex_lock(&g_jobs.mutex);
	g_jobs.stop = HUI_TRUE;
	hui_cond_broadcast(&g_jobs.work_cv);
	hui_mutex_unlock(&g_jobs.mutex);
	for (i = 0; i < g_jobs.worker_count; i++) hui_thread_join(&g_jobs.workers[i]);
	/* workers drain the queue before leaving; with zero workers, run what is left */
	hui_mutex_lock(&g_jobs.mutex);
	while (hui_jobs_pop(&job)) {
		hui_mutex_unlock(&g_jobs.mutex);
		hui_jobs_execute(&job);
		hui_mutex_lock(&g_jobs.mutex);
	}
	hui_mutex_unlock(&g_jobs.mutex);
	hui_free(g_jobs.queue);
	hui_cond_destroy(&g_jobs.done_cv);
	hui_cond_destroy(&g_jobs.work_cv);
	hui_mutex_destroy(&g_jobs.mutex);
	memset(&g_jobs, 0, sizeof(g_jobs));
}

int hui_jobs_worker_count(void) {
	return g_jobs.worker_count;
}

void hui_jobs_submit(hui_job_group_t* group, hui_job_fn function, void* arg) {
	hui_job_t job;
	job.function = function;
	job.arg = arg;
	job.group = group;
	if (group) hui_atomic_add(&group->pending, 1);

	if (!g_jobs.initialized || (g_jobs.worker_count == 0 && !group)) {
		/* nobody will ever pick up an unowned job: run it now */
		hui_jobs_execute(&job);
		return;
	}

	hui_mutex_lock(&g_jobs.mutex);
	if (g_jobs.queue_count == g_jobs.queue_cap) {
		/* grow the ring, unwrapping it to the start of the new storage */
		uint32_t new_cap = g_jobs.queue_cap * 2, i;
		hui_job_t* queue = (hui_job_t*)hui_alloc(sizeof(hui_job_t) * new_cap);
		for (i = 0; i < g_jobs.queue_count; i++) {
			queue[i] = g_jobs.queue[(g_jobs.queue_head + i) % g_jobs.queue_cap];
		}
		hui_free(g_jobs.queue);
		g_jobs.queue = queue;
		g_jobs.queue_cap = new_cap;
		g_jobs.queue_head = 0;
	}
	g_jobs.queue[(g_jobs.queue_head + g_jobs.queue_count) % g_jobs.queue_cap] = job;
	g_jobs.queue_count++;
	hui_cond_signal(&g_jobs.work_cv);
	if (g_jobs.waiters) hui_cond_broadcast(&g_jobs.done_cv);
	hui_mutex_unlock(&g_jobs.mutex);
}

void hui_jobs_wait(hui_job_group_t* group) {
	if (!g_jobs.initialized) return;
	hui_mutex_lock(&g_jobs.mutex);
	while (hui_atomic_load(&group->pending) > 0) {
		hui_job_t job;
		if (hui_jobs_pop(&job)) {
			hui_mutex_unlock(&g_jobs.mutex);
			hui_jobs_execute(&job);
			hui_mutex_lock(&g_jobs.mutex);
		} else {
			g_jobs.waiters++;
			hui_cond_wait(&g_jobs.done_cv, &g_jobs.mutex);
			g_jobs.waiters--;
		}
	}
	hui_mutex_unlock(&g_jobs.mutex);
}

/* ---------------------------------------------------------------------------
 *  parallel_for
 * ------------------------------------------------------------------------- */

typedef struct hui_parallel_t {
	hui_atomic_t next;
	uint32_t count;
	uint32_t grain;
	hui_job_range_fn function;
	void* context;
} hui_parallel_t;

static void hui_parallel_run(void* arg) {
	hui_parallel_t* parallel = (hui_parallel_t*)arg;
	for (;;) {
		uint32_t end = (uint32_t)hui_atomic_add(&parallel->next, (int)parallel->grain);
		uint32_t begin = end - parallel->grain;
		if (begin >= parallel->count) break;
		if (end > parallel->count) end = parallel->count;
		parallel->function(begin, end, parallel->context);
	}
}

void hui_jobs_parallel_for(uint32_t count, uint32_t grain, hui_job_range_fn function, void* context) {
	hui_parallel_t parallel;
	hui_job_group_t group;
	uint32_t chunks, helpers, i;

	if (count == 0) return;
	if (grain == 0) grain = 1;
	if (grain > count) grain = count;
	chunks = (count + grain - 1) / grain;
	helpers = hui_minu(chunks - 1, (uint32_t)g_jobs.worker_count);
	if (helpers == 0 || count > 0x3FFFFFFFu) {
		function(0, count, context);
		return;
	}

	memset(&parallel, 0, sizeof(parallel));
	memset(&group, 0, sizeof(group));
	parallel.count = count;
	parallel.grain = grain;
	parallel.function = function;
	parallel.context = context;
	for (i = 0; i < helpers; i++) hui_jobs_submit(&group, hui_parallel_run, &parallel);
	hui_parallel_run(&parallel);
	hui_jobs_wait(&group);
}
