/**
 * HUI - xml/xml_attributes.c
 * Text to value conversions, and the attributes every widget understands
 * (name, visibility, size, slot, style overrides, action).
 *
 * Number parsing does not depend on the C locale: "1.5" is always valid,
 * whatever setlocale() the application called.
 */

#include "../widgets/common.h"

/* ---------------------------------------------------------------------------
 *  Values
 * ------------------------------------------------------------------------- */

static const char* hui_skip_spaces(const char* text) {
	while (*text == ' ' || *text == '\t' || *text == '\n' || *text == '\r') text++;
	return text;
}

/* Reads a number at *cursor and advances it. Returns HUI_FALSE when there is none. */
static int hui_read_number(const char** cursor, float* value) {
	const char* p = hui_skip_spaces(*cursor);
	double result = 0.0, scale = 1.0;
	int negative = 0, digits = 0;

	if (*p == '-' || *p == '+') {
		negative = *p == '-';
		p++;
	}
	while (*p >= '0' && *p <= '9') {
		result = result * 10.0 + (double)(*p - '0');
		digits++;
		p++;
	}
	if (*p == '.') {
		p++;
		while (*p >= '0' && *p <= '9') {
			scale *= 0.1;
			result += (double)(*p - '0') * scale;
			digits++;
			p++;
		}
	}
	if (digits == 0) return HUI_FALSE;
	if (*p == 'e' || *p == 'E') {
		const char* e = p + 1;
		int exponent = 0, exponent_negative = 0, exponent_digits = 0;
		if (*e == '-' || *e == '+') {
			exponent_negative = *e == '-';
			e++;
		}
		while (*e >= '0' && *e <= '9' && exponent < 100) {
			exponent = exponent * 10 + (*e - '0');
			exponent_digits++;
			e++;
		}
		if (exponent_digits > 0) {
			result *= pow(10.0, exponent_negative ? -exponent : exponent);
			p = e;
		}
	}
	*value = (float)(negative ? -result : result);
	*cursor = p;
	return HUI_TRUE;
}

int hui_parse_float(const char* text, float* value) {
	float number;
	if (!text || !hui_read_number(&text, &number)) return HUI_FALSE;
	if (*hui_skip_spaces(text) != '\0') return HUI_FALSE;
	*value = number;
	return HUI_TRUE;
}

int hui_parse_int(const char* text, int* value) {
	float number;
	if (!hui_parse_float(text, &number)) return HUI_FALSE;
	*value = (int)(number < 0.0f ? number - 0.5f : number + 0.5f);
	return HUI_TRUE;
}

int hui_parse_bool(const char* text, int* value) {
	if (!text) return HUI_FALSE;
	if (hui_stricmp(text, "true") == 0 || hui_stricmp(text, "yes") == 0 || strcmp(text, "1") == 0) *value = HUI_TRUE;
	else if (hui_stricmp(text, "false") == 0 || hui_stricmp(text, "no") == 0 || strcmp(text, "0") == 0) *value = HUI_FALSE;
	else return HUI_FALSE;
	return HUI_TRUE;
}

int hui_parse_floats(const char* text, float* values, int max) {
	int count = 0;
	if (!text) return 0;
	while (count < max) {
		if (!hui_read_number(&text, &values[count])) return 0;
		count++;
		text = hui_skip_spaces(text);
		if (*text == '\0') return count;
		/* numbers are separated by a comma or by spaces */
		if (*text == ',') text++;
	}
	return *hui_skip_spaces(text) == '\0' ? count : 0;
}

int hui_parse_margin(const char* text, hui_margin_t* margin) {
	float values[4];
	switch (hui_parse_floats(text, values, 4)) {
		case 1:
			*margin = hui_margin(values[0], values[0], values[0], values[0]);
			return HUI_TRUE;
		case 2:
			*margin = hui_margin(values[0], values[1], values[0], values[1]);
			return HUI_TRUE;
		case 4:
			*margin = hui_margin(values[0], values[1], values[2], values[3]);
			return HUI_TRUE;
		default:
			return HUI_FALSE;
	}
}

static int hui_hex_digit(char c) {
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

int hui_parse_color(const char* text, hui_color_t* color) {
	static const struct { const char* name; uint32_t rgb; uint32_t alpha; } named[] = {
		{ "transparent", 0x000000, 0 }, { "white", 0xFFFFFF, 255 }, { "black", 0x000000, 255 },
		{ "red", 0xE5484D, 255 }, { "green", 0x46A758, 255 }, { "blue", 0x3B82F6, 255 },
		{ "yellow", 0xF5D90A, 255 }, { "orange", 0xF76B15, 255 }, { "cyan", 0x05A2C2, 255 },
		{ "magenta", 0xD6409F, 255 }, { "gray", 0x808080, 255 }, { "grey", 0x808080, 255 }
	};
	uint32_t digits[8];
	size_t length, i;

	if (!text) return HUI_FALSE;
	text = hui_skip_spaces(text);
	if (text[0] != '#') {
		for (i = 0; i < HUI_COUNT_OF(named); i++) {
			if (hui_stricmp(text, named[i].name) == 0) {
				*color = hui_hex(named[i].rgb, named[i].alpha);
				return HUI_TRUE;
			}
		}
		return HUI_FALSE;
	}
	text++;
	length = strlen(text);
	if (length != 3 && length != 4 && length != 6 && length != 8) return HUI_FALSE;
	for (i = 0; i < length; i++) {
		int digit = hui_hex_digit(text[i]);
		if (digit < 0) return HUI_FALSE;
		digits[i] = (uint32_t)digit;
	}
	if (length <= 4) {
		/* #RGB and #RGBA: each digit is doubled */
		*color = hui_rgba8(digits[0] * 17u, digits[1] * 17u, digits[2] * 17u, length == 4 ? digits[3] * 17u : 255u);
	} else {
		*color = hui_rgba8(digits[0] * 16u + digits[1], digits[2] * 16u + digits[3], digits[4] * 16u + digits[5],
			length == 8 ? digits[6] * 16u + digits[7] : 255u);
	}
	return HUI_TRUE;
}

int hui_parse_orientation(const char* text, HUI_EOrientation* orientation) {
	if (hui_stricmp(text, "Horizontal") == 0) *orientation = HUI_HORIZONTAL;
	else if (hui_stricmp(text, "Vertical") == 0) *orientation = HUI_VERTICAL;
	else return HUI_FALSE;
	return HUI_TRUE;
}

/* ---------------------------------------------------------------------------
 *  Names of the theme values ("Color.Accent", "Metric.FontSize")
 * ------------------------------------------------------------------------- */

static const struct { const char* name; HUI_EColor role; } g_color_names[] = {
	{ "Window", HUI_COLOR_WINDOW }, { "Panel", HUI_COLOR_PANEL }, { "Border", HUI_COLOR_BORDER },
	{ "Text", HUI_COLOR_TEXT }, { "TextDisabled", HUI_COLOR_TEXT_DISABLED }, { "TextPlaceholder", HUI_COLOR_TEXT_PLACEHOLDER },
	{ "Accent", HUI_COLOR_ACCENT }, { "AccentHovered", HUI_COLOR_ACCENT_HOVERED }, { "AccentPressed", HUI_COLOR_ACCENT_PRESSED },
	{ "AccentText", HUI_COLOR_ACCENT_TEXT }, { "Control", HUI_COLOR_CONTROL }, { "ControlHovered", HUI_COLOR_CONTROL_HOVERED },
	{ "ControlPressed", HUI_COLOR_CONTROL_PRESSED }, { "ControlBorder", HUI_COLOR_CONTROL_BORDER }, { "Input", HUI_COLOR_INPUT },
	{ "Focus", HUI_COLOR_FOCUS }, { "Selection", HUI_COLOR_SELECTION }, { "RowHovered", HUI_COLOR_ROW_HOVERED },
	{ "Track", HUI_COLOR_TRACK }, { "Handle", HUI_COLOR_HANDLE }, { "HandleHovered", HUI_COLOR_HANDLE_HOVERED },
	{ "Separator", HUI_COLOR_SEPARATOR }, { "Header", HUI_COLOR_HEADER }, { "HeaderHovered", HUI_COLOR_HEADER_HOVERED },
	{ "TabActive", HUI_COLOR_TAB_ACTIVE }, { "Popup", HUI_COLOR_POPUP }, { "PopupBorder", HUI_COLOR_POPUP_BORDER },
	{ "Shadow", HUI_COLOR_SHADOW }, { "Tooltip", HUI_COLOR_TOOLTIP }, { "TooltipText", HUI_COLOR_TOOLTIP_TEXT }
};

static const struct { const char* name; HUI_EMetric metric; } g_metric_names[] = {
	{ "FontSize", HUI_METRIC_FONT_SIZE }, { "CornerRadius", HUI_METRIC_CORNER_RADIUS }, { "BorderWidth", HUI_METRIC_BORDER_WIDTH },
	{ "ControlHeight", HUI_METRIC_CONTROL_HEIGHT }, { "PaddingX", HUI_METRIC_PADDING_X }, { "PaddingY", HUI_METRIC_PADDING_Y },
	{ "Spacing", HUI_METRIC_SPACING }, { "ScrollbarSize", HUI_METRIC_SCROLLBAR_SIZE }, { "RowHeight", HUI_METRIC_ROW_HEIGHT },
	{ "Indent", HUI_METRIC_INDENT }, { "SplitterSize", HUI_METRIC_SPLITTER_SIZE }, { "CheckSize", HUI_METRIC_CHECK_SIZE },
	{ "SliderHandleSize", HUI_METRIC_SLIDER_HANDLE_SIZE }
};

static int hui_color_role_by_name(const char* name, HUI_EColor* role) {
	size_t i;
	for (i = 0; i < HUI_COUNT_OF(g_color_names); i++) {
		if (hui_stricmp(g_color_names[i].name, name) == 0) {
			*role = g_color_names[i].role;
			return HUI_TRUE;
		}
	}
	return HUI_FALSE;
}

static int hui_metric_by_name(const char* name, HUI_EMetric* metric) {
	size_t i;
	for (i = 0; i < HUI_COUNT_OF(g_metric_names); i++) {
		if (hui_stricmp(g_metric_names[i].name, name) == 0) {
			*metric = g_metric_names[i].metric;
			return HUI_TRUE;
		}
	}
	return HUI_FALSE;
}

/* ---------------------------------------------------------------------------
 *  Attributes shared by all widgets
 * ------------------------------------------------------------------------- */

static int hui_parse_cursor(const char* value, HUI_ECursor* cursor) {
	static const struct { const char* name; HUI_ECursor cursor; } cursors[] = {
		{ "Arrow", HUI_CURSOR_ARROW }, { "IBeam", HUI_CURSOR_IBEAM }, { "Hand", HUI_CURSOR_HAND },
		{ "Crosshair", HUI_CURSOR_CROSSHAIR }, { "ResizeH", HUI_CURSOR_RESIZE_H }, { "ResizeV", HUI_CURSOR_RESIZE_V },
		{ "ResizeNWSE", HUI_CURSOR_RESIZE_NWSE }, { "ResizeNESW", HUI_CURSOR_RESIZE_NESW }, { "Move", HUI_CURSOR_MOVE },
		{ "NotAllowed", HUI_CURSOR_NOT_ALLOWED }
	};
	size_t i;
	for (i = 0; i < HUI_COUNT_OF(cursors); i++) {
		if (hui_stricmp(value, cursors[i].name) == 0) {
			*cursor = cursors[i].cursor;
			return HUI_TRUE;
		}
	}
	return HUI_FALSE;
}

/* "Slot.Xxx": how the widget is placed by its parent. `name` is the part after the dot. */
static int hui_widget_set_slot_attribute(hui_widget_t* widget, const char* name, const char* value) {
	hui_slot_t* slot = &widget->slot;
	float values[4];
	int flag, count;

	if (HUI_ATTR("Padding")) {
		if (!hui_parse_margin(value, &slot->padding)) return HUI_FALSE;
	} else if (HUI_ATTR("HAlign")) {
		if (hui_stricmp(value, "Fill") == 0) slot->halign = HUI_HALIGN_FILL;
		else if (hui_stricmp(value, "Left") == 0) slot->halign = HUI_HALIGN_LEFT;
		else if (hui_stricmp(value, "Center") == 0) slot->halign = HUI_HALIGN_CENTER;
		else if (hui_stricmp(value, "Right") == 0) slot->halign = HUI_HALIGN_RIGHT;
		else return HUI_FALSE;
	} else if (HUI_ATTR("VAlign")) {
		if (hui_stricmp(value, "Fill") == 0) slot->valign = HUI_VALIGN_FILL;
		else if (hui_stricmp(value, "Top") == 0) slot->valign = HUI_VALIGN_TOP;
		else if (hui_stricmp(value, "Center") == 0) slot->valign = HUI_VALIGN_CENTER;
		else if (hui_stricmp(value, "Bottom") == 0) slot->valign = HUI_VALIGN_BOTTOM;
		else return HUI_FALSE;
	} else if (HUI_ATTR("Fill")) {
		if (!hui_parse_float(value, &values[0]) || values[0] < 0.0f) return HUI_FALSE;
		slot->is_fill = HUI_TRUE;
		slot->fill = values[0];
	} else if (HUI_ATTR("Auto")) {
		if (!hui_parse_bool(value, &flag)) return HUI_FALSE;
		slot->is_fill = !flag;
	} else if (HUI_ATTR("MaxSize")) {
		if (!hui_parse_float(value, &slot->max_size)) return HUI_FALSE;
	} else if (HUI_ATTR("Column")) {
		if (!hui_parse_int(value, &slot->column) || slot->column < 0) return HUI_FALSE;
	} else if (HUI_ATTR("Row")) {
		if (!hui_parse_int(value, &slot->row) || slot->row < 0) return HUI_FALSE;
	} else if (HUI_ATTR("ColumnSpan")) {
		if (!hui_parse_int(value, &slot->column_span) || slot->column_span < 1) return HUI_FALSE;
	} else if (HUI_ATTR("RowSpan")) {
		if (!hui_parse_int(value, &slot->row_span) || slot->row_span < 1) return HUI_FALSE;
	} else if (HUI_ATTR("Anchors")) {
		/* "x,y" pins the widget to a point, "minX,minY,maxX,maxY" stretches it */
		count = hui_parse_floats(value, values, 4);
		if (count == 2) {
			values[2] = values[0];
			values[3] = values[1];
		} else if (count != 4) {
			return HUI_FALSE;
		}
		memcpy(slot->anchors, values, sizeof(slot->anchors));
	} else if (HUI_ATTR("Position")) {
		if (hui_parse_floats(value, slot->position, 2) != 2) return HUI_FALSE;
	} else if (HUI_ATTR("Size")) {
		if (hui_parse_floats(value, slot->size, 2) != 2) return HUI_FALSE;
	} else if (HUI_ATTR("Alignment")) {
		if (hui_parse_floats(value, slot->alignment, 2) != 2) return HUI_FALSE;
	} else if (HUI_ATTR("Title")) {
		hui_str_set(&slot->title, value);
	} else {
		return HUI_FALSE;
	}
	hui_widget_invalidate_layout(widget->parent ? widget->parent : widget);
	return HUI_TRUE;
}

int hui_widget_set_base_attribute(hui_widget_t* widget, const char* name, const char* value) {
	hui_color_t color;
	HUI_EColor role;
	HUI_EMetric metric;
	float number;
	int flag;

	if (hui_strnicmp(name, "Slot.", 5) == 0) return hui_widget_set_slot_attribute(widget, name + 5, value);

	if (hui_strnicmp(name, "Color.", 6) == 0) {
		if (!hui_color_role_by_name(name + 6, &role) || !hui_parse_color(value, &color)) return HUI_FALSE;
		hui_widget_set_color(widget, role, color);
		return HUI_TRUE;
	}
	if (hui_strnicmp(name, "Metric.", 7) == 0) {
		if (!hui_metric_by_name(name + 7, &metric) || !hui_parse_float(value, &number)) return HUI_FALSE;
		hui_widget_set_metric(widget, metric, number);
		return HUI_TRUE;
	}

	if (HUI_ATTR("Name") || HUI_ATTR("Id")) {
		hui_str_set(&widget->name, value);
	} else if (HUI_ATTR("Visibility")) {
		HUI_EVisibility visibility;
		if (hui_stricmp(value, "Visible") == 0) visibility = HUI_VISIBLE;
		else if (hui_stricmp(value, "Hidden") == 0) visibility = HUI_HIDDEN;
		else if (hui_stricmp(value, "Collapsed") == 0) visibility = HUI_COLLAPSED;
		else return HUI_FALSE;
		if (visibility != widget->visibility) {
			widget->visibility = visibility;
			if (visibility != HUI_VISIBLE && widget->window) hui_window_forget_widget(widget->window, widget);
			hui_widget_invalidate_layout(widget->parent ? widget->parent : widget);
		}
	} else if (HUI_ATTR("Enabled")) {
		if (!hui_parse_bool(value, &flag)) return HUI_FALSE;
		if (flag) widget->flags &= ~HUI_WF_DISABLED;
		else widget->flags |= HUI_WF_DISABLED;
		hui_widget_invalidate_paint(widget);
	} else if (HUI_ATTR("HitTestable")) {
		if (!hui_parse_bool(value, &flag)) return HUI_FALSE;
		if (flag) widget->flags &= ~HUI_WF_NO_HIT;
		else widget->flags |= HUI_WF_NO_HIT;
	} else if (HUI_ATTR("Focusable")) {
		if (!hui_parse_bool(value, &flag)) return HUI_FALSE;
		if (flag) widget->flags |= HUI_WF_FOCUSABLE;
		else widget->flags &= ~HUI_WF_FOCUSABLE;
	} else if (HUI_ATTR("Clip")) {
		if (!hui_parse_bool(value, &flag)) return HUI_FALSE;
		if (flag) widget->flags |= HUI_WF_CLIP;
		else widget->flags &= ~HUI_WF_CLIP;
		hui_widget_invalidate_paint(widget);
	} else if (HUI_ATTR("Opacity")) {
		if (!hui_parse_float(value, &number)) return HUI_FALSE;
		widget->opacity = hui_clampf(number, 0.0f, 1.0f);
		hui_widget_invalidate_paint(widget);
	} else if (HUI_ATTR("Tooltip")) {
		hui_str_set(&widget->tooltip, value[0] ? value : NULL);
	} else if (HUI_ATTR("Cursor")) {
		if (!hui_parse_cursor(value, &widget->cursor)) return HUI_FALSE;
	} else if (HUI_ATTR("Width")) {
		if (!hui_parse_float(value, &widget->fixed_size.x)) return HUI_FALSE;
		hui_widget_invalidate_layout(widget);
	} else if (HUI_ATTR("Height")) {
		if (!hui_parse_float(value, &widget->fixed_size.y)) return HUI_FALSE;
		hui_widget_invalidate_layout(widget);
	} else if (HUI_ATTR("MinWidth")) {
		if (!hui_parse_float(value, &widget->min_size.x)) return HUI_FALSE;
		hui_widget_invalidate_layout(widget);
	} else if (HUI_ATTR("MinHeight")) {
		if (!hui_parse_float(value, &widget->min_size.y)) return HUI_FALSE;
		hui_widget_invalidate_layout(widget);
	} else if (HUI_ATTR("MaxWidth")) {
		if (!hui_parse_float(value, &widget->max_size.x)) return HUI_FALSE;
		hui_widget_invalidate_layout(widget);
	} else if (HUI_ATTR("MaxHeight")) {
		if (!hui_parse_float(value, &widget->max_size.y)) return HUI_FALSE;
		hui_widget_invalidate_layout(widget);
	} else if (HUI_ATTR("Action") || HUI_ATTR("OnClicked") || HUI_ATTR("OnChanged") ||
		HUI_ATTR("OnCommitted") || HUI_ATTR("OnActivated")) {
		hui_str_set(&widget->action, value[0] ? value : NULL);
	} else {
		return HUI_FALSE;
	}
	return HUI_TRUE;
}

int hui_widget_set_attribute(hui_widget_t* widget, const char* name, const char* value) {
	if (!name || !value) return HUI_FALSE;
	/* the class goes first: "Text", "Spacing"... may mean something specific for it */
	if (widget->cls->set_attribute && widget->cls->set_attribute(widget, name, value)) return HUI_TRUE;
	return hui_widget_set_base_attribute(widget, name, value);
}

int HUI_SetWidgetAttribute(HUI_id id, const char* name, const char* value) {
	HUI_WIDGET(widget, id, 0, HUI_FALSE);
	if (!name || !value) {
		hui_error(HUI_ERROR_INVALID_VALUE, HUI_SEVERITY_ERROR, "HUI_SetWidgetAttribute: NULL name or value");
		return HUI_FALSE;
	}
	return hui_widget_set_attribute(widget, name, value);
}
