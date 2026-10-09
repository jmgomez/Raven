/**
 * HUI - widgets/overlay.c
 * Overlay (Slate SOverlay): children drawn on top of each other, each one
 * aligned in the whole panel by its slot. Spacer: empty space.
 *
 * Both rely entirely on the default measure / arrange of the widget core:
 * the desired size is the largest child, and every child is placed in the
 * full bounds according to its slot padding and alignment.
 */

#include "common.h"

const hui_widget_class_t hui_class_overlay = {
	.type = HUI_WIDGET_OVERLAY,
	.name = "Overlay",
	.size = sizeof(hui_widget_t),
	.flags = HUI_CF_NO_HIT
};

/* A spacer has no content: its size comes from HUI_SetWidgetSize / the fill slot. */
const hui_widget_class_t hui_class_spacer = {
	.type = HUI_WIDGET_SPACER,
	.name = "Spacer",
	.size = sizeof(hui_widget_t),
	.flags = HUI_CF_NO_HIT | HUI_CF_NO_CHILDREN
};

void HUI_SetSpacerSize(HUI_id id, float width, float height) {
	HUI_WIDGET_VOID(widget, id, HUI_WIDGET_SPACER);
	widget->fixed_size = hui_vec2(width, height);
	hui_widget_invalidate_layout(widget);
}
