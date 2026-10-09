/**
 * HUI - widget/registry.c
 * Table of the widget classes. A new widget type is listed here once.
 */

#include "../widgets/common.h"

static const hui_widget_class_t* const g_classes[] = {
	/* panels */
	&hui_class_vbox,
	&hui_class_hbox,
	&hui_class_grid,
	&hui_class_uniform_grid,
	&hui_class_overlay,
	&hui_class_border,
	&hui_class_size_box,
	&hui_class_spacer,
	&hui_class_scroll_box,
	&hui_class_splitter,
	&hui_class_wrap_box,
	&hui_class_canvas,
	&hui_class_switcher,
	&hui_class_tab_view,
	&hui_class_expander,
	/* controls */
	&hui_class_label,
	&hui_class_button,
	&hui_class_checkbox,
	&hui_class_slider,
	&hui_class_progress_bar,
	&hui_class_image,
	&hui_class_text_input,
	&hui_class_spin_box,
	&hui_class_combo_box,
	&hui_class_separator,
	&hui_class_list_view,
	&hui_class_tree_view,
	&hui_class_menu_bar,
	&hui_class_menu,
	&hui_class_menu_item,
	&hui_class_viewport,
	&hui_class_custom
};

/** Other tags accepted in XML, for people coming from Slate / UMG. */
static const struct { const char* alias; HUI_EWidgetType type; } g_aliases[] = {
	{ "VerticalBox", HUI_WIDGET_VBOX },
	{ "HorizontalBox", HUI_WIDGET_HBOX },
	{ "GridPanel", HUI_WIDGET_GRID },
	{ "UniformGridPanel", HUI_WIDGET_UNIFORM_GRID },
	{ "Box", HUI_WIDGET_SIZE_BOX },
	{ "CanvasPanel", HUI_WIDGET_CANVAS },
	{ "WidgetSwitcher", HUI_WIDGET_SWITCHER },
	{ "Tabs", HUI_WIDGET_TAB_VIEW },
	{ "ExpandableArea", HUI_WIDGET_EXPANDER },
	{ "Text", HUI_WIDGET_LABEL },
	{ "TextBlock", HUI_WIDGET_LABEL },
	{ "CheckBox", HUI_WIDGET_CHECKBOX },
	{ "EditableText", HUI_WIDGET_TEXT_INPUT },
	{ "TextBox", HUI_WIDGET_TEXT_INPUT }
};

const hui_widget_class_t* hui_widget_class_by_type(HUI_EWidgetType type) {
	size_t i;
	for (i = 0; i < HUI_COUNT_OF(g_classes); i++) {
		if (g_classes[i]->type == type) return g_classes[i];
	}
	return NULL;
}

const hui_widget_class_t* hui_widget_class_by_name(const char* name) {
	size_t i;
	if (!name) return NULL;
	for (i = 0; i < HUI_COUNT_OF(g_classes); i++) {
		if (hui_stricmp(g_classes[i]->name, name) == 0) return g_classes[i];
	}
	for (i = 0; i < HUI_COUNT_OF(g_aliases); i++) {
		if (hui_stricmp(g_aliases[i].alias, name) == 0) return hui_widget_class_by_type(g_aliases[i].type);
	}
	return NULL;
}
