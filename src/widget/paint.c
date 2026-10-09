/**
 * Raven - widget/paint.c
 * Tree traversal that fills the draw list, with parallel painting of large
 * subtrees, and hit testing.
 *
 * Painting only reads the widget tree, so sibling subtrees can be painted at
 * the same time: each job fills its own draw list and the lists are appended
 * in child order afterwards, which keeps the result identical to a serial
 * paint. Widgets whose paint calls application code (RVN_CF_UI_PAINT) are
 * always painted on the UI thread.
 */

#include "widget.h"

/** A widget forks when its subtree holds at least this many widgets. */
#define RVN_PAINT_FORK_MIN 512
/** Minimum number of widgets given to one job. */
#define RVN_PAINT_CHUNK_MIN 96
#define RVN_PAINT_MAX_CHUNKS 32
/** Widgets may draw a little outside their bounds (shadows, focus outlines). */
#define RVN_PAINT_CULL_MARGIN 48.0f

/* ---------------------------------------------------------------------------
 *  Scratch draw lists reused by the paint jobs
 * ------------------------------------------------------------------------- */

static struct {
	rvn_mutex_t mutex;
	rvn_drawlist_t** lists;
	uint32_t count;
	uint32_t cap;
	rvn_atomic_t jobs;
	int initialized;
} g_paint;

void rvn_paint_init(void) {
	memset(&g_paint, 0, sizeof(g_paint));
	rvn_mutex_init(&g_paint.mutex);
	g_paint.initialized = R_TRUE;
}

void rvn_paint_shutdown(void) {
	uint32_t i;
	if (!g_paint.initialized) return;
	for (i = 0; i < g_paint.count; i++) {
		rvn_drawlist_free(g_paint.lists[i]);
		rvn_free(g_paint.lists[i]);
	}
	rvn_free(g_paint.lists);
	rvn_mutex_destroy(&g_paint.mutex);
	memset(&g_paint, 0, sizeof(g_paint));
}

/** Number of paint jobs started since the last call. */
int rvn_paint_take_job_count(void) {
	return rvn_atomic_exchange(&g_paint.jobs, 0);
}

static rvn_drawlist_t* rvn_paint_scratch_acquire(void) {
	rvn_drawlist_t* list = NULL;
	rvn_mutex_lock(&g_paint.mutex);
	if (g_paint.count > 0) list = g_paint.lists[--g_paint.count];
	rvn_mutex_unlock(&g_paint.mutex);
	if (!list) {
		list = (rvn_drawlist_t*)rvn_alloc(sizeof(rvn_drawlist_t));
		rvn_drawlist_init(list);
	}
	rvn_drawlist_reset(list);
	return list;
}

static void rvn_paint_scratch_release(rvn_drawlist_t* list) {
	rvn_mutex_lock(&g_paint.mutex);
	RVN_ARRAY_RESERVE(g_paint.lists, g_paint.cap, g_paint.count + 1);
	g_paint.lists[g_paint.count++] = list;
	rvn_mutex_unlock(&g_paint.mutex);
}

/* ---------------------------------------------------------------------------
 *  Children
 * ------------------------------------------------------------------------- */

typedef struct rvn_paint_chunk_t {
	rvn_widget_t* parent;
	uint32_t first;
	uint32_t last;
	uint32_t weight;
	int needs_ui_thread;
	RPainter painter;
	/** Where the instances of the chunk go in the list of the parent. */
	RDrawInstance* destination;
} rvn_paint_chunk_t;

/** Below this many instances, copying them in parallel is not worth a job. */
#define RVN_PAINT_PARALLEL_COPY_MIN 8192

static void rvn_paint_range(rvn_widget_t* parent, uint32_t first, uint32_t last, const RPainter* painter) {
	uint32_t i;
	for (i = first; i < last; i++) {
		rvn_widget_t* child = parent->children[i];
		if (parent->cls->child_active && !parent->cls->child_active(parent, child)) continue;
		rvn_widget_paint(child, painter);
	}
}

static void rvn_paint_chunk_job(void* arg) {
	rvn_paint_chunk_t* chunk = (rvn_paint_chunk_t*)arg;
	rvn_paint_range(chunk->parent, chunk->first, chunk->last, &chunk->painter);
}

static void rvn_paint_copy_job(void* arg) {
	rvn_paint_chunk_t* chunk = (rvn_paint_chunk_t*)arg;
	const rvn_drawlist_t* source = chunk->painter.list;
	if (source->instance_count) {
		memcpy(chunk->destination, source->instances, sizeof(RDrawInstance) * source->instance_count);
	}
}

/* Returns R_FALSE when the children should simply be painted in order. */
static int rvn_paint_children_parallel(rvn_widget_t* widget, const RPainter* painter) {
	rvn_paint_chunk_t chunks[RVN_PAINT_MAX_CHUNKS];
	rvn_job_group_t group;
	uint32_t chunk_count = 0, target, i;
	int workers = rvn_jobs_worker_count();

	target = widget->subtree_count / (uint32_t)((workers + 1) * 2);
	if (target < RVN_PAINT_CHUNK_MIN) target = RVN_PAINT_CHUNK_MIN;

	for (i = 0; i < widget->child_count; i++) {
		rvn_widget_t* child = widget->children[i];
		rvn_paint_chunk_t* chunk;
		if (chunk_count == 0 || (chunks[chunk_count - 1].weight >= target && chunk_count < RVN_PAINT_MAX_CHUNKS)) {
			chunk = &chunks[chunk_count++];
			chunk->parent = widget;
			chunk->first = i;
			chunk->weight = 0;
			chunk->needs_ui_thread = R_FALSE;
		}
		chunk = &chunks[chunk_count - 1];
		chunk->last = i + 1;
		chunk->weight += child->subtree_count;
		if (child->ui_paint_count > 0) chunk->needs_ui_thread = R_TRUE;
	}
	if (chunk_count < 2) return R_FALSE;

	memset(&group, 0, sizeof(group));
	for (i = 0; i < chunk_count; i++) {
		chunks[i].painter = *painter;
		chunks[i].painter.list = rvn_paint_scratch_acquire();
		/* the last chunk is kept for this thread, which would otherwise only wait */
		if (!chunks[i].needs_ui_thread && i + 1 < chunk_count) {
			rvn_jobs_submit(&group, rvn_paint_chunk_job, &chunks[i]);
			rvn_atomic_add(&g_paint.jobs, 1);
		}
	}
	for (i = 0; i < chunk_count; i++) {
		if (chunks[i].needs_ui_thread || i + 1 == chunk_count) rvn_paint_chunk_job(&chunks[i]);
	}
	rvn_jobs_wait(&group);

	/* gather the chunks in child order: the result is the same as a serial paint */
	{
		rvn_drawlist_t* list = painter->list;
		uint32_t base = list->instance_count, total = 0;
		for (i = 0; i < chunk_count; i++) total += chunks[i].painter.list->instance_count;
		RVN_ARRAY_RESERVE(list->instances, list->instance_cap, base + total);
		total = 0;
		for (i = 0; i < chunk_count; i++) {
			chunks[i].destination = list->instances + base + total;
			total += chunks[i].painter.list->instance_count;
		}
		/* the copy itself is large enough to be shared between the threads */
		for (i = 0; i < chunk_count; i++) {
			if (total >= RVN_PAINT_PARALLEL_COPY_MIN && i + 1 < chunk_count) {
				rvn_jobs_submit(&group, rvn_paint_copy_job, &chunks[i]);
			} else {
				rvn_paint_copy_job(&chunks[i]);
			}
		}
		rvn_jobs_wait(&group);
		list->instance_count = base + total;
		for (i = 0; i < chunk_count; i++) {
			rvn_drawlist_append_commands(list, chunks[i].painter.list,
				(uint32_t)(chunks[i].destination - list->instances));
			rvn_paint_scratch_release(chunks[i].painter.list);
		}
	}
	return R_TRUE;
}

static void rvn_paint_children(rvn_widget_t* widget, const RPainter* painter) {
	if (widget->subtree_count >= RVN_PAINT_FORK_MIN && widget->child_count >= 2 && rvn_jobs_worker_count() > 0) {
		if (rvn_paint_children_parallel(widget, painter)) return;
	}
	rvn_paint_range(widget, 0, widget->child_count, painter);
}

/* ---------------------------------------------------------------------------
 *  Widget
 * ------------------------------------------------------------------------- */

void rvn_widget_paint(rvn_widget_t* widget, const RPainter* parent) {
	RPainter painter;
	rvn_box_t bounds;

	if (widget->visibility != R_VISIBLE || widget->opacity <= 0.0f) return;

	painter = *parent;
	painter.ox += widget->rect.x;
	painter.oy += widget->rect.y;
	painter.opacity *= widget->opacity;
	painter.clip_depth = 0;
	if (widget->flags & RVN_WF_DISABLED) painter.enabled = R_FALSE;
	if (rvn_widget_has_color(widget, R_COLOR_TEXT)) painter.text_color = rvn_widget_color(widget, R_COLOR_TEXT);

	bounds = rvn_paint_snap(&painter, rvn_rect(0.0f, 0.0f, widget->rect.w, widget->rect.h));
	if (!(widget->cls->flags & RVN_CF_OVERFLOW)) {
		/* nothing of this subtree can be visible: skip it entirely */
		float margin = RVN_PAINT_CULL_MARGIN * painter.scale;
		rvn_box_t grown = rvn_box(bounds.x0 - margin, bounds.y0 - margin, bounds.x1 + margin, bounds.y1 + margin);
		if (!rvn_box_overlaps(grown, painter.clip)) return;
	}

	if (widget->cls->paint) widget->cls->paint(widget, &painter);

	if (widget->child_count > 0) {
		rvn_box_t saved_clip = painter.clip;
		painter.clip_depth = 0;
		if (widget->flags & RVN_WF_CLIP) painter.clip = rvn_box_intersect(painter.clip, bounds);
		if (!rvn_box_empty(painter.clip)) rvn_paint_children(widget, &painter);
		painter.clip = saved_clip;
	}

	if (widget->cls->paint_overlay) {
		painter.clip_depth = 0;
		widget->cls->paint_overlay(widget, &painter);
	}
}

/* ---------------------------------------------------------------------------
 *  Animation
 * ------------------------------------------------------------------------- */

float rvn_paint_animate(RPainter* painter, float* value, float target, float speed) {
	float distance = target - *value;
	if (distance > -0.004f && distance < 0.004f) {
		*value = target;
		return target;
	}
	/* an exponential approach: fast at first, soft landing, any frame rate */
	*value += distance * (1.0f - expf(-speed * painter->dt));
	if (painter->window) rvn_atomic_store(&painter->window->animating, 1);
	return *value;
}

/* ---------------------------------------------------------------------------
 *  Hit testing
 * ------------------------------------------------------------------------- */

rvn_widget_t* rvn_widget_hit_test(rvn_widget_t* widget, float x, float y) {
	float local_x, local_y;
	int inside;
	uint32_t i;

	if (widget->visibility != R_VISIBLE) return NULL;
	local_x = x - widget->rect.x;
	local_y = y - widget->rect.y;
	inside = local_x >= 0.0f && local_y >= 0.0f && local_x < widget->rect.w && local_y < widget->rect.h;
	/* children are inside the bounds unless the class says they may overflow */
	if (!inside && ((widget->flags & RVN_WF_CLIP) || !(widget->cls->flags & RVN_CF_OVERFLOW))) return NULL;

	/* the last child is drawn on top: test it first */
	for (i = widget->child_count; i > 0; i--) {
		rvn_widget_t* child = widget->children[i - 1];
		rvn_widget_t* hit;
		if (widget->cls->child_active && !widget->cls->child_active(widget, child)) continue;
		hit = rvn_widget_hit_test(child, local_x, local_y);
		if (hit) return hit;
	}
	if (inside && !(widget->flags & RVN_WF_NO_HIT)) return widget;
	return NULL;
}
