/**
 * HUI - widget/paint.c
 * Tree traversal that fills the draw list, with parallel painting of large
 * subtrees, and hit testing.
 *
 * Painting only reads the widget tree, so sibling subtrees can be painted at
 * the same time: each job fills its own draw list and the lists are appended
 * in child order afterwards, which keeps the result identical to a serial
 * paint. Widgets whose paint calls application code (HUI_CF_UI_PAINT) are
 * always painted on the UI thread.
 */

#include "widget.h"

/** A widget forks when its subtree holds at least this many widgets. */
#define HUI_PAINT_FORK_MIN 512
/** Minimum number of widgets given to one job. */
#define HUI_PAINT_CHUNK_MIN 96
#define HUI_PAINT_MAX_CHUNKS 32
/** Widgets may draw a little outside their bounds (shadows, focus outlines). */
#define HUI_PAINT_CULL_MARGIN 48.0f

/* ---------------------------------------------------------------------------
 *  Scratch draw lists reused by the paint jobs
 * ------------------------------------------------------------------------- */

static struct {
	hui_mutex_t mutex;
	hui_drawlist_t** lists;
	uint32_t count;
	uint32_t cap;
	hui_atomic_t jobs;
	int initialized;
} g_paint;

void hui_paint_init(void) {
	memset(&g_paint, 0, sizeof(g_paint));
	hui_mutex_init(&g_paint.mutex);
	g_paint.initialized = HUI_TRUE;
}

void hui_paint_shutdown(void) {
	uint32_t i;
	if (!g_paint.initialized) return;
	for (i = 0; i < g_paint.count; i++) {
		hui_drawlist_free(g_paint.lists[i]);
		hui_free(g_paint.lists[i]);
	}
	hui_free(g_paint.lists);
	hui_mutex_destroy(&g_paint.mutex);
	memset(&g_paint, 0, sizeof(g_paint));
}

/** Number of paint jobs started since the last call. */
int hui_paint_take_job_count(void) {
	return hui_atomic_exchange(&g_paint.jobs, 0);
}

static hui_drawlist_t* hui_paint_scratch_acquire(void) {
	hui_drawlist_t* list = NULL;
	hui_mutex_lock(&g_paint.mutex);
	if (g_paint.count > 0) list = g_paint.lists[--g_paint.count];
	hui_mutex_unlock(&g_paint.mutex);
	if (!list) {
		list = (hui_drawlist_t*)hui_alloc(sizeof(hui_drawlist_t));
		hui_drawlist_init(list);
	}
	hui_drawlist_reset(list);
	return list;
}

static void hui_paint_scratch_release(hui_drawlist_t* list) {
	hui_mutex_lock(&g_paint.mutex);
	HUI_ARRAY_RESERVE(g_paint.lists, g_paint.cap, g_paint.count + 1);
	g_paint.lists[g_paint.count++] = list;
	hui_mutex_unlock(&g_paint.mutex);
}

/* ---------------------------------------------------------------------------
 *  Children
 * ------------------------------------------------------------------------- */

typedef struct hui_paint_chunk_t {
	hui_widget_t* parent;
	uint32_t first;
	uint32_t last;
	uint32_t weight;
	int needs_ui_thread;
	HUI_Painter painter;
	/** Where the instances of the chunk go in the list of the parent. */
	HUI_DrawInstance* destination;
} hui_paint_chunk_t;

/** Below this many instances, copying them in parallel is not worth a job. */
#define HUI_PAINT_PARALLEL_COPY_MIN 8192

static void hui_paint_range(hui_widget_t* parent, uint32_t first, uint32_t last, const HUI_Painter* painter) {
	uint32_t i;
	for (i = first; i < last; i++) {
		hui_widget_t* child = parent->children[i];
		if (parent->cls->child_active && !parent->cls->child_active(parent, child)) continue;
		hui_widget_paint(child, painter);
	}
}

static void hui_paint_chunk_job(void* arg) {
	hui_paint_chunk_t* chunk = (hui_paint_chunk_t*)arg;
	hui_paint_range(chunk->parent, chunk->first, chunk->last, &chunk->painter);
}

static void hui_paint_copy_job(void* arg) {
	hui_paint_chunk_t* chunk = (hui_paint_chunk_t*)arg;
	const hui_drawlist_t* source = chunk->painter.list;
	if (source->instance_count) {
		memcpy(chunk->destination, source->instances, sizeof(HUI_DrawInstance) * source->instance_count);
	}
}

/* Returns HUI_FALSE when the children should simply be painted in order. */
static int hui_paint_children_parallel(hui_widget_t* widget, const HUI_Painter* painter) {
	hui_paint_chunk_t chunks[HUI_PAINT_MAX_CHUNKS];
	hui_job_group_t group;
	uint32_t chunk_count = 0, target, i;
	int workers = hui_jobs_worker_count();

	target = widget->subtree_count / (uint32_t)((workers + 1) * 2);
	if (target < HUI_PAINT_CHUNK_MIN) target = HUI_PAINT_CHUNK_MIN;

	for (i = 0; i < widget->child_count; i++) {
		hui_widget_t* child = widget->children[i];
		hui_paint_chunk_t* chunk;
		if (chunk_count == 0 || (chunks[chunk_count - 1].weight >= target && chunk_count < HUI_PAINT_MAX_CHUNKS)) {
			chunk = &chunks[chunk_count++];
			chunk->parent = widget;
			chunk->first = i;
			chunk->weight = 0;
			chunk->needs_ui_thread = HUI_FALSE;
		}
		chunk = &chunks[chunk_count - 1];
		chunk->last = i + 1;
		chunk->weight += child->subtree_count;
		if (child->ui_paint_count > 0) chunk->needs_ui_thread = HUI_TRUE;
	}
	if (chunk_count < 2) return HUI_FALSE;

	memset(&group, 0, sizeof(group));
	for (i = 0; i < chunk_count; i++) {
		chunks[i].painter = *painter;
		chunks[i].painter.list = hui_paint_scratch_acquire();
		/* the last chunk is kept for this thread, which would otherwise only wait */
		if (!chunks[i].needs_ui_thread && i + 1 < chunk_count) {
			hui_jobs_submit(&group, hui_paint_chunk_job, &chunks[i]);
			hui_atomic_add(&g_paint.jobs, 1);
		}
	}
	for (i = 0; i < chunk_count; i++) {
		if (chunks[i].needs_ui_thread || i + 1 == chunk_count) hui_paint_chunk_job(&chunks[i]);
	}
	hui_jobs_wait(&group);

	/* gather the chunks in child order: the result is the same as a serial paint */
	{
		hui_drawlist_t* list = painter->list;
		uint32_t base = list->instance_count, total = 0;
		for (i = 0; i < chunk_count; i++) total += chunks[i].painter.list->instance_count;
		HUI_ARRAY_RESERVE(list->instances, list->instance_cap, base + total);
		total = 0;
		for (i = 0; i < chunk_count; i++) {
			chunks[i].destination = list->instances + base + total;
			total += chunks[i].painter.list->instance_count;
		}
		/* the copy itself is large enough to be shared between the threads */
		for (i = 0; i < chunk_count; i++) {
			if (total >= HUI_PAINT_PARALLEL_COPY_MIN && i + 1 < chunk_count) {
				hui_jobs_submit(&group, hui_paint_copy_job, &chunks[i]);
			} else {
				hui_paint_copy_job(&chunks[i]);
			}
		}
		hui_jobs_wait(&group);
		list->instance_count = base + total;
		for (i = 0; i < chunk_count; i++) {
			hui_drawlist_append_commands(list, chunks[i].painter.list,
				(uint32_t)(chunks[i].destination - list->instances));
			hui_paint_scratch_release(chunks[i].painter.list);
		}
	}
	return HUI_TRUE;
}

static void hui_paint_children(hui_widget_t* widget, const HUI_Painter* painter) {
	if (widget->subtree_count >= HUI_PAINT_FORK_MIN && widget->child_count >= 2 && hui_jobs_worker_count() > 0) {
		if (hui_paint_children_parallel(widget, painter)) return;
	}
	hui_paint_range(widget, 0, widget->child_count, painter);
}

/* ---------------------------------------------------------------------------
 *  Widget
 * ------------------------------------------------------------------------- */

void hui_widget_paint(hui_widget_t* widget, const HUI_Painter* parent) {
	HUI_Painter painter;
	hui_box_t bounds;

	if (widget->visibility != HUI_VISIBLE || widget->opacity <= 0.0f) return;

	painter = *parent;
	painter.ox += widget->rect.x;
	painter.oy += widget->rect.y;
	painter.opacity *= widget->opacity;
	painter.clip_depth = 0;
	if (widget->flags & HUI_WF_DISABLED) painter.enabled = HUI_FALSE;
	if (hui_widget_has_color(widget, HUI_COLOR_TEXT)) painter.text_color = hui_widget_color(widget, HUI_COLOR_TEXT);

	bounds = hui_paint_snap(&painter, hui_rect(0.0f, 0.0f, widget->rect.w, widget->rect.h));
	if (!(widget->cls->flags & HUI_CF_OVERFLOW)) {
		/* nothing of this subtree can be visible: skip it entirely */
		float margin = HUI_PAINT_CULL_MARGIN * painter.scale;
		hui_box_t grown = hui_box(bounds.x0 - margin, bounds.y0 - margin, bounds.x1 + margin, bounds.y1 + margin);
		if (!hui_box_overlaps(grown, painter.clip)) return;
	}

	if (widget->cls->paint) widget->cls->paint(widget, &painter);

	if (widget->child_count > 0) {
		hui_box_t saved_clip = painter.clip;
		painter.clip_depth = 0;
		if (widget->flags & HUI_WF_CLIP) painter.clip = hui_box_intersect(painter.clip, bounds);
		if (!hui_box_empty(painter.clip)) hui_paint_children(widget, &painter);
		painter.clip = saved_clip;
	}

	if (widget->cls->paint_overlay) {
		painter.clip_depth = 0;
		widget->cls->paint_overlay(widget, &painter);
	}
}

/* ---------------------------------------------------------------------------
 *  Hit testing
 * ------------------------------------------------------------------------- */

hui_widget_t* hui_widget_hit_test(hui_widget_t* widget, float x, float y) {
	float local_x, local_y;
	int inside;
	uint32_t i;

	if (widget->visibility != HUI_VISIBLE) return NULL;
	local_x = x - widget->rect.x;
	local_y = y - widget->rect.y;
	inside = local_x >= 0.0f && local_y >= 0.0f && local_x < widget->rect.w && local_y < widget->rect.h;
	/* children are inside the bounds unless the class says they may overflow */
	if (!inside && ((widget->flags & HUI_WF_CLIP) || !(widget->cls->flags & HUI_CF_OVERFLOW))) return NULL;

	/* the last child is drawn on top: test it first */
	for (i = widget->child_count; i > 0; i--) {
		hui_widget_t* child = widget->children[i - 1];
		hui_widget_t* hit;
		if (widget->cls->child_active && !widget->cls->child_active(widget, child)) continue;
		hit = hui_widget_hit_test(child, local_x, local_y);
		if (hit) return hit;
	}
	if (inside && !(widget->flags & HUI_WF_NO_HIT)) return widget;
	return NULL;
}
