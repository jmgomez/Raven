/**
 * Raven - widgets/checkbox.c
 * Checkbox (Slate SCheckBox): a two-state mark with a text, drawn as a check
 * box, a radio button or an on / off switch.
 */

#include "common.h"

typedef struct rvn_checkbox_t {
	rvn_widget_t base;
	char* text;
	int checked;
	int armed;
	RCheckboxStyle style;
	RCheckboxChangedCallback callback;
	void* user_data;
} rvn_checkbox_t;

static void rvn_checkbox_init(rvn_widget_t* widget) {
	((rvn_checkbox_t*)widget)->style = R_CHECKBOX_CHECK;
}

static void rvn_checkbox_destroy(rvn_widget_t* widget) {
	rvn_free(((rvn_checkbox_t*)widget)->text);
}

/* Size of the mark: a square, or a wider pill for the switch style. */
static rvn_vec2_t rvn_checkbox_mark(const rvn_widget_t* widget) {
	float size = rvn_widget_metric(widget, R_METRIC_CHECK_SIZE);
	if (((const rvn_checkbox_t*)widget)->style == R_CHECKBOX_SWITCH) {
		/* the switch of the glass themes is a long capsule with an elongated knob */
		if (g_rvn.theme.glass) return rvn_vec2(floorf((size + 6.0f) * 2.1f), size + 6.0f);
		return rvn_vec2(floorf(size * 1.9f), size);
	}
	return rvn_vec2(size, size);
}

static rvn_vec2_t rvn_checkbox_measure(rvn_widget_t* widget) {
	rvn_checkbox_t* checkbox = (rvn_checkbox_t*)widget;
	rvn_vec2_t mark = rvn_checkbox_mark(widget);
	rvn_vec2_t desired = mark;
	if (checkbox->text && checkbox->text[0]) {
		desired.x += rvn_widget_metric(widget, R_METRIC_SPACING) +
			ceilf(rvn_widget_text_width(widget, R_INVALID_ID, 0.0f, checkbox->text));
		desired.y = rvn_maxf(desired.y, ceilf(rvn_widget_line_height(widget, R_INVALID_ID, 0.0f)));
	}
	desired.y = rvn_maxf(desired.y, rvn_widget_metric(widget, R_METRIC_ROW_HEIGHT));
	return desired;
}

static void rvn_checkbox_paint(rvn_widget_t* widget, RPainter* painter) {
	rvn_checkbox_t* checkbox = (rvn_checkbox_t*)widget;
	rvn_vec2_t mark = rvn_checkbox_mark(widget);
	rvn_rect_t box = rvn_rect(0.0f, floorf((widget->rect.h - mark.y) * 0.5f), mark.x, mark.y);
	int hovered = (widget->flags & RVN_WF_HOVERED) && painter->enabled;
	int pressed = (widget->flags & RVN_WF_PRESSED) && checkbox->armed;
	rvn_color_t accent = rvn_widget_color(widget, hovered ? R_COLOR_ACCENT_HOVERED : R_COLOR_ACCENT);
	rvn_color_t off = rvn_widget_color(widget, pressed ? R_COLOR_CONTROL_PRESSED : (hovered ? R_COLOR_CONTROL_HOVERED : R_COLOR_CONTROL));
	rvn_color_t outline = rvn_widget_color(widget, R_COLOR_CONTROL_BORDER);
	rvn_color_t on_mark = rvn_widget_color(widget, R_COLOR_ACCENT_TEXT);
	float border = rvn_widget_metric(widget, R_METRIC_BORDER_WIDTH);

	if (!painter->enabled) {
		accent = rvn_color_fade(accent, 0.4f);
		off = rvn_color_fade(off, 0.5f);
		outline = rvn_color_fade(outline, 0.5f);
	}

	if (g_rvn.theme.glass) {
		/* the mark fades and the knob slides instead of jumping */
		float on = rvn_paint_animate(painter, &widget->anim[0], checkbox->checked ? 1.0f : 0.0f, 14.0f);
		float press = rvn_widget_press(widget, painter, pressed);
		if (checkbox->style == R_CHECKBOX_SWITCH) {
			rvn_color_t track = rvn_color_mix(rvn_widget_color(widget, R_COLOR_TRACK), rvn_widget_color(widget, R_COLOR_SWITCH), on);
			float knob_h = mark.y - 4.0f, knob_w = floorf(knob_h * 1.45f);
			float knob_x = box.x + 2.0f + (mark.x - knob_w - 4.0f) * on;
			if (!painter->enabled) track = rvn_color_fade(track, 0.5f);
			rvn_paint_box(painter, box, mark.y * 0.5f, track, 0.0f, 0);
			rvn_paint_knob(painter, widget, rvn_rect(knob_x, box.y + 2.0f, knob_w, knob_h), press);
		} else {
			/* check boxes and radio buttons are discs: empty with a ring, or filled with the accent color */
			rvn_color_t ring = rvn_color_mix(outline, accent, on);
			rvn_paint_box(painter, box, mark.y * 0.5f, rvn_color_mix(off, accent, on), 1.5f * (1.0f - on), ring);
			if (on > 0.5f) {
				float opacity = painter->opacity;
				painter->opacity *= (on - 0.5f) * 2.0f;
				if (checkbox->style == R_CHECKBOX_RADIO) {
					float dot = floorf(mark.y * 0.38f);
					rvn_paint_box(painter, rvn_rect(box.x + (mark.x - dot) * 0.5f, box.y + (mark.y - dot) * 0.5f, dot, dot),
						dot * 0.5f, on_mark, 0.0f, 0);
				} else {
					rvn_paint_icon(painter, RVN_ICON_CHECK, box.x + mark.x * 0.5f, box.y + mark.y * 0.5f, mark.y * 0.5f, on_mark);
				}
				painter->opacity = opacity;
			}
		}
	} else switch (checkbox->style) {
		case R_CHECKBOX_RADIO:
			rvn_paint_box(painter, box, mark.y * 0.5f, checkbox->checked ? accent : off, checkbox->checked ? 0.0f : border, outline);
			if (checkbox->checked) {
				float dot = floorf(mark.y * 0.4f);
				rvn_paint_box(painter, rvn_rect(box.x + (mark.x - dot) * 0.5f, box.y + (mark.y - dot) * 0.5f, dot, dot),
					dot * 0.5f, on_mark, 0.0f, 0);
			}
			break;
		case R_CHECKBOX_SWITCH: {
			float knob = mark.y - 4.0f;
			float knob_x = checkbox->checked ? box.x + mark.x - knob - 2.0f : box.x + 2.0f;
			rvn_paint_box(painter, box, mark.y * 0.5f, checkbox->checked ? accent : off, checkbox->checked ? 0.0f : border, outline);
			rvn_paint_box(painter, rvn_rect(knob_x, box.y + 2.0f, knob, knob), knob * 0.5f,
				checkbox->checked ? on_mark : rvn_widget_color(widget, R_COLOR_HANDLE_HOVERED), 0.0f, 0);
			break;
		}
		case R_CHECKBOX_CHECK:
		default:
			rvn_paint_box(painter, box, 3.0f, checkbox->checked ? accent : off, checkbox->checked ? 0.0f : border, outline);
			if (checkbox->checked) {
				rvn_paint_icon(painter, RVN_ICON_CHECK, box.x + mark.x * 0.5f, box.y + mark.y * 0.5f, mark.y * 0.55f, on_mark);
			}
			break;
	}
	if (checkbox->text && checkbox->text[0]) {
		float x = mark.x + rvn_widget_metric(widget, R_METRIC_SPACING);
		rvn_paint_text_in(painter, R_INVALID_ID, rvn_widget_font_size(widget, 0.0f),
			rvn_rect(x, 0.0f, rvn_maxf(0.0f, widget->rect.w - x), widget->rect.h), 0.0f, checkbox->text, -1,
			rvn_widget_text_color(widget, painter));
	}
}

static void rvn_checkbox_paint_overlay(rvn_widget_t* widget, RPainter* painter) {
	rvn_paint_focus(widget, painter, g_rvn.theme.glass ? 10.0f : 3.0f);
}

static void rvn_checkbox_set(rvn_widget_t* widget, int checked, int notify) {
	rvn_checkbox_t* checkbox = (rvn_checkbox_t*)widget;
	checked = checked != 0;
	if (checkbox->checked == checked) return;
	checkbox->checked = checked;
	rvn_widget_invalidate_paint(widget);

	/* radio boxes of one parent are exclusive */
	if (checked && checkbox->style == R_CHECKBOX_RADIO && widget->parent) {
		uint32_t i;
		for (i = 0; i < widget->parent->child_count; i++) {
			rvn_widget_t* sibling = widget->parent->children[i];
			if (sibling != widget && sibling->cls->type == R_WIDGET_CHECKBOX &&
				((rvn_checkbox_t*)sibling)->style == R_CHECKBOX_RADIO) {
				rvn_checkbox_set(sibling, R_FALSE, notify);
			}
		}
	}
	if (notify && rvn_widget_alive(widget)) {
		if (checkbox->callback) checkbox->callback(widget->id, checked, checkbox->user_data);
		if (rvn_widget_alive(widget)) rvn_widget_fire_action(widget);
	}
}

static void rvn_checkbox_toggle(rvn_widget_t* widget) {
	rvn_checkbox_t* checkbox = (rvn_checkbox_t*)widget;
	/* a radio box can only be unchecked by checking another one */
	if (checkbox->style == R_CHECKBOX_RADIO && checkbox->checked) return;
	rvn_checkbox_set(widget, !checkbox->checked, R_TRUE);
}

static int rvn_checkbox_event(rvn_widget_t* widget, const REvent* event) {
	rvn_checkbox_t* checkbox = (rvn_checkbox_t*)widget;
	int inside = event->local_x >= 0.0f && event->local_y >= 0.0f &&
		event->local_x < widget->rect.w && event->local_y < widget->rect.h;

	switch (event->type) {
		case R_EVENT_MOUSE_DOWN:
			if (event->button != R_MOUSE_LEFT) return R_FALSE;
			widget->flags |= RVN_WF_PRESSED;
			checkbox->armed = R_TRUE;
			rvn_widget_capture_mouse(widget);
			rvn_widget_invalidate_paint(widget);
			return R_TRUE;
		case R_EVENT_MOUSE_MOVE:
			if (!(widget->flags & RVN_WF_PRESSED)) return R_FALSE;
			if (checkbox->armed != inside) {
				checkbox->armed = inside;
				rvn_widget_invalidate_paint(widget);
			}
			return R_TRUE;
		case R_EVENT_MOUSE_UP:
			if (event->button != R_MOUSE_LEFT || !(widget->flags & RVN_WF_PRESSED)) return R_FALSE;
			widget->flags &= ~RVN_WF_PRESSED;
			rvn_widget_release_mouse(widget);
			rvn_widget_invalidate_paint(widget);
			if (inside) rvn_checkbox_toggle(widget);
			return R_TRUE;
		case R_EVENT_KEY_DOWN:
			if (!(widget->flags & RVN_WF_FOCUSED) || event->repeat) return R_FALSE;
			if (event->key == R_KEY_SPACE || event->key == R_KEY_ENTER) {
				rvn_checkbox_toggle(widget);
				return R_TRUE;
			}
			return R_FALSE;
		default:
			return R_FALSE;
	}
}

static int rvn_checkbox_set_attribute(rvn_widget_t* widget, const char* name, const char* value) {
	rvn_checkbox_t* checkbox = (rvn_checkbox_t*)widget;
	int flag;
	if (RVN_ATTR("Text")) {
		rvn_str_set(&checkbox->text, value);
	} else if (RVN_ATTR("Checked")) {
		if (!rvn_parse_bool(value, &flag)) return R_FALSE;
		rvn_checkbox_set(widget, flag, R_FALSE);
	} else if (RVN_ATTR("Style")) {
		if (rvn_stricmp(value, "Check") == 0) checkbox->style = R_CHECKBOX_CHECK;
		else if (rvn_stricmp(value, "Radio") == 0) checkbox->style = R_CHECKBOX_RADIO;
		else if (rvn_stricmp(value, "Switch") == 0) checkbox->style = R_CHECKBOX_SWITCH;
		else return R_FALSE;
	} else {
		return R_FALSE;
	}
	rvn_widget_invalidate_layout(widget);
	return R_TRUE;
}

const rvn_widget_class_t rvn_class_checkbox = {
	.type = R_WIDGET_CHECKBOX,
	.name = "Checkbox",
	.size = sizeof(rvn_checkbox_t),
	.flags = RVN_CF_NO_CHILDREN | RVN_CF_FOCUSABLE,
	.init = rvn_checkbox_init,
	.destroy = rvn_checkbox_destroy,
	.measure = rvn_checkbox_measure,
	.paint = rvn_checkbox_paint,
	.paint_overlay = rvn_checkbox_paint_overlay,
	.event = rvn_checkbox_event,
	.set_attribute = rvn_checkbox_set_attribute
};

Rid Rcheckbox_create(const char* text) {
	rvn_widget_t* widget;
	RVN_API_GUARD(R_INVALID_ID);
	widget = rvn_widget_create(R_WIDGET_CHECKBOX);
	if (!widget) return R_INVALID_ID;
	((rvn_checkbox_t*)widget)->text = rvn_strdup(text);
	return widget->id;
}

void Rcheckbox_set_text(Rid id, const char* text) {
	RVN_WIDGET_VOID(widget, id, R_WIDGET_CHECKBOX);
	if (rvn_str_set(&((rvn_checkbox_t*)widget)->text, text)) rvn_widget_invalidate_layout(widget);
}

void Rcheckbox_set_checked(Rid id, int checked) {
	RVN_WIDGET_VOID(widget, id, R_WIDGET_CHECKBOX);
	rvn_checkbox_set(widget, checked, R_FALSE);
}

int Rcheckbox_is_checked(Rid id) {
	RVN_WIDGET(widget, id, R_WIDGET_CHECKBOX, R_FALSE);
	return ((rvn_checkbox_t*)widget)->checked;
}

void Rcheckbox_set_style(Rid id, RCheckboxStyle style) {
	RVN_WIDGET_VOID(widget, id, R_WIDGET_CHECKBOX);
	if (style < R_CHECKBOX_CHECK || style > R_CHECKBOX_SWITCH) {
		rvn_error(R_ERROR_INVALID_ENUM, R_SEVERITY_ERROR, "Rcheckbox_set_style: invalid style");
		return;
	}
	((rvn_checkbox_t*)widget)->style = style;
	rvn_widget_invalidate_layout(widget);
}

void Rcheckbox_set_changed_callback(Rid id, RCheckboxChangedCallback callback, void* user_data) {
	RVN_WIDGET_VOID(widget, id, R_WIDGET_CHECKBOX);
	((rvn_checkbox_t*)widget)->callback = callback;
	((rvn_checkbox_t*)widget)->user_data = user_data;
}
