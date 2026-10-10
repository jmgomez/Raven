// Raven for C++.
//
// Generated from the C headers by tools/bindgen (Raven 0.1): do not edit.
//
// The C API is used as it is (same enumerations, same structures); this header
// adds, in the namespace raven:
//
//   - a class for each kind of handle (Window, Widget, Button, TextEditor,
//     XmlNode...), whose member functions are the C functions of that object:
//     Rbutton_set_text(button, "OK") is button.set_text("OK"). A class is the
//     size of a handle and converts to and from Rid; it does not own what it
//     names (call destroy(), as in C);
//   - an overload of every function that takes a callback, taking anything
//     callable instead (a lambda with captures): the last parameter of the C
//     callback, user_data, is not passed to it. The callable is kept until
//     another one replaces it for the same object, or until the program ends;
//   - a Painter around RPainter*, and every other function as raven::name
//     (Rdraw_frame is raven::draw_frame).
//
//   #include <raven.hpp>
//
//   raven::init(R_BACKEND_VULKAN);
//   auto window = raven::Window::create("Hello", 400, 200, R_WINDOW_DEFAULT);
//   auto button = raven::Button::create("Quit");
//   button.set_clicked_callback([](Rid) { raven::quit(0); });
//   window.set_content(button);
//   while (raven::wait_frame(-1.0)) {}
//   raven::shutdown();

#ifndef RAVEN_HPP
#define RAVEN_HPP

#include <Raven.h>

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <utility>

namespace raven {

namespace detail {

// The callables given to the library: one per (object, function), replaced when set again.
struct Kept {
	void* callable = nullptr;
	void (*destroy)(void*) = nullptr;
};

struct Registry {
	std::unordered_map<std::uint64_t, Kept> map;
	~Registry() {
		for (auto& entry : map) {
			if (entry.second.callable) entry.second.destroy(entry.second.callable);
		}
	}
};

inline std::unordered_map<std::uint64_t, Kept>& kept() {
	static Registry registry;
	return registry.map;
}

template <typename Function>
inline void* keep(std::uint64_t object, std::uint32_t slot, Function function) {
	Kept& entry = kept()[(object << 16) ^ slot];
	if (entry.callable) entry.destroy(entry.callable);
	entry.callable = new Function(std::move(function));
	entry.destroy = [](void* callable) { delete static_cast<Function*>(callable); };
	return entry.callable;
}

inline std::uint64_t key_of(const char* text) {
	std::uint64_t hash = 1469598103934665603ull;
	for (; text && *text; text++) hash = (hash ^ static_cast<unsigned char>(*text)) * 1099511628211ull;
	return hash >> 16;
}

} // namespace detail

/// A handle of the library. Converts to and from Rid; false when it names nothing.
class Handle {
public:
	constexpr Handle() noexcept : id_(R_INVALID_ID) {}
	constexpr Handle(Rid id) noexcept : id_(id) {}
	constexpr Rid id() const noexcept { return id_; }
	constexpr operator Rid() const noexcept { return id_; }
	constexpr explicit operator bool() const noexcept { return id_ != R_INVALID_ID; }

protected:
	Rid id_;
};

class Widget : public Handle {
public:
	using Handle::Handle;
	constexpr Widget() noexcept = default;

	/// Creates a widget. It is not shown until it is added to a parent
	/// that belongs to a window (Rwidget_add_child, Rwindow_set_content).
	static Widget create(RWidgetType type) { return Widget(Rwidget_create(type)); }

	/// Deletes a widget and all its descendants.
	void destroy() const { Rwidget_destroy(id_); }

	int is_valid() const { return Rwidget_is_valid(id_); }

	RWidgetType get_type() const { return Rwidget_get_type(id_); }

	/// Appends a child to a panel. The parent takes ownership.
	/// A widget that already has a parent is moved.
	/// Returns: `child`, so calls can be nested:
	///         Rid ok = Rwidget_add_child(row, Rbutton_create("OK"));
	Rid add_child(Rid child) const { return Rwidget_add_child(id_, child); }

	Rid insert_child(Rid child, int index) const { return Rwidget_insert_child(id_, child, index); }

	/// Detaches a child without deleting it. The caller owns it again.
	void remove_child(Rid child) const { Rwidget_remove_child(id_, child); }

	/// Deletes every child of a panel.
	void clear_children() const { Rwidget_clear_children(id_); }

	Rid get_parent() const { return Rwidget_get_parent(id_); }

	int get_child_count() const { return Rwidget_get_child_count(id_); }

	Rid get_child(int index) const { return Rwidget_get_child(id_, index); }

	/// Returns: The window showing the widget, or R_INVALID_ID.
	Rid get_window() const { return Rwidget_get_window(id_); }

	void set_name(const char* name) const { Rwidget_set_name(id_, name); }

	const char* get_name() const { return Rwidget_get_name(id_); }

	/// Finds a widget by name.
	/// scope A window or a widget; the search covers its whole subtree.
	/// Returns: R_INVALID_ID when no widget has that name.
	Rid find(const char* name) const { return Rwidget_find(id_, name); }

	void set_visibility(RVisibility visibility) const { Rwidget_set_visibility(id_, visibility); }

	RVisibility get_visibility() const { return Rwidget_get_visibility(id_); }

	/// A disabled widget ignores input and is drawn dimmed, with its descendants.
	void set_enabled(int enabled) const { Rwidget_set_enabled(id_, enabled); }

	int is_enabled() const { return Rwidget_is_enabled(id_); }

	/// R_FALSE lets mouse events pass through the widget (not its children).
	void set_hit_testable(int hit_testable) const { Rwidget_set_hit_testable(id_, hit_testable); }

	/// Opacity multiplier applied to the widget and its descendants (0..1).
	void set_opacity(float opacity) const { Rwidget_set_opacity(id_, opacity); }

	/// Gives the widget a transition that plays each time it appears:
	/// when its window is first drawn, when it is added to a window that is
	/// shown, and when it becomes visible again (Rwidget_set_visibility, a
	/// tab or a switcher showing it). In XML: Appear="FlyUp" AppearDelay="0.1".
	/// delay_seconds Time before it starts. Give growing delays to the
	/// items of a group to make them arrive one after the other.
	void set_transition(RTransition transition, float delay_seconds) const { Rwidget_set_transition(id_, transition, delay_seconds); }

	/// Plays a transition once, now, whatever the widget was given with Rwidget_set_transition.
	void play_transition(RTransition transition, float delay_seconds) const { Rwidget_play_transition(id_, transition, delay_seconds); }

	/// R_TRUE clips the descendants to the bounds of the widget.
	void set_clipping(int clip) const { Rwidget_set_clipping(id_, clip); }

	/// Text shown when the cursor rests on the widget. NULL removes it.
	void set_tooltip(const char* text) const { Rwidget_set_tooltip(id_, text); }

	/// Returns: The tooltip text of the widget, "" when it has none.
	const char* get_tooltip() const { return Rwidget_get_tooltip(id_); }

	void set_cursor(RCursor cursor) const { Rwidget_set_cursor(id_, cursor); }

	/// Size constraints, in logical units. They apply to the desired size the
	/// widget reports to its parent. A negative value removes the constraint
	/// on that axis.
	void set_size(float width, float height) const { Rwidget_set_size(id_, width, height); }

	void set_min_size(float width, float height) const { Rwidget_set_min_size(id_, width, height); }

	void set_max_size(float width, float height) const { Rwidget_set_max_size(id_, width, height); }

	/// Bounds of the widget in its window, in logical units, after the last layout.
	void get_rect(float* x, float* y, float* width, float* height) const { Rwidget_get_rect(id_, x, y, width, height); }

	void get_desired_size(float* width, float* height) const { Rwidget_get_desired_size(id_, width, height); }

	int is_hovered() const { return Rwidget_is_hovered(id_); }

	void set_focusable(int focusable) const { Rwidget_set_focusable(id_, focusable); }

	/// Gives the keyboard focus to the widget.
	void focus() const { Rwidget_focus(id_); }

	int is_focused() const { return Rwidget_is_focused(id_); }

	void set_user_handle(void* handle) const { Rwidget_set_user_handle(id_, handle); }

	void* get_user_handle() const { return Rwidget_get_user_handle(id_); }

	void set_event_callback(RWidgetEventCallback callback, void* user_data) const { Rwidget_set_event_callback(id_, callback, user_data); }
	void set_event_callback(std::function<int(Rid, const REvent*)> callback) const {
		Rwidget_set_event_callback(id_, [](Rid widget, const REvent* event, void* kept_) -> int { return (*static_cast<std::function<int(Rid, const REvent*)>*>(kept_))(widget, event); }, detail::keep(id_, 1, std::move(callback)));
	}

	/// Name of the action fired by the main event of the widget: button
	/// click, value change, text commit, item activation (see Raction_bind).
	void set_action(const char* action) const { Rwidget_set_action(id_, action); }

	const char* get_action() const { return Rwidget_get_action(id_); }

	/// Makes the widget a target for what is dragged from other
	/// applications: files from the file manager, text. It is highlighted
	/// while a drop on it would be accepted. NULL removes the callback.
	void set_drop_callback(RDropCallback callback, void* user_data) const { Rwidget_set_drop_callback(id_, callback, user_data); }
	void set_drop_callback(std::function<int(Rid, const RDropEvent*)> callback) const {
		Rwidget_set_drop_callback(id_, [](Rid target, const RDropEvent* event, void* kept_) -> int { return (*static_cast<std::function<int(Rid, const RDropEvent*)>*>(kept_))(target, event); }, detail::keep(id_, 2, std::move(callback)));
	}

	/// In a window created with R_WINDOW_CUSTOM_HEADER: dragging the
	/// widget moves the window and a double click maximizes it, as a title bar
	/// does. The buttons, fields and other controls inside it keep working.
	/// In XML: WindowDrag="true".
	void set_window_drag(int enabled) const { Rwidget_set_window_drag(id_, enabled); }

	/// Deepest widget of a subtree under a point of its window, or R_INVALID_ID.
	Rid hit_test(float x, float y) const { return Rwidget_hit_test(id_, x, y); }

	/// Deepest visible widget of a subtree under a point of its window,
	/// whether it reacts to the pointer or not: a label, a panel, a disabled
	/// button. What a design tool selects with a click.
	Rid pick(float x, float y) const { return Rwidget_pick(id_, x, y); }

	/// Menu opened by a right click on the widget. The widget does not own it.
	void set_context_menu(Rid menu) const { Rwidget_set_context_menu(id_, menu); }

	/// Asks for the widget to be laid out and painted again.
	void invalidate() const { Rwidget_invalidate(id_); }

	/// Overrides a theme color for one widget. R_COLOR_TEXT is also
	/// inherited by its descendants.
	void set_color(RColorRole role, float r, float g, float b, float a) const { Rwidget_set_color(id_, role, r, g, b, a); }

	void clear_color(RColorRole role) const { Rwidget_clear_color(id_, role); }

	/// Gives one widget a material of its own, whatever the theme:
	/// a solid button in a glass window, a panel of clear glass in a flat one.
	/// A widget that draws no surface (a box, a label) gets one of this
	/// material behind it: any widget can become a panel of glass.
	/// In XML: Material="Glass | FrostedGlass | TintedGlass | Solid".
	/// material R_MATERIAL_DEFAULT follows the theme again.
	void set_material(RMaterial material) const { Rwidget_set_material(id_, material); }

	/// Returns: The material the widget is drawn with: its own, or the one of the theme.
	RMaterial get_material() const { return Rwidget_get_material(id_); }

	/// Overrides a theme metric for one widget.
	void set_metric(RMetric metric, float value) const { Rwidget_set_metric(id_, metric, value); }

	void clear_metric(RMetric metric) const { Rwidget_clear_metric(id_, metric); }

	/// Scrolls every ScrollBox above the widget so that the widget is visible.
	void scroll_into_view() const { Rwidget_scroll_into_view(id_); }

	/// Creates a widget tree from a document whose root element is a
	/// widget. The caller owns the returned widget until it is added to a parent.
	static Widget create_from_xml(const char* xml, size_t size) { return Widget(Rwidget_create_from_xml(xml, size)); }

	static Widget create_from_xml_file(const char* path) { return Widget(Rwidget_create_from_xml_file(path)); }

	/// Sets a property from text, with the names and value syntax of the
	/// XML attributes. Handy for scripting and data-driven tools.
	/// Returns: R_TRUE when the attribute exists for this widget and the value is valid.
	int set_attribute(const char* name, const char* value) const { return Rwidget_set_attribute(id_, name, value); }

	/// Returns: The node a widget was built from, or R_INVALID_ID: not built
	/// from a document, or the document is gone. The widgets inside a tag
	/// declared with <Widget> all come from the node where the tag is used.
	Rid get_xml_node() const { return Rwidget_get_xml_node(id_); }

	/// Space kept around the child, inside its slot. All panels.
	void slot_set_padding(float left, float top, float right, float bottom) const { Rslot_set_padding(id_, left, top, right, bottom); }

	/// Alignment of the child inside its slot. Default: fill. All panels.
	void slot_set_halign(RHAlign align) const { Rslot_set_halign(id_, align); }

	void slot_set_valign(RVAlign align) const { Rslot_set_valign(id_, align); }

	/// The slot takes the desired size of the child along the panel
	/// direction (Slate AutoWidth / AutoHeight). This is the default.
	/// VBox, HBox.
	void slot_set_auto() const { Rslot_set_auto(id_); }

	/// The slot shares the space left by the auto slots, in proportion
	/// to `coefficient` (Slate FillWidth / FillHeight). VBox, HBox.
	/// In a Splitter, the coefficient is the relative size of the pane.
	void slot_set_fill(float coefficient) const { Rslot_set_fill(id_, coefficient); }

	/// Upper bound of the slot along the panel direction. 0 = none. VBox, HBox.
	void slot_set_max_size(float max_size) const { Rslot_set_max_size(id_, max_size); }

	/// Cell of the child. Grid, UniformGrid.
	void slot_set_grid(int column, int row) const { Rslot_set_grid(id_, column, row); }

	void slot_set_grid_span(int column_span, int row_span) const { Rslot_set_grid_span(id_, column_span, row_span); }

	/// Canvas placement. Anchors are fractions of the canvas size (0..1).
	///   - anchors equal on an axis: the child is at anchor + position, with
	///     the slot size (or its desired size when the slot size is <= 0);
	///   - anchors different on an axis: the child stretches between them,
	///     position and size are then the near and far margins.
	/// Alignment is the pivot of the child (0,0 = top-left, 0.5,0.5 = center).
	void slot_set_anchors(float min_x, float min_y, float max_x, float max_y) const { Rslot_set_anchors(id_, min_x, min_y, max_x, max_y); }

	void slot_set_position(float x, float y) const { Rslot_set_position(id_, x, y); }

	void slot_set_size(float width, float height) const { Rslot_set_size(id_, width, height); }

	void slot_set_alignment(float x, float y) const { Rslot_set_alignment(id_, x, y); }

	/// Title of the tab showing the child. TabView.
	void slot_set_title(const char* title) const { Rslot_set_title(id_, title); }
};

class Label : public Widget {
public:
	using Widget::Widget;
	constexpr Label() noexcept = default;
	/// The same widget, seen as a Label: nothing is checked.
	constexpr explicit Label(const Handle& other) noexcept : Widget(other.id()) {}

	/// Shortcut for Rwidget_create(R_WIDGET_LABEL) + Rlabel_set_text.
	static Label create(const char* text) { return Label(Rlabel_create(text)); }

	/// Sets the text. '\n' starts a new line.
	void set_text(const char* text) const { Rlabel_set_text(id_, text); }

	const char* get_text() const { return Rlabel_get_text(id_); }

	/// R_INVALID_ID uses the theme font.
	void set_font(Rid font) const { Rlabel_set_font(id_, font); }

	/// Font size in logical units. 0 uses R_METRIC_FONT_SIZE.
	void set_text_size(float size) const { Rlabel_set_text_size(id_, size); }

	void set_text_align(RTextAlign align) const { Rlabel_set_text_align(id_, align); }

	/// R_TRUE wraps the text on the width given by the parent.
	void set_wrap(int wrap) const { Rlabel_set_wrap(id_, wrap); }

	/// Text color (R_COLOR_TEXT of this widget).
	void set_color(float r, float g, float b, float a) const { Rlabel_set_color(id_, r, g, b, a); }
};

class Button : public Widget {
public:
	using Widget::Widget;
	constexpr Button() noexcept = default;
	/// The same widget, seen as a Button: nothing is checked.
	constexpr explicit Button(const Handle& other) noexcept : Widget(other.id()) {}

	/// Shortcut for Rwidget_create(R_WIDGET_BUTTON) + Rbutton_set_text.
	static Button create(const char* text) { return Button(Rbutton_create(text)); }

	/// Sets the content of the button to a label. A button can instead
	/// hold any single child widget (Rwidget_add_child), e.g. an HBox with an icon.
	void set_text(const char* text) const { Rbutton_set_text(id_, text); }

	void set_style(RButtonStyle style) const { Rbutton_set_style(id_, style); }

	void set_clicked_callback(RButtonClickedCallback callback, void* user_data) const { Rbutton_set_clicked_callback(id_, callback, user_data); }
	void set_clicked_callback(std::function<void(Rid)> callback) const {
		Rbutton_set_clicked_callback(id_, [](Rid button, void* kept_) -> void { return (*static_cast<std::function<void(Rid)>*>(kept_))(button); }, detail::keep(id_, 3, std::move(callback)));
	}
};

class Checkbox : public Widget {
public:
	using Widget::Widget;
	constexpr Checkbox() noexcept = default;
	/// The same widget, seen as a Checkbox: nothing is checked.
	constexpr explicit Checkbox(const Handle& other) noexcept : Widget(other.id()) {}

	static Checkbox create(const char* text) { return Checkbox(Rcheckbox_create(text)); }

	void set_text(const char* text) const { Rcheckbox_set_text(id_, text); }

	void set_checked(int checked) const { Rcheckbox_set_checked(id_, checked); }

	int is_checked() const { return Rcheckbox_is_checked(id_); }

	void set_style(RCheckboxStyle style) const { Rcheckbox_set_style(id_, style); }

	void set_changed_callback(RCheckboxChangedCallback callback, void* user_data) const { Rcheckbox_set_changed_callback(id_, callback, user_data); }
	void set_changed_callback(std::function<void(Rid, int)> callback) const {
		Rcheckbox_set_changed_callback(id_, [](Rid checkbox, int checked, void* kept_) -> void { return (*static_cast<std::function<void(Rid, int)>*>(kept_))(checkbox, checked); }, detail::keep(id_, 4, std::move(callback)));
	}
};

class Slider : public Widget {
public:
	using Widget::Widget;
	constexpr Slider() noexcept = default;
	/// The same widget, seen as a Slider: nothing is checked.
	constexpr explicit Slider(const Handle& other) noexcept : Widget(other.id()) {}

	static Slider create() { return Slider(Rwidget_create(R_WIDGET_SLIDER)); }

	void set_range(float minimum, float maximum) const { Rslider_set_range(id_, minimum, maximum); }

	void set_value(float value) const { Rslider_set_value(id_, value); }

	float get_value() const { return Rslider_get_value(id_); }

	/// Snaps the value to multiples of `step`. 0 = continuous (default).
	void set_step(float step) const { Rslider_set_step(id_, step); }

	void set_orientation(ROrientation orientation) const { Rslider_set_orientation(id_, orientation); }

	void set_changed_callback(RSliderChangedCallback callback, void* user_data) const { Rslider_set_changed_callback(id_, callback, user_data); }
	void set_changed_callback(std::function<void(Rid, float)> callback) const {
		Rslider_set_changed_callback(id_, [](Rid slider, float value, void* kept_) -> void { return (*static_cast<std::function<void(Rid, float)>*>(kept_))(slider, value); }, detail::keep(id_, 5, std::move(callback)));
	}
};

class ProgressBar : public Widget {
public:
	using Widget::Widget;
	constexpr ProgressBar() noexcept = default;
	/// The same widget, seen as a ProgressBar: nothing is checked.
	constexpr explicit ProgressBar(const Handle& other) noexcept : Widget(other.id()) {}

	static ProgressBar create() { return ProgressBar(Rwidget_create(R_WIDGET_PROGRESSBAR)); }

	/// Progress between 0 and 1.
	void set_value(float value) const { Rprogress_bar_set_value(id_, value); }

	float get_value() const { return Rprogress_bar_get_value(id_); }
};

class Image : public Widget {
public:
	using Widget::Widget;
	constexpr Image() noexcept = default;
	/// The same widget, seen as a Image: nothing is checked.
	constexpr explicit Image(const Handle& other) noexcept : Widget(other.id()) {}

	static Image create() { return Image(Rwidget_create(R_WIDGET_IMAGE)); }

	void set_texture(Rid texture) const { Rimage_set_texture(id_, texture); }

	void set_tint_color(float r, float g, float b, float a) const { Rimage_set_tint_color(id_, r, g, b, a); }

	void set_stretch(RStretch stretch) const { Rimage_set_stretch(id_, stretch); }

	/// Part of the texture to show, in UV coordinates. Default: 0, 0, 1, 1.
	void set_region(float min_u, float min_v, float max_u, float max_v) const { Rimage_set_region(id_, min_u, min_v, max_u, max_v); }

	void set_corner_radius(float radius) const { Rimage_set_corner_radius(id_, radius); }
};

class TextInput : public Widget {
public:
	using Widget::Widget;
	constexpr TextInput() noexcept = default;
	/// The same widget, seen as a TextInput: nothing is checked.
	constexpr explicit TextInput(const Handle& other) noexcept : Widget(other.id()) {}

	static TextInput create() { return TextInput(Rwidget_create(R_WIDGET_TEXT_INPUT)); }

	void set_text(const char* text) const { Rtext_input_set_text(id_, text); }

	const char* get_text() const { return Rtext_input_get_text(id_); }

	/// Dimmed text shown while the input is empty.
	void set_placeholder(const char* text) const { Rtext_input_set_placeholder(id_, text); }

	void set_password(int password) const { Rtext_input_set_password(id_, password); }

	void set_read_only(int read_only) const { Rtext_input_set_read_only(id_, read_only); }

	/// Maximum number of characters. 0 = no limit (default).
	void set_max_length(int max_length) const { Rtext_input_set_max_length(id_, max_length); }

	void select_all() const { Rtext_input_select_all(id_); }

	/// Called on every edit.
	void set_changed_callback(RTextChangedCallback callback, void* user_data) const { Rtext_input_set_changed_callback(id_, callback, user_data); }
	void set_changed_callback(std::function<void(Rid, const char*)> callback) const {
		Rtext_input_set_changed_callback(id_, [](Rid text_input, const char* text, void* kept_) -> void { return (*static_cast<std::function<void(Rid, const char*)>*>(kept_))(text_input, text); }, detail::keep(id_, 6, std::move(callback)));
	}

	/// Called when Enter is pressed. This also fires the action of the widget.
	void set_committed_callback(RTextChangedCallback callback, void* user_data) const { Rtext_input_set_committed_callback(id_, callback, user_data); }
	void set_committed_callback(std::function<void(Rid, const char*)> callback) const {
		Rtext_input_set_committed_callback(id_, [](Rid text_input, const char* text, void* kept_) -> void { return (*static_cast<std::function<void(Rid, const char*)>*>(kept_))(text_input, text); }, detail::keep(id_, 7, std::move(callback)));
	}
};

class SpinBox : public Widget {
public:
	using Widget::Widget;
	constexpr SpinBox() noexcept = default;
	/// The same widget, seen as a SpinBox: nothing is checked.
	constexpr explicit SpinBox(const Handle& other) noexcept : Widget(other.id()) {}

	static SpinBox create() { return SpinBox(Rwidget_create(R_WIDGET_SPIN_BOX)); }

	void set_range(double minimum, double maximum) const { Rspin_box_set_range(id_, minimum, maximum); }

	void set_value(double value) const { Rspin_box_set_value(id_, value); }

	double get_value() const { return Rspin_box_get_value(id_); }

	void set_step(double step) const { Rspin_box_set_step(id_, step); }

	/// Digits shown after the decimal point. Default: 2.
	void set_decimals(int decimals) const { Rspin_box_set_decimals(id_, decimals); }

	void set_changed_callback(RSpinBoxChangedCallback callback, void* user_data) const { Rspin_box_set_changed_callback(id_, callback, user_data); }
	void set_changed_callback(std::function<void(Rid, double)> callback) const {
		Rspin_box_set_changed_callback(id_, [](Rid spin_box, double value, void* kept_) -> void { return (*static_cast<std::function<void(Rid, double)>*>(kept_))(spin_box, value); }, detail::keep(id_, 8, std::move(callback)));
	}
};

class ComboBox : public Widget {
public:
	using Widget::Widget;
	constexpr ComboBox() noexcept = default;
	/// The same widget, seen as a ComboBox: nothing is checked.
	constexpr explicit ComboBox(const Handle& other) noexcept : Widget(other.id()) {}

	static ComboBox create() { return ComboBox(Rwidget_create(R_WIDGET_COMBO_BOX)); }

	/// Returns: The index of the new item.
	int add_item(const char* text) const { return Rcombo_box_add_item(id_, text); }

	void clear_items() const { Rcombo_box_clear_items(id_); }

	int get_item_count() const { return Rcombo_box_get_item_count(id_); }

	const char* get_item_text(int index) const { return Rcombo_box_get_item_text(id_, index); }

	/// Selects an item. -1 selects nothing.
	void set_selected(int index) const { Rcombo_box_set_selected(id_, index); }

	int get_selected() const { return Rcombo_box_get_selected(id_); }

	void set_changed_callback(RComboBoxChangedCallback callback, void* user_data) const { Rcombo_box_set_changed_callback(id_, callback, user_data); }
	void set_changed_callback(std::function<void(Rid, int)> callback) const {
		Rcombo_box_set_changed_callback(id_, [](Rid combo_box, int index, void* kept_) -> void { return (*static_cast<std::function<void(Rid, int)>*>(kept_))(combo_box, index); }, detail::keep(id_, 9, std::move(callback)));
	}
};

class Separator : public Widget {
public:
	using Widget::Widget;
	constexpr Separator() noexcept = default;
	/// The same widget, seen as a Separator: nothing is checked.
	constexpr explicit Separator(const Handle& other) noexcept : Widget(other.id()) {}

	static Separator create() { return Separator(Rwidget_create(R_WIDGET_SEPARATOR)); }

	/// R_HORIZONTAL draws a horizontal line (default).
	void set_orientation(ROrientation orientation) const { Rseparator_set_orientation(id_, orientation); }
};

class ListView : public Widget {
public:
	using Widget::Widget;
	constexpr ListView() noexcept = default;
	/// The same widget, seen as a ListView: nothing is checked.
	constexpr explicit ListView(const Handle& other) noexcept : Widget(other.id()) {}

	static ListView create() { return ListView(Rwidget_create(R_WIDGET_LIST_VIEW)); }

	/// Returns: The index of the new item.
	int add_item(const char* text) const { return Rlist_view_add_item(id_, text); }

	void set_item_text(int index, const char* text) const { Rlist_view_set_item_text(id_, index, text); }

	const char* get_item_text(int index) const { return Rlist_view_get_item_text(id_, index); }

	void remove_item(int index) const { Rlist_view_remove_item(id_, index); }

	void clear_items() const { Rlist_view_clear_items(id_); }

	int get_item_count() const { return Rlist_view_get_item_count(id_); }

	void set_virtual_items(int count, RListItemTextCallback callback, void* user_data) const { Rlist_view_set_virtual_items(id_, count, callback, user_data); }
	void set_virtual_items(int count, std::function<const char*(Rid, int)> callback) const {
		Rlist_view_set_virtual_items(id_, count, [](Rid list_view, int index, void* kept_) -> const char* { return (*static_cast<std::function<const char*(Rid, int)>*>(kept_))(list_view, index); }, detail::keep(id_, 10, std::move(callback)));
	}

	/// Selects a row. -1 selects nothing.
	void set_selected(int index) const { Rlist_view_set_selected(id_, index); }

	int get_selected() const { return Rlist_view_get_selected(id_); }

	void scroll_to_item(int index) const { Rlist_view_scroll_to_item(id_, index); }

	void set_selection_changed_callback(RListEventCallback callback, void* user_data) const { Rlist_view_set_selection_changed_callback(id_, callback, user_data); }
	void set_selection_changed_callback(std::function<void(Rid, int)> callback) const {
		Rlist_view_set_selection_changed_callback(id_, [](Rid list_view, int index, void* kept_) -> void { return (*static_cast<std::function<void(Rid, int)>*>(kept_))(list_view, index); }, detail::keep(id_, 11, std::move(callback)));
	}

	/// Called on double click or Enter. This also fires the action of the widget.
	void set_activated_callback(RListEventCallback callback, void* user_data) const { Rlist_view_set_activated_callback(id_, callback, user_data); }
	void set_activated_callback(std::function<void(Rid, int)> callback) const {
		Rlist_view_set_activated_callback(id_, [](Rid list_view, int index, void* kept_) -> void { return (*static_cast<std::function<void(Rid, int)>*>(kept_))(list_view, index); }, detail::keep(id_, 12, std::move(callback)));
	}
};

class TreeView : public Widget {
public:
	using Widget::Widget;
	constexpr TreeView() noexcept = default;
	/// The same widget, seen as a TreeView: nothing is checked.
	constexpr explicit TreeView(const Handle& other) noexcept : Widget(other.id()) {}

	static TreeView create() { return TreeView(Rwidget_create(R_WIDGET_TREE_VIEW)); }

	/// Adds an item.
	/// parent_item Item to add under, or R_INVALID_ID for a top-level item.
	/// Returns: The handle of the new item.
	Rid add_item(Rid parent_item, const char* text) const { return Rtree_view_add_item(id_, parent_item, text); }

	void clear_items() const { Rtree_view_clear_items(id_); }

	/// Top-level items of a tree.
	int get_item_count() const { return Rtree_view_get_item_count(id_); }

	Rid get_item(int index) const { return Rtree_view_get_item(id_, index); }

	/// Selects an item. R_INVALID_ID selects nothing.
	void set_selected_item(Rid item) const { Rtree_view_set_selected_item(id_, item); }

	Rid get_selected_item() const { return Rtree_view_get_selected_item(id_); }

	void set_selection_changed_callback(RTreeEventCallback callback, void* user_data) const { Rtree_view_set_selection_changed_callback(id_, callback, user_data); }
	void set_selection_changed_callback(std::function<void(Rid, Rid)> callback) const {
		Rtree_view_set_selection_changed_callback(id_, [](Rid tree_view, Rid item, void* kept_) -> void { return (*static_cast<std::function<void(Rid, Rid)>*>(kept_))(tree_view, item); }, detail::keep(id_, 13, std::move(callback)));
	}

	/// Called on double click or Enter. This also fires the action of the widget.
	void set_activated_callback(RTreeEventCallback callback, void* user_data) const { Rtree_view_set_activated_callback(id_, callback, user_data); }
	void set_activated_callback(std::function<void(Rid, Rid)> callback) const {
		Rtree_view_set_activated_callback(id_, [](Rid tree_view, Rid item, void* kept_) -> void { return (*static_cast<std::function<void(Rid, Rid)>*>(kept_))(tree_view, item); }, detail::keep(id_, 14, std::move(callback)));
	}
};

class MenuItem : public Widget {
public:
	using Widget::Widget;
	constexpr MenuItem() noexcept = default;
	/// The same widget, seen as a MenuItem: nothing is checked.
	constexpr explicit MenuItem(const Handle& other) noexcept : Widget(other.id()) {}

	static MenuItem create() { return MenuItem(Rwidget_create(R_WIDGET_MENU_ITEM)); }

	void set_text(const char* text) const { Rmenu_item_set_text(id_, text); }

	/// Shows a shortcut next to the item and triggers the item when the
	/// keys are pressed in the window of its menu bar.
	void set_shortcut(RKey key, Ruint modifiers) const { Rmenu_item_set_shortcut(id_, key, modifiers); }

	void set_checkable(int checkable) const { Rmenu_item_set_checkable(id_, checkable); }

	void set_checked(int checked) const { Rmenu_item_set_checked(id_, checked); }

	int is_checked() const { return Rmenu_item_is_checked(id_); }

	void set_clicked_callback(RMenuItemClickedCallback callback, void* user_data) const { Rmenu_item_set_clicked_callback(id_, callback, user_data); }
	void set_clicked_callback(std::function<void(Rid)> callback) const {
		Rmenu_item_set_clicked_callback(id_, [](Rid item, void* kept_) -> void { return (*static_cast<std::function<void(Rid)>*>(kept_))(item); }, detail::keep(id_, 15, std::move(callback)));
	}
};

class Menu : public Widget {
public:
	using Widget::Widget;
	constexpr Menu() noexcept = default;
	/// The same widget, seen as a Menu: nothing is checked.
	constexpr explicit Menu(const Handle& other) noexcept : Widget(other.id()) {}

	static Menu create() { return Menu(Rwidget_create(R_WIDGET_MENU)); }

	/// Creates a menu inside a menu bar or as a sub-menu of another menu.
	Rid add_menu(const char* title) const { return Rmenu_add_menu(id_, title); }

	void set_title(const char* title) const { Rmenu_set_title(id_, title); }

	Rid add_item(const char* text) const { return Rmenu_add_item(id_, text); }

	Rid add_separator() const { return Rmenu_add_separator(id_); }

	/// Opens a menu as a popup at a position of the window (logical units).
	void open(Rid window, float x, float y) const { Rmenu_open(id_, window, x, y); }
};

class Panel : public Widget {
public:
	using Widget::Widget;
	constexpr Panel() noexcept = default;
	/// The same widget, seen as a Panel: nothing is checked.
	constexpr explicit Panel(const Handle& other) noexcept : Widget(other.id()) {}

	/// Gap inserted between neighbouring children.
	void set_spacing(float spacing) const { Rpanel_set_spacing(id_, spacing); }
};

class UniformGrid : public Widget {
public:
	using Widget::Widget;
	constexpr UniformGrid() noexcept = default;
	/// The same widget, seen as a UniformGrid: nothing is checked.
	constexpr explicit UniformGrid(const Handle& other) noexcept : Widget(other.id()) {}

	static UniformGrid create() { return UniformGrid(Rwidget_create(R_WIDGET_UNIFORM_GRID)); }

	/// Places children automatically, left to right then top to bottom,
	/// in the given number of columns. 0 uses the grid cell of each slot (default).
	void set_columns(int columns) const { Runiform_grid_set_columns(id_, columns); }

	void set_min_cell_size(float width, float height) const { Runiform_grid_set_min_cell_size(id_, width, height); }
};

class Grid : public Widget {
public:
	using Widget::Widget;
	constexpr Grid() noexcept = default;
	/// The same widget, seen as a Grid: nothing is checked.
	constexpr explicit Grid(const Handle& other) noexcept : Widget(other.id()) {}

	static Grid create() { return Grid(Rwidget_create(R_WIDGET_GRID)); }

	/// Lets a column / row grow to take a share of the free space
	/// (Slate FillColumn / FillRow). 0 sizes it to its content (default).
	void set_column_fill(int column, float coefficient) const { Rgrid_set_column_fill(id_, column, coefficient); }

	void set_row_fill(int row, float coefficient) const { Rgrid_set_row_fill(id_, row, coefficient); }
};

class Border : public Widget {
public:
	using Widget::Widget;
	constexpr Border() noexcept = default;
	/// The same widget, seen as a Border: nothing is checked.
	constexpr explicit Border(const Handle& other) noexcept : Widget(other.id()) {}

	static Border create() { return Border(Rwidget_create(R_WIDGET_BORDER)); }

	/// Space between the outline and the child.
	void set_padding(float left, float top, float right, float bottom) const { Rborder_set_padding(id_, left, top, right, bottom); }

	/// Background color (R_COLOR_PANEL of this widget). Alpha 0 draws no background.
	void set_background(float r, float g, float b, float a) const { Rborder_set_background(id_, r, g, b, a); }

	/// Outline width and color. Width 0 draws no outline (default).
	void set_outline(float width, float r, float g, float b, float a) const { Rborder_set_outline(id_, width, r, g, b, a); }

	void set_corner_radius(float top_left, float top_right, float bottom_right, float bottom_left) const { Rborder_set_corner_radius(id_, top_left, top_right, bottom_right, bottom_left); }

	/// Soft drop shadow behind the border. Blur 0 disables it (default).
	void set_shadow(float blur, float offset_x, float offset_y) const { Rborder_set_shadow(id_, blur, offset_x, offset_y); }

	/// What the border is made of. R_MATERIAL_GLASS turns it into a
	/// panel of glass tinted by its background color (the alpha of the color
	/// is the strength of the tint): floating tool bars, side bars, cards.
	void set_material(RMaterial material) const { Rborder_set_material(id_, material); }
};

class SizeBox : public Widget {
public:
	using Widget::Widget;
	constexpr SizeBox() noexcept = default;
	/// The same widget, seen as a SizeBox: nothing is checked.
	constexpr explicit SizeBox(const Handle& other) noexcept : Widget(other.id()) {}

	static SizeBox create() { return SizeBox(Rwidget_create(R_WIDGET_SIZE_BOX)); }

	/// Keeps the child at a fixed width / height ratio, as large as fits.
	/// 0 disables it (default). The fixed, minimum and maximum sizes are set
	/// with Rwidget_set_size, Rwidget_set_min_size and Rwidget_set_max_size.
	void set_aspect_ratio(float ratio) const { Rsize_box_set_aspect_ratio(id_, ratio); }
};

class Spacer : public Widget {
public:
	using Widget::Widget;
	constexpr Spacer() noexcept = default;
	/// The same widget, seen as a Spacer: nothing is checked.
	constexpr explicit Spacer(const Handle& other) noexcept : Widget(other.id()) {}

	static Spacer create() { return Spacer(Rwidget_create(R_WIDGET_SPACER)); }

	void set_size(float width, float height) const { Rspacer_set_size(id_, width, height); }
};

class ScrollBox : public Widget {
public:
	using Widget::Widget;
	constexpr ScrollBox() noexcept = default;
	/// The same widget, seen as a ScrollBox: nothing is checked.
	constexpr explicit ScrollBox(const Handle& other) noexcept : Widget(other.id()) {}

	static ScrollBox create() { return ScrollBox(Rwidget_create(R_WIDGET_SCROLL_BOX)); }

	/// Direction in which children are stacked and scrolled. Default: R_VERTICAL.
	void set_orientation(ROrientation orientation) const { Rscroll_box_set_orientation(id_, orientation); }

	/// R_TRUE also scrolls across the stacking direction when the content is too large.
	void set_scroll_both(int enabled) const { Rscroll_box_set_scroll_both(id_, enabled); }

	void set_offset(float x, float y) const { Rscroll_box_set_offset(id_, x, y); }

	void get_offset(float* x, float* y) const { Rscroll_box_get_offset(id_, x, y); }

	void scroll_to_end() const { Rscroll_box_scroll_to_end(id_); }
};

class Splitter : public Widget {
public:
	using Widget::Widget;
	constexpr Splitter() noexcept = default;
	/// The same widget, seen as a Splitter: nothing is checked.
	constexpr explicit Splitter(const Handle& other) noexcept : Widget(other.id()) {}

	static Splitter create() { return Splitter(Rwidget_create(R_WIDGET_SPLITTER)); }

	/// R_HORIZONTAL puts the panes side by side (default), R_VERTICAL stacks them.
	void set_orientation(ROrientation orientation) const { Rsplitter_set_orientation(id_, orientation); }

	void set_min_pane_size(float size) const { Rsplitter_set_min_pane_size(id_, size); }
};

class Switcher : public Widget {
public:
	using Widget::Widget;
	constexpr Switcher() noexcept = default;
	/// The same widget, seen as a Switcher: nothing is checked.
	constexpr explicit Switcher(const Handle& other) noexcept : Widget(other.id()) {}

	static Switcher create() { return Switcher(Rwidget_create(R_WIDGET_SWITCHER)); }

	void set_index(int index) const { Rswitcher_set_index(id_, index); }

	int get_index() const { return Rswitcher_get_index(id_); }
};

class TabView : public Widget {
public:
	using Widget::Widget;
	constexpr TabView() noexcept = default;
	/// The same widget, seen as a TabView: nothing is checked.
	constexpr explicit TabView(const Handle& other) noexcept : Widget(other.id()) {}

	static TabView create() { return TabView(Rwidget_create(R_WIDGET_TAB_VIEW)); }

	/// Adds a page. Same as Rwidget_add_child followed by Rslot_set_title.
	/// Returns: The index of the new tab, or -1.
	int add_tab(const char* title, Rid content) const { return Rtab_view_add_tab(id_, title, content); }

	void set_active(int index) const { Rtab_view_set_active(id_, index); }

	int get_active() const { return Rtab_view_get_active(id_); }

	void set_changed_callback(RTabChangedCallback callback, void* user_data) const { Rtab_view_set_changed_callback(id_, callback, user_data); }
	void set_changed_callback(std::function<void(Rid, int)> callback) const {
		Rtab_view_set_changed_callback(id_, [](Rid tab_view, int index, void* kept_) -> void { return (*static_cast<std::function<void(Rid, int)>*>(kept_))(tab_view, index); }, detail::keep(id_, 16, std::move(callback)));
	}
};

class Expander : public Widget {
public:
	using Widget::Widget;
	constexpr Expander() noexcept = default;
	/// The same widget, seen as a Expander: nothing is checked.
	constexpr explicit Expander(const Handle& other) noexcept : Widget(other.id()) {}

	static Expander create() { return Expander(Rwidget_create(R_WIDGET_EXPANDER)); }

	void set_title(const char* title) const { Rexpander_set_title(id_, title); }

	void set_expanded(int expanded) const { Rexpander_set_expanded(id_, expanded); }

	int is_expanded() const { return Rexpander_is_expanded(id_); }

	void set_changed_callback(RExpanderChangedCallback callback, void* user_data) const { Rexpander_set_changed_callback(id_, callback, user_data); }
	void set_changed_callback(std::function<void(Rid, int)> callback) const {
		Rexpander_set_changed_callback(id_, [](Rid expander, int expanded, void* kept_) -> void { return (*static_cast<std::function<void(Rid, int)>*>(kept_))(expander, expanded); }, detail::keep(id_, 17, std::move(callback)));
	}
};

class TextEditor : public Widget {
public:
	using Widget::Widget;
	constexpr TextEditor() noexcept = default;
	/// The same widget, seen as a TextEditor: nothing is checked.
	constexpr explicit TextEditor(const Handle& other) noexcept : Widget(other.id()) {}

	/// text Initial content, or NULL.
	static TextEditor create(const char* text) { return TextEditor(Rtext_editor_create(text)); }

	/// Replaces the whole text. The history of undo is cleared; the changed callback is not called.
	void set_text(const char* text) const { Rtext_editor_set_text(id_, text); }

	/// Returns: The whole text, lines separated by '\n'. Valid until the next call or the next edit.
	const char* get_text() const { return Rtext_editor_get_text(id_); }

	/// Returns: The number of bytes of Rtext_editor_get_text.
	int get_length() const { return Rtext_editor_get_length(id_); }

	int get_line_count() const { return Rtext_editor_get_line_count(id_); }

	/// Returns: One line, without its line break. Valid until the next edit. NULL when `line` is out of range.
	const char* get_line(int line) const { return Rtext_editor_get_line(id_, line); }

	/// Types a text at the cursor, replacing the selection, as the user
	/// would: it can be undone and the changed callback is called.
	void insert_text(const char* text) const { Rtext_editor_insert_text(id_, text); }

	void set_cursor(int line, int column) const { Rtext_editor_set_cursor(id_, line, column); }

	void get_cursor(int* line, int* column) const { Rtext_editor_get_cursor(id_, line, column); }

	/// Selects from one position to another; the cursor is on the second.
	void select(int from_line, int from_column, int to_line, int to_column) const { Rtext_editor_select(id_, from_line, from_column, to_line, to_column); }

	void select_all() const { Rtext_editor_select_all(id_); }

	/// Start and end of the selection, in the order of the text. Returns R_FALSE when nothing is selected.
	int get_selection(int* from_line, int* from_column, int* to_line, int* to_column) const { return Rtext_editor_get_selection(id_, from_line, from_column, to_line, to_column); }

	/// Returns: The selected text, "" when there is none. Valid until the next call or the next edit.
	const char* get_selected_text() const { return Rtext_editor_get_selected_text(id_); }

	/// Scrolls so that a line is visible.
	void scroll_to_line(int line) const { Rtext_editor_scroll_to_line(id_, line); }

	void undo() const { Rtext_editor_undo(id_); }

	void redo() const { Rtext_editor_redo(id_); }

	int can_undo() const { return Rtext_editor_can_undo(id_); }

	int can_redo() const { return Rtext_editor_can_redo(id_); }

	/// A read-only editor can still be scrolled, selected and copied.
	void set_read_only(int read_only) const { Rtext_editor_set_read_only(id_, read_only); }

	/// Long lines continue on the next row instead of scrolling sideways. Off by default.
	void set_word_wrap(int enabled) const { Rtext_editor_set_word_wrap(id_, enabled); }

	/// Shows the line numbers in a margin. Off by default.
	void set_line_numbers(int enabled) const { Rtext_editor_set_line_numbers(id_, enabled); }

	/// Width of a tab, in spaces (4 by default).
	void set_tab_size(int spaces) const { Rtext_editor_set_tab_size(id_, spaces); }

	/// R_TRUE makes the Tab key insert spaces instead of a tab character. Off by default.
	void set_tab_inserts_spaces(int enabled) const { Rtext_editor_set_tab_inserts_spaces(id_, enabled); }

	/// The same as Rtext_editor_set_language with "XML", "C" or NULL.
	void set_syntax(RSyntax syntax) const { Rtext_editor_set_syntax(id_, syntax); }

	/// font R_INVALID_ID uses the font of the theme (or the fixed-width
	/// font, with a language).
	/// size 0 uses the font size of the theme.
	void set_font(Rid font, float size) const { Rtext_editor_set_font(id_, font, size); }

	/// Text shown, dimmed, while the editor is empty.
	void set_placeholder(const char* text) const { Rtext_editor_set_placeholder(id_, text); }

	/// Gives the editor a language, by name ("C++", "Python"...), by
	/// another usual name ("cpp", "js") or by the extension of its files
	/// ("rs"). NULL or "" removes it. An editor with a language uses the
	/// fixed-width font unless it was given a font.
	void set_language(const char* name) const { Rtext_editor_set_language(id_, name); }

	/// Returns: The name of the language of the editor, or "".
	const char* get_language() const { return Rtext_editor_get_language(id_); }

	/// What the language of the editor makes of the character at a
	/// position: the colors come from it. R_TOKEN_TEXT without a language.
	RToken get_token(int line, int column) const { return Rtext_editor_get_token(id_, line, column); }

	/// R_FALSE stops the editor from proposing words by itself; Ctrl+Space still does. On by default.
	void set_completion(int enabled) const { Rtext_editor_set_completion(id_, enabled); }

	/// Proposes words for what is before the cursor, as Ctrl+Space does.
	void show_completions() const { Rtext_editor_show_completions(id_); }

	int is_completing() const { return Rtext_editor_is_completing(id_); }

	void set_completion_callback(RCompletionCallback callback, void* user_data) const { Rtext_editor_set_completion_callback(id_, callback, user_data); }
	void set_completion_callback(std::function<void(Rid, const char*)> callback) const {
		Rtext_editor_set_completion_callback(id_, [](Rid editor, const char* prefix, void* kept_) -> void { return (*static_cast<std::function<void(Rid, const char*)>*>(kept_))(editor, prefix); }, detail::keep(id_, 18, std::move(callback)));
	}

	/// Adds a proposal. Only from a completion callback. Proposals that
	/// do not start with the prefix are left out; the others keep the order
	/// they were added in.
	/// detail A few words shown beside it ("function", "int"), or NULL.
	void add_completion(const char* text, const char* detail) const { Rtext_editor_add_completion(id_, text, detail); }

	/// R_FALSE stops the editor from closing brackets and quotes and
	/// from indenting after them. On by default; it only acts with a language.
	void set_auto_pairs(int enabled) const { Rtext_editor_set_auto_pairs(id_, enabled); }

	/// Underlines a part of a line and attaches a message to it, shown
	/// when the pointer rests there: an error of a compiler, a warning.
	/// Markers follow their line when text is added or removed above it, and
	/// go away when their own line is edited.
	/// to_column -1 for the end of the line.
	void add_marker(int line, int from_column, int to_column, RMarkerKind kind, const char* message) const { Rtext_editor_add_marker(id_, line, from_column, to_column, kind, message); }

	void clear_markers() const { Rtext_editor_clear_markers(id_); }

	int get_marker_count() const { return Rtext_editor_get_marker_count(id_); }

	void set_changed_callback(RTextEditorChangedCallback callback, void* user_data) const { Rtext_editor_set_changed_callback(id_, callback, user_data); }
	void set_changed_callback(std::function<void(Rid)> callback) const {
		Rtext_editor_set_changed_callback(id_, [](Rid editor, void* kept_) -> void { return (*static_cast<std::function<void(Rid)>*>(kept_))(editor); }, detail::keep(id_, 19, std::move(callback)));
	}
};

class HeaderBar : public Widget {
public:
	using Widget::Widget;
	constexpr HeaderBar() noexcept = default;
	/// The same widget, seen as a HeaderBar: nothing is checked.
	constexpr explicit HeaderBar(const Handle& other) noexcept : Widget(other.id()) {}

	/// title Text shown in the bar; NULL shows the title of the window.
	static HeaderBar create(const char* title) { return HeaderBar(Rheader_bar_create(title)); }

	/// title NULL shows the title of the window.
	void set_title(const char* title) const { Rheader_bar_set_title(id_, title); }

	/// buttons Bitmask of RHeaderButtons. All by default; the maximize button only shows on a resizable window.
	void set_buttons(Ruint buttons) const { Rheader_bar_set_buttons(id_, buttons); }

	/// side R_HALIGN_RIGHT (default): minimize, maximize, close, as on
	/// Windows. R_HALIGN_LEFT: three round colored buttons, as on macOS.
	void set_button_side(RHAlign side) const { Rheader_bar_set_button_side(id_, side); }

	/// align R_HALIGN_CENTER (default) or R_HALIGN_LEFT.
	void set_title_align(RHAlign align) const { Rheader_bar_set_title_align(id_, align); }

	/// R_FALSE removes the background of the bar: for a header that
	/// floats above the content (in an Overlay) or sits inside a glass panel.
	void set_background(int enabled) const { Rheader_bar_set_background(id_, enabled); }
};

class DockArea : public Widget {
public:
	using Widget::Widget;
	constexpr DockArea() noexcept = default;
	/// The same widget, seen as a DockArea: nothing is checked.
	constexpr explicit DockArea(const Handle& other) noexcept : Widget(other.id()) {}

	static DockArea create() { return DockArea(Rdock_area_create()); }

	/// Adds a panel. The dock area takes ownership of the widget.
	/// title    Text of its tab.
	/// side     Where it goes.
	/// relative A panel of the area to split or to share tabs with, or
	///                 R_INVALID_ID for the whole area (with R_DOCK_TAB: the
	///                 first group of tabs).
	/// size     Part of the split space given to the new panel, 0..1
	///                 (0 = a half, or a quarter along the whole area).
	void add_panel(Rid panel, const char* title, RDockSide side, Rid relative, float size) const { Rdock_area_add_panel(id_, panel, title, side, relative, size); }

	/// Moves a panel of the area somewhere else in it.
	void move_panel(Rid panel, RDockSide side, Rid relative, float size) const { Rdock_area_move_panel(id_, panel, side, relative, size); }

	/// Removes a panel and deletes it.
	void close_panel(Rid panel) const { Rdock_area_close_panel(id_, panel); }

	void set_panel_title(Rid panel, const char* title) const { Rdock_area_set_panel_title(id_, panel, title); }

	/// R_FALSE removes the close button of the tab of a panel. Panels can be closed by default.
	void set_panel_closable(Rid panel, int closable) const { Rdock_area_set_panel_closable(id_, panel, closable); }

	/// Brings the tab of a panel to the front.
	void show_panel(Rid panel) const { Rdock_area_show_panel(id_, panel); }

	/// Returns: R_TRUE when the panel is the one shown in its group of tabs.
	int is_panel_shown(Rid panel) const { return Rdock_area_is_panel_shown(id_, panel); }

	/// R_FALSE stops the user from moving the panels (the borders can still be dragged).
	void set_movable(int movable) const { Rdock_area_set_movable(id_, movable); }

	/// Describes how the panels are arranged, as a short text to keep
	/// in a settings file. Panels are named by their widget name
	/// (Rwidget_set_name): unnamed panels are not part of the description.
	/// Returns: A string valid until the next call.
	const char* save_layout() const { return Rdock_area_save_layout(id_); }

	/// Arranges the panels as a saved description says. Panels of the
	/// area that the description does not name keep a place; names it has
	/// that match no panel are ignored.
	/// Returns: R_FALSE when the text is not a layout (nothing is changed).
	int load_layout(const char* layout) const { return Rdock_area_load_layout(id_, layout); }

	void set_close_callback(RDockCloseCallback callback, void* user_data) const { Rdock_area_set_close_callback(id_, callback, user_data); }
	void set_close_callback(std::function<int(Rid, Rid)> callback) const {
		Rdock_area_set_close_callback(id_, [](Rid area, Rid panel, void* kept_) -> int { return (*static_cast<std::function<int(Rid, Rid)>*>(kept_))(area, panel); }, detail::keep(id_, 20, std::move(callback)));
	}

	void set_changed_callback(RDockChangedCallback callback, void* user_data) const { Rdock_area_set_changed_callback(id_, callback, user_data); }
	void set_changed_callback(std::function<void(Rid)> callback) const {
		Rdock_area_set_changed_callback(id_, [](Rid area, void* kept_) -> void { return (*static_cast<std::function<void(Rid)>*>(kept_))(area); }, detail::keep(id_, 21, std::move(callback)));
	}
};

class Island : public Widget {
public:
	using Widget::Widget;
	constexpr Island() noexcept = default;
	/// The same widget, seen as a Island: nothing is checked.
	constexpr explicit Island(const Handle& other) noexcept : Widget(other.id()) {}

	static Island create() { return Island(Risland_create()); }

	/// Unfolds or folds the island. The changed callback is not called.
	void set_expanded(int expanded) const { Risland_set_expanded(id_, expanded); }

	int is_expanded() const { return Risland_is_expanded(id_); }

	/// Unfolds the island, and folds it back by itself after a while:
	/// to tell something to the user. It stays open while the pointer is on it.
	void expand_for(double seconds) const { Risland_expand_for(id_, seconds); }

	/// R_TRUE (default): the island unfolds under the pointer and folds back when it leaves.
	void set_expand_on_hover(int enabled) const { Risland_set_expand_on_hover(id_, enabled); }

	/// R_TRUE (default): a click on the island unfolds or folds it.
	void set_expand_on_click(int enabled) const { Risland_set_expand_on_click(id_, enabled); }

	/// R_TRUE gives the island square upper corners: a notch that hangs
	/// from the edge it touches, instead of a capsule that floats under it.
	void set_attached(int attached) const { Risland_set_attached(id_, attached); }

	void set_changed_callback(RIslandChangedCallback callback, void* user_data) const { Risland_set_changed_callback(id_, callback, user_data); }
	void set_changed_callback(std::function<void(Rid, int)> callback) const {
		Risland_set_changed_callback(id_, [](Rid island, int expanded, void* kept_) -> void { return (*static_cast<std::function<void(Rid, int)>*>(kept_))(island, expanded); }, detail::keep(id_, 22, std::move(callback)));
	}
};

class FileBrowser : public Widget {
public:
	using Widget::Widget;
	constexpr FileBrowser() noexcept = default;
	/// The same widget, seen as a FileBrowser: nothing is checked.
	constexpr explicit FileBrowser(const Handle& other) noexcept : Widget(other.id()) {}

	static FileBrowser create() { return FileBrowser(Rfile_browser_create()); }

	void set_mode(RFileBrowserMode mode) const { Rfile_browser_set_mode(id_, mode); }

	/// Shows a folder. "~" is the home of the user. Returns R_FALSE when it cannot be read.
	int set_directory(const char* path) const { return Rfile_browser_set_directory(id_, path); }

	const char* get_directory() const { return Rfile_browser_get_directory(id_); }

	/// Reads the folder again.
	void refresh() const { Rfile_browser_refresh(id_); }

	/// Kinds of files the user chooses from; the first one is active. The strings are copied.
	void set_filters(const RFileFilter* filters, int count) const { Rfile_browser_set_filters(id_, filters, count); }

	void set_show_hidden(int show) const { Rfile_browser_set_show_hidden(id_, show); }

	/// R_TRUE lets several files be selected (Ctrl and Shift with a click).
	void set_multiple(int multiple) const { Rfile_browser_set_multiple(id_, multiple); }

	/// R_FALSE hides the name field, the filter list and the buttons:
	/// what remains is an explorer to put in a side panel. Shown by default.
	void set_controls_visible(int visible) const { Rfile_browser_set_controls_visible(id_, visible); }

	/// R_FALSE hides the list of places on the left.
	void set_places_visible(int visible) const { Rfile_browser_set_places_visible(id_, visible); }

	/// Name typed in the name field (save mode).
	void set_file_name(const char* name) const { Rfile_browser_set_file_name(id_, name); }

	/// Added to a name typed without extension, in save mode.
	void set_default_extension(const char* extension) const { Rfile_browser_set_default_extension(id_, extension); }

	/// Paths of the selected entries. The strings are valid until the selection or the folder changes.
	int get_selection_count() const { return Rfile_browser_get_selection_count(id_); }

	const char* get_selection(int index) const { return Rfile_browser_get_selection(id_, index); }

	/// Selects an entry of the folder by its name.
	void select(const char* name) const { Rfile_browser_select(id_, name); }

	void set_accepted_callback(RFileBrowserCallback callback, void* user_data) const { Rfile_browser_set_accepted_callback(id_, callback, user_data); }
	void set_accepted_callback(std::function<void(Rid, const char*)> callback) const {
		Rfile_browser_set_accepted_callback(id_, [](Rid browser, const char* path, void* kept_) -> void { return (*static_cast<std::function<void(Rid, const char*)>*>(kept_))(browser, path); }, detail::keep(id_, 23, std::move(callback)));
	}

	/// Called by the Cancel button.
	void set_cancelled_callback(RFileBrowserCallback callback, void* user_data) const { Rfile_browser_set_cancelled_callback(id_, callback, user_data); }
	void set_cancelled_callback(std::function<void(Rid, const char*)> callback) const {
		Rfile_browser_set_cancelled_callback(id_, [](Rid browser, const char* path, void* kept_) -> void { return (*static_cast<std::function<void(Rid, const char*)>*>(kept_))(browser, path); }, detail::keep(id_, 24, std::move(callback)));
	}

	/// Called when the selection changes; `path` is the entry under the cursor, or NULL.
	void set_selection_changed_callback(RFileBrowserCallback callback, void* user_data) const { Rfile_browser_set_selection_changed_callback(id_, callback, user_data); }
	void set_selection_changed_callback(std::function<void(Rid, const char*)> callback) const {
		Rfile_browser_set_selection_changed_callback(id_, [](Rid browser, const char* path, void* kept_) -> void { return (*static_cast<std::function<void(Rid, const char*)>*>(kept_))(browser, path); }, detail::keep(id_, 25, std::move(callback)));
	}
};

class Viewport : public Widget {
public:
	using Widget::Widget;
	constexpr Viewport() noexcept = default;
	/// The same widget, seen as a Viewport: nothing is checked.
	constexpr explicit Viewport(const Handle& other) noexcept : Widget(other.id()) {}

	static Viewport create() { return Viewport(Rwidget_create(R_WIDGET_VIEWPORT)); }

	/// Shows a texture. R_INVALID_ID shows nothing.
	void set_texture(Rid texture) const { Rviewport_set_texture(id_, texture); }

	/// How the texture is fitted in the widget. Default: R_STRETCH_FILL.
	void set_stretch(RStretch stretch) const { Rviewport_set_stretch(id_, stretch); }

	/// R_TRUE flips the texture vertically (OpenGL framebuffers are bottom-up).
	void set_flip_y(int flip) const { Rviewport_set_flip_y(id_, flip); }

	void set_draw_callback(RViewportDrawCallback callback, void* user_data) const { Rviewport_set_draw_callback(id_, callback, user_data); }
	void set_draw_callback(std::function<void(const RViewportDrawInfo*)> callback) const {
		Rviewport_set_draw_callback(id_, [](const RViewportDrawInfo* info, void* kept_) -> void { return (*static_cast<std::function<void(const RViewportDrawInfo*)>*>(kept_))(info); }, detail::keep(id_, 26, std::move(callback)));
	}

	/// Creates (or destroys) the native child window of the viewport.
	/// The viewport must already be in a window.
	/// Returns: R_TRUE on success.
	int set_native(int enabled) const { return Rviewport_set_native(id_, enabled); }

	/// Fills `native` with the handles of the child window.
	int get_native_handle(RNativeWindow* native) const { return Rviewport_get_native_handle(id_, native); }

	/// Size of the viewport in pixels after the last layout.
	void get_pixel_size(int* width, int* height) const { Rviewport_get_pixel_size(id_, width, height); }

	void set_resize_callback(RViewportResizeCallback callback, void* user_data) const { Rviewport_set_resize_callback(id_, callback, user_data); }
	void set_resize_callback(std::function<void(Rid, int, int)> callback) const {
		Rviewport_set_resize_callback(id_, [](Rid viewport, int width, int height, void* kept_) -> void { return (*static_cast<std::function<void(Rid, int, int)>*>(kept_))(viewport, width, height); }, detail::keep(id_, 27, std::move(callback)));
	}

	/// R_TRUE redraws the window every frame while the viewport is
	/// visible (game view). R_FALSE redraws only on Rwidget_invalidate (default).
	void set_realtime(int realtime) const { Rviewport_set_realtime(id_, realtime); }
};

class Custom : public Widget {
public:
	using Widget::Widget;
	constexpr Custom() noexcept = default;
	/// The same widget, seen as a Custom: nothing is checked.
	constexpr explicit Custom(const Handle& other) noexcept : Widget(other.id()) {}

	static Custom create() { return Custom(Rwidget_create(R_WIDGET_CUSTOM)); }

	void set_paint_callback(RPaintCallback callback, void* user_data) const { Rcustom_set_paint_callback(id_, callback, user_data); }
	void set_paint_callback(std::function<void(Rid, RPainter*, float, float)> callback) const {
		Rcustom_set_paint_callback(id_, [](Rid widget, RPainter* painter, float width, float height, void* kept_) -> void { return (*static_cast<std::function<void(Rid, RPainter*, float, float)>*>(kept_))(widget, painter, width, height); }, detail::keep(id_, 28, std::move(callback)));
	}

	/// Reports the desired size of the widget. Without it the widget asks for 0 x 0.
	void set_measure_callback(RMeasureCallback callback, void* user_data) const { Rcustom_set_measure_callback(id_, callback, user_data); }
	void set_measure_callback(std::function<void(Rid, float*, float*)> callback) const {
		Rcustom_set_measure_callback(id_, [](Rid widget, float* width, float* height, void* kept_) -> void { return (*static_cast<std::function<void(Rid, float*, float*)>*>(kept_))(widget, width, height); }, detail::keep(id_, 29, std::move(callback)));
	}
};

class Window : public Handle {
public:
	using Handle::Handle;
	constexpr Window() noexcept = default;

	void get_frame_stats(RFrameStats* stats) const { Rwindow_get_frame_stats(id_, stats); }

	/// Opens a top-level window.
	/// width,height Client size in logical units.
	/// flags        Bitmask of RWindowFlags.
	static Window create(const char* title, int width, int height, Ruint flags) { return Window(Rwindow_create(title, width, height, flags)); }

	/// Closes a window and deletes its content widgets.
	void destroy() const { Rwindow_destroy(id_); }

	int is_valid() const { return Rwindow_is_valid(id_); }

	/// Sets the root widget of the window. The window takes ownership of
	/// the widget; the previous content is deleted.
	void set_content(Rid widget) const { Rwindow_set_content(id_, widget); }

	Rid get_content() const { return Rwindow_get_content(id_); }

	void set_title(const char* title) const { Rwindow_set_title(id_, title); }

	const char* get_title() const { return Rwindow_get_title(id_); }

	void set_size(int width, int height) const { Rwindow_set_size(id_, width, height); }

	void get_size(int* width, int* height) const { Rwindow_get_size(id_, width, height); }

	void set_min_size(int width, int height) const { Rwindow_set_min_size(id_, width, height); }

	void set_position(int x, int y) const { Rwindow_set_position(id_, x, y); }

	void get_position(int* x, int* y) const { Rwindow_get_position(id_, x, y); }

	/// Size of the drawable area in pixels.
	void get_framebuffer_size(int* width, int* height) const { Rwindow_get_framebuffer_size(id_, width, height); }

	/// Returns: Pixels per logical unit (monitor scale x global UI scale).
	float get_scale() const { return Rwindow_get_scale(id_); }

	void show() const { Rwindow_show(id_); }

	void hide() const { Rwindow_hide(id_); }

	void maximize() const { Rwindow_maximize(id_); }

	void minimize() const { Rwindow_minimize(id_); }

	void restore() const { Rwindow_restore(id_); }

	int is_maximized() const { return Rwindow_is_maximized(id_); }

	int is_minimized() const { return Rwindow_is_minimized(id_); }

	/// Returns: R_TRUE while the window is the one the user is working in.
	int is_active() const { return Rwindow_is_active(id_); }

	/// Asks to close the window, as the close button of its title bar
	/// does: the close callback is asked first and may refuse.
	void close() const { Rwindow_close(id_); }

	/// Custom headers. Call these from a mouse-down handler, while the left
	/// button is held: the window follows the pointer until the button is
	/// released (the widget does not receive the release). A
	/// R_WIDGET_HEADER_BAR and the widgets given to Rwidget_set_window_drag do
	/// it by themselves.
	void begin_move() const { Rwindow_begin_move(id_); }

	void begin_resize(RWindowEdge edge) const { Rwindow_begin_resize(id_, edge); }

	/// Color behind the content. By default it is the theme color
	/// R_COLOR_WINDOW (and, with a glass theme, the wallpaper of the theme).
	void set_clear_color(float r, float g, float b, float a) const { Rwindow_set_clear_color(id_, r, g, b, a); }

	/// Image drawn behind the content, scaled to cover the window:
	/// what the glass of a glass theme looks through. R_INVALID_ID removes it.
	void set_background(Rid texture) const { Rwindow_set_background(id_, texture); }

	/// Asks for a new frame even if no widget changed. Thread-safe.
	void request_redraw() const { Rwindow_request_redraw(id_); }

	/// R_TRUE redraws the window every frame (games, live viewports).
	void set_continuous_redraw(int enabled) const { Rwindow_set_continuous_redraw(id_, enabled); }

	void set_close_callback(RWindowCloseCallback callback, void* user_data) const { Rwindow_set_close_callback(id_, callback, user_data); }
	void set_close_callback(std::function<int(Rid)> callback) const {
		Rwindow_set_close_callback(id_, [](Rid window, void* kept_) -> int { return (*static_cast<std::function<int(Rid)>*>(kept_))(window); }, detail::keep(id_, 30, std::move(callback)));
	}

	void set_resize_callback(RWindowResizeCallback callback, void* user_data) const { Rwindow_set_resize_callback(id_, callback, user_data); }
	void set_resize_callback(std::function<void(Rid, int, int)> callback) const {
		Rwindow_set_resize_callback(id_, [](Rid window, int width, int height, void* kept_) -> void { return (*static_cast<std::function<void(Rid, int, int)>*>(kept_))(window, width, height); }, detail::keep(id_, 31, std::move(callback)));
	}

	/// Takes what is dropped on the window from other applications,
	/// where no widget takes it (Rwidget_set_drop_callback). NULL removes it.
	void set_drop_callback(RDropCallback callback, void* user_data) const { Rwindow_set_drop_callback(id_, callback, user_data); }
	void set_drop_callback(std::function<int(Rid, const RDropEvent*)> callback) const {
		Rwindow_set_drop_callback(id_, [](Rid target, const RDropEvent* event, void* kept_) -> int { return (*static_cast<std::function<int(Rid, const RDropEvent*)>*>(kept_))(target, event); }, detail::keep(id_, 32, std::move(callback)));
	}

	void set_user_handle(void* handle) const { Rwindow_set_user_handle(id_, handle); }

	void* get_user_handle() const { return Rwindow_get_user_handle(id_); }

	/// Fills `native` with the OS handles of the window.
	int get_native_handle(RNativeWindow* native) const { return Rwindow_get_native_handle(id_, native); }

	/// Reads back the last rendered frame as tightly packed RGBA8 pixels.
	/// Blocks until the frame is rendered. Free the result with Rpixels_free.
	/// Returns: NULL when the backend cannot capture.
	uint8_t* capture(int* width, int* height) const { return Rwindow_capture(id_, width, height); }

	/// Widget that has the keyboard focus in the window, or R_INVALID_ID.
	Rid get_focused_widget() const { return Rwindow_get_focused_widget(id_); }

	/// Deepest widget under the cursor, or R_INVALID_ID.
	Rid get_hovered_widget() const { return Rwindow_get_hovered_widget(id_); }

	void get_mouse_position(float* x, float* y) const { Rwindow_get_mouse_position(id_, x, y); }

	/// Inject input as if it came from the OS. Positions are in logical units.
	/// Useful for automated tests, replays and when Raven is driven by a host
	/// application.
	void inject_mouse_move(float x, float y) const { Rwindow_inject_mouse_move(id_, x, y); }

	void inject_mouse_button(RMouseButton button, int pressed) const { Rwindow_inject_mouse_button(id_, button, pressed); }

	void inject_mouse_wheel(float wheel_x, float wheel_y) const { Rwindow_inject_mouse_wheel(id_, wheel_x, wheel_y); }

	void inject_key(RKey key, int pressed, Ruint modifiers) const { Rwindow_inject_key(id_, key, pressed, modifiers); }

	void inject_text(const char* text) const { Rwindow_inject_text(id_, text); }

	/// A drag from another application moves over the window
	/// (`inside` R_TRUE) or leaves it.
	/// Returns: R_TRUE when a drop at this position would be accepted.
	int inject_drag(float x, float y, int inside) const { return Rwindow_inject_drag(id_, x, y, inside); }

	/// Drops files and / or text at a position. Returns R_TRUE when a target took them.
	int inject_drop(float x, float y, const char*const* paths, int path_count, const char* text) const { return Rwindow_inject_drop(id_, x, y, paths, path_count, text); }

	/// Registers a keyboard shortcut active while the window has focus and
	/// no widget handled the key.
	/// action Name of the action to fire (see Raction_bind).
	void register_shortcut(RKey key, Ruint modifiers, const char* action) const { Rwindow_register_shortcut(id_, key, modifiers, action); }

	void unregister_shortcut(RKey key, Ruint modifiers) const { Rwindow_unregister_shortcut(id_, key, modifiers); }

	/// Closes every open menu and popup of the window.
	void close_popups() const { Rwindow_close_popups(id_); }

	/// Shows a widget (usually an island) floating at the top of a
	/// window, in the middle, above its content. The window takes ownership;
	/// the previous one is deleted. R_INVALID_ID removes it. The padding of
	/// its slot (Rslot_set_padding) is its distance from the top edge.
	void set_island(Rid widget) const { Rwindow_set_island(id_, widget); }

	Rid get_island() const { return Rwindow_get_island(id_); }

	/// Returns: R_TRUE while a dialog is shown in the window.
	int has_dialog() const { return Rwindow_has_dialog(id_); }

	/// Creates a window from a document whose root element is <Window>.
	/// xml  UTF-8 text. It does not need to be null-terminated.
	/// size Size of the text in bytes.
	/// Returns: The window, or R_INVALID_ID. Parse errors are reported with
	///         their line and column through the error callback (R_ERROR_XML).
	static Window create_from_xml(const char* xml, size_t size) { return Window(Rwindow_create_from_xml(xml, size)); }

	static Window create_from_xml_file(const char* path) { return Window(Rwindow_create_from_xml_file(path)); }
};

class TreeItem : public Handle {
public:
	using Handle::Handle;
	constexpr TreeItem() noexcept = default;

	/// Deletes an item and its descendants.
	void destroy() const { Rtree_item_destroy(id_); }

	int is_valid() const { return Rtree_item_is_valid(id_); }

	void set_text(const char* text) const { Rtree_item_set_text(id_, text); }

	const char* get_text() const { return Rtree_item_get_text(id_); }

	void set_expanded(int expanded) const { Rtree_item_set_expanded(id_, expanded); }

	int is_expanded() const { return Rtree_item_is_expanded(id_); }

	void set_user_handle(void* handle) const { Rtree_item_set_user_handle(id_, handle); }

	void* get_user_handle() const { return Rtree_item_get_user_handle(id_); }

	/// Returns: The parent item, or R_INVALID_ID for a top-level item.
	Rid get_parent() const { return Rtree_item_get_parent(id_); }

	int get_child_count() const { return Rtree_item_get_child_count(id_); }

	Rid get_child(int index) const { return Rtree_item_get_child(id_, index); }
};

class Font : public Handle {
public:
	using Handle::Handle;
	constexpr Font() noexcept = default;

	/// Creates a font from the content of a .ttf / .otf file.
	/// The buffer is copied. Glyphs are rasterized on demand, at the exact
	/// pixel size needed by each window scale, into a shared atlas.
	/// Returns: R_INVALID_ID on failure.
	static Font create(const char* data, size_t data_size) { return Font(Rfont_create(data, data_size)); }

	static Font create_from_file(const char* path) { return Font(Rfont_create_from_file(path)); }

	void destroy() const { Rfont_destroy(id_); }

	int is_valid() const { return Rfont_is_valid(id_); }

	/// Returns: The font embedded in the library (Roboto, Latin and Latin Extended-A).
	static Font get_default() { return Font(Rfont_get_default()); }

	/// Returns: The fixed-width font embedded in the library (Roboto Mono, same
	/// characters), used by default by the text editors. In XML: Font="monospace".
	static Font get_monospace() { return Font(Rfont_get_monospace()); }

	/// Font used for the characters missing from `font` (symbols, icons,
	/// other scripts). Fallbacks can be chained.
	void set_fallback(Rid fallback) const { Rfont_set_fallback(id_, fallback); }

	/// Measures a UTF-8 string, in logical units, at scale 1.
	/// font R_INVALID_ID uses the theme font.
	/// size Font size; 0 uses R_METRIC_FONT_SIZE.
	void measure_text(float size, const char* text, float* width, float* height) const { Rfont_measure_text(id_, size, text, width, height); }
};

class Texture : public Handle {
public:
	using Handle::Handle;
	constexpr Texture() noexcept = default;

	/// Creates a texture from the content of an image file (PNG, JPEG,
	/// BMP, TGA, GIF, PSD). Decoding happens on the calling thread.
	/// Returns: R_INVALID_ID on failure.
	static Texture create(const char* data, size_t data_size) { return Texture(Rtexture_create(data, data_size)); }

	/// Same as Rtexture_create, decoded on a worker thread. The handle
	/// is usable at once; widgets showing it draw nothing until it is ready.
	/// The buffer is copied.
	static Texture create_async(const char* data, size_t data_size) { return Texture(Rtexture_create_async(data, data_size)); }

	static Texture create_from_file(const char* path) { return Texture(Rtexture_create_from_file(path)); }

	/// Reads and decodes the file on a worker thread.
	static Texture create_from_file_async(const char* path) { return Texture(Rtexture_create_from_file_async(path)); }

	/// Creates a texture from tightly packed RGBA8 pixels (straight alpha).
	static Texture create_from_pixels(const uint8_t* rgba, int width, int height) { return Texture(Rtexture_create_from_pixels(rgba, width, height)); }

	/// Replaces the pixels of a texture. The size may change. Use it to
	/// show frames produced on the CPU or read back from another renderer.
	void update(const uint8_t* rgba, int width, int height) const { Rtexture_update(id_, rgba, width, height); }

	/// Returns: R_TRUE once an asynchronous texture is decoded (or failed).
	int is_ready() const { return Rtexture_is_ready(id_); }

	/// Blocks until an asynchronous texture is ready.
	void wait() const { Rtexture_wait(id_); }

	void destroy() const { Rtexture_destroy(id_); }

	int is_valid() const { return Rtexture_is_valid(id_); }

	void get_size(int* width, int* height) const { Rtexture_get_size(id_, width, height); }

	void set_filter(RFilterType filter) const { Rtexture_set_filter(id_, filter); }
};

class Timer : public Handle {
public:
	using Handle::Handle;
	constexpr Timer() noexcept = default;

	/// Calls a function on the UI thread after `interval_seconds`.
	/// repeat R_TRUE to fire repeatedly, R_FALSE to fire once and delete itself.
	static Timer create(double interval_seconds, int repeat, RTimerCallback callback, void* user_data) { return Timer(Rtimer_create(interval_seconds, repeat, callback, user_data)); }
	static Rid create(double interval_seconds, int repeat, std::function<void(Rid)> callback) {
		return Rtimer_create(interval_seconds, repeat, [](Rid timer, void* kept_) -> void { return (*static_cast<std::function<void(Rid)>*>(kept_))(timer); }, detail::keep(0, 33, std::move(callback)));
	}

	void destroy() const { Rtimer_destroy(id_); }

	int is_valid() const { return Rtimer_is_valid(id_); }
};

class XmlDocument : public Handle {
public:
	using Handle::Handle;
	constexpr XmlDocument() noexcept = default;

	/// A new document with one element.
	static XmlDocument create(const char* root_tag) { return XmlDocument(Rxml_document_create(root_tag)); }

	/// Reads a text into a document. Comments are kept.
	/// error When not NULL, a mistake in the text is described there
	/// and nothing is reported; when NULL it goes to the error callback.
	/// Returns: The document, or R_INVALID_ID.
	static XmlDocument parse(const char* xml, size_t size, RXmlError* error) { return XmlDocument(Rxml_document_parse(xml, size, error)); }

	/// Reads a file. The paths written in it are relative to its folder.
	static XmlDocument load(const char* path) { return XmlDocument(Rxml_document_load(path)); }

	/// Releases a document and its nodes. What was built from it lives on.
	void destroy() const { Rxml_document_destroy(id_); }

	int is_valid() const { return Rxml_document_is_valid(id_); }

	Rid get_root() const { return Rxml_document_get_root(id_); }

	/// Writes the document as text, one element per line, indented
	/// with tabs. Rxml_node_get_line then says where each node was written.
	/// Returns: The text, valid until the next call for this document or its destruction.
	const char* to_string() const { return Rxml_document_to_string(id_); }

	int save(const char* path) const { return Rxml_document_save(id_, path); }

	/// Builds the widgets the document describes, as
	/// Rwidget_create_from_xml does. When the root is a <Window>, builds what
	/// the window would show (R_INVALID_ID when it is empty).
	Rid create_widget() const { return Rxml_document_create_widget(id_); }

	/// Builds the window the document describes, as Rwindow_create_from_xml does.
	Rid create_window() const { return Rxml_document_create_window(id_); }
};

class XmlNode : public Handle {
public:
	using Handle::Handle;
	constexpr XmlNode() noexcept = default;

	int is_valid() const { return Rxml_node_is_valid(id_); }

	Rid get_document() const { return Rxml_node_get_document(id_); }

	/// Returns: The tag of an element, "" for a comment.
	const char* get_tag() const { return Rxml_node_get_tag(id_); }

	void set_tag(const char* tag) const { Rxml_node_set_tag(id_, tag); }

	int is_comment() const { return Rxml_node_is_comment(id_); }

	/// The text inside an element (<Label>this</Label>), or the text of a comment.
	const char* get_text() const { return Rxml_node_get_text(id_); }

	void set_text(const char* text) const { Rxml_node_set_text(id_, text); }

	Rid get_parent() const { return Rxml_node_get_parent(id_); }

	int get_child_count() const { return Rxml_node_get_child_count(id_); }

	Rid get_child(int index) const { return Rxml_node_get_child(id_, index); }

	/// Returns: The place of a node among the children of its parent; 0 for the root.
	int get_index() const { return Rxml_node_get_index(id_); }

	/// index Where among the children; -1 for the end.
	Rid add_child(const char* tag, int index) const { return Rxml_node_add_child(id_, tag, index); }

	Rid add_comment(const char* text, int index) const { return Rxml_node_add_comment(id_, text, index); }

	/// Reads a piece of XML (one element and what it contains) and
	/// adds it: what a palette, a paste or a template inserts.
	Rid add_xml(const char* xml, size_t size, int index) const { return Rxml_node_add_xml(id_, xml, size, index); }

	/// Removes a node and what it contains. The root stays.
	void remove() const { Rxml_node_remove(id_); }

	/// Moves a node, with what it contains, under another parent of
	/// the same document or elsewhere under its own.
	/// index Where among the children of the new parent, counted once the node has left its place; -1 for the end.
	/// Returns: R_FALSE when it cannot be done (the root, or a parent that is inside the node).
	int move(Rid new_parent, int index) const { return Rxml_node_move(id_, new_parent, index); }

	/// Copies a node and what it contains, right after it. Returns the copy.
	Rid duplicate() const { return Rxml_node_duplicate(id_); }

	/// The node as text. Valid until the next call for this document.
	const char* to_string() const { return Rxml_node_to_string(id_); }

	int get_attribute_count() const { return Rxml_node_get_attribute_count(id_); }

	const char* get_attribute_name(int index) const { return Rxml_node_get_attribute_name(id_, index); }

	const char* get_attribute_value(int index) const { return Rxml_node_get_attribute_value(id_, index); }

	/// Returns: The value of an attribute (names are compared without case), or NULL when the node does not have it.
	const char* get_attribute(const char* name) const { return Rxml_node_get_attribute(id_, name); }

	/// Sets an attribute, added after the others when it is new. A NULL value removes it.
	void set_attribute(const char* name, const char* value) const { Rxml_node_set_attribute(id_, name, value); }

	/// Returns: The line of the node, starting at 1, in the text the document
	/// was read from or last written to; 0 for a node added since.
	int get_line() const { return Rxml_node_get_line(id_); }

	/// Returns: The widget (the window, for a <Window>) the node became the
	/// last time the document was built, or R_INVALID_ID: never built,
	/// destroyed since, or not a widget (an <Item>, a comment). A tag declared
	/// with <Widget> gives the widget it stands for.
	Rid get_widget() const { return Rxml_node_get_widget(id_); }
};

/// What a paint callback draws with.
class Painter {
public:
	constexpr Painter(RPainter* painter) noexcept : painter_(painter) {}
	constexpr operator RPainter*() const noexcept { return painter_; }

	/// Filled rectangle with rounded corners (radius 0 = sharp).
	void rect(float x, float y, float width, float height, float radius, float r, float g, float b, float a) const { Rdraw_rect(painter_, x, y, width, height, radius, r, g, b, a); }

	/// Outline of a rounded rectangle, drawn inside its bounds.
	void rect_outline(float x, float y, float width, float height, float radius, float thickness, float r, float g, float b, float a) const { Rdraw_rect_outline(painter_, x, y, width, height, radius, thickness, r, g, b, a); }

	void circle(float center_x, float center_y, float radius, float r, float g, float b, float a) const { Rdraw_circle(painter_, center_x, center_y, radius, r, g, b, a); }

	/// Anti-aliased line with round caps.
	void line(float x0, float y0, float x1, float y1, float thickness, float r, float g, float b, float a) const { Rdraw_line(painter_, x0, y0, x1, y1, thickness, r, g, b, a); }

	void triangle(float x0, float y0, float x1, float y1, float x2, float y2, float r, float g, float b, float a) const { Rdraw_triangle(painter_, x0, y0, x1, y1, x2, y2, r, g, b, a); }

	/// Texture stretched to a rectangle and multiplied by a tint color.
	void image(Rid texture, float x, float y, float width, float height, float r, float g, float b, float a) const { Rdraw_image(painter_, texture, x, y, width, height, r, g, b, a); }

	/// Draws UTF-8 text; (x, y) is the top-left corner of the first line.
	/// font R_INVALID_ID uses the theme font.
	/// size 0 uses R_METRIC_FONT_SIZE.
	void text(Rid font, float size, float x, float y, const char* text, float r, float g, float b, float a) const { Rdraw_text(painter_, font, size, x, y, text, r, g, b, a); }

	/// Rounded rectangle of glass. It shows what was drawn before it,
	/// frosted and bent along its rim, under a tint: (r, g, b) is the color of
	/// the tint and `a` its strength (0 = clear glass, 1 = opaque color).
	/// Works with any theme. Needs a backend that implements glass (the Vulkan
	/// backend does).
	void glass(float x, float y, float width, float height, float radius, float r, float g, float b, float a) const { Rdraw_glass(painter_, x, y, width, height, radius, r, g, b, a); }

	/// Animates a value from the paint callback of a custom widget.
	/// Moves the spring toward `target` by the time elapsed since the last
	/// frame and returns its value. While it moves, the window keeps drawing
	/// frames by itself. The first call puts the spring on its target.
	///
	///     static RSpring size;                                  (zeroed)
	///     float s = Rdraw_spring(painter, &size, hovered ? 1.0f : 0.0f, R_MOTION_BOUNCY);
	///     Rdraw_circle(painter, cx, cy, 20.0f + 8.0f * s, ...);
	float spring(RSpring* spring, float target, RMotion motion) const { return Rdraw_spring(painter_, spring, target, motion); }

	/// Restricts the following draws to a rectangle, intersected with the current one.
	void push_clip_rect(float x, float y, float width, float height) const { Rdraw_push_clip_rect(painter_, x, y, width, height); }

	void pop_clip_rect() const { Rdraw_pop_clip_rect(painter_); }

private:
	RPainter* painter_;
};

/// Starts the library with default settings on the given backend.
/// The calling thread becomes the UI thread.
/// Returns: R_TRUE on success. On failure, see Rerror_get_last.
inline int init(RBackend backend) { return Rinit(backend); }

/// Starts the library with explicit settings.
/// desc Settings, or NULL for the defaults.
inline int init_ex(const RInitDesc* desc) { return Rinit_ex(desc); }

/// Destroys every window, widget and resource, stops the worker and
/// render threads and releases the backend.
inline void shutdown() { Rshutdown(); }

inline int is_initialized() { return Ris_initialized(); }

/// Returns: The library version, e.g. "0.1.0".
inline const char* get_version() { return Rget_version(); }

/// Replaces the allocator of Raven with the functions of the
/// application (an engine allocator, a tracker...). NULL selects the
/// allocator of Raven again. Must be called before Rinit, and before any
/// other function that allocates. The functions are called from several
/// threads; alloc must return memory aligned for any type (16 bytes).
inline void set_allocator(const RAllocator* allocator) { Rset_allocator(allocator); }

/// Returns: An allocator that forwards to malloc, realloc and free of the C
/// runtime: Rset_allocator(Rallocator_get_system()) is how memory tools
/// that follow malloc (Valgrind, heap profilers) see the blocks of Raven.
/// The environment variable RAVEN_ALLOCATOR=system does the same without
/// recompiling, and it is the default in a build with AddressSanitizer.
inline const RAllocator* allocator_get_system() { return Rallocator_get_system(); }

/// Memory from the allocator of Raven (or from the one given to
/// Rset_allocator), for the application: custom widgets, pixel buffers.
/// Aligned on 16 bytes. Never returns NULL: running out of memory is a
/// fatal error. Thread-safe.
inline void* memory_alloc(size_t size) { return Rmemory_alloc(size); }

/// Resizes a block, keeping its content. A NULL block is allocated. Thread-safe.
inline void* memory_realloc(void* ptr, size_t size) { return Rmemory_realloc(ptr, size); }

/// Frees a block. NULL is ignored. Thread-safe.
inline void memory_free(void* ptr) { Rmemory_free(ptr); }

/// What the library holds right now. Thread-safe.
inline void memory_get_stats(RMemoryStats* stats) { Rmemory_get_stats(stats); }

/// Walks the whole heap of Raven and verifies its structure.
/// Returns: R_FALSE when it is corrupted (a buffer overrun wrote over a block
/// header). Slow: for tests and for hunting a bug. Thread-safe.
inline int memory_check() { return Rmemory_check(); }

/// Returns to the OS the regions of the heap that hold nothing. Thread-safe.
inline void memory_trim() { Rmemory_trim(); }

/// Produces one frame: processes the pending OS events without
/// waiting, runs the posted tasks and the timers, then lays out, paints and
/// submits every window that changed. A window in which nothing changed is
/// not drawn again, so the call is cheap when the UI is idle.
/// Returns: R_FALSE once Rquit was called or the last window is closed.
inline int draw_frame() { return Rdraw_frame(); }

/// Same as Rdraw_frame, but first sleeps until there is something to
/// do: an OS event, a posted task, a timer, an animation, a pending redraw.
/// timeout_seconds Longest sleep. Negative sleeps without limit, 0
/// does not sleep.
inline int wait_frame(double timeout_seconds) { return Rwait_frame(timeout_seconds); }

/// Makes Rdraw_frame and Rwait_frame return R_FALSE. Thread-safe.
inline void quit(int exit_code) { Rquit(exit_code); }

/// Returns: The code given to Rquit, 0 when the last window was closed.
inline int get_exit_code() { return Rget_exit_code(); }

/// Returns: R_FALSE once Rquit was called or every window is closed.
inline int is_running() { return Ris_running(); }

/// Processes the pending OS events without producing a frame.
/// Rdraw_frame does it already: only useful to keep the windows responsive
/// during a long operation.
inline void poll_events() { Rpoll_events(); }

/// Sleeps until an OS event, a posted task, a timer or a pending
/// redraw, and processes the OS events. Rwait_frame does it already.
/// timeout_seconds Maximum wait. Negative waits without limit.
inline void wait_events(double timeout_seconds) { Rwait_events(timeout_seconds); }

inline void set_frame_callback(RFrameCallback callback, void* user_data) { Rset_frame_callback(callback, user_data); }
inline void set_frame_callback(std::function<void(double)> callback) {
	Rset_frame_callback([](double delta_seconds, void* kept_) -> void { return (*static_cast<std::function<void(double)>*>(kept_))(delta_seconds); }, detail::keep(0, 34, std::move(callback)));
}

/// Returns: Seconds between the last two frames.
inline double get_delta_time() { return Rget_delta_time(); }

/// Returns: Seconds elapsed since Rinit, from a monotonic clock. Thread-safe.
inline double get_time() { return Rget_time(); }

/// R_FALSE makes every change immediate: for users who ask for
/// reduced motion, and for tests that read the state right after an input.
inline void set_animations_enabled(int enabled) { Rset_animations_enabled(enabled); }

inline int get_animations_enabled() { return Rget_animations_enabled(); }

/// Multiplies the speed of every animation. 1 is the default, 0.25 a slow motion.
inline void set_animation_speed(float speed) { Rset_animation_speed(speed); }

/// When positive, each frame advances the animations by exactly this
/// many seconds instead of the time really elapsed: frames rendered one by
/// one for a video or a screenshot, deterministic tests. 0 follows the clock.
inline void set_animation_time_step(double seconds) { Rset_animation_time_step(seconds); }

/// Queues a function to run on the UI thread during the next frame
/// and wakes Rwait_frame. Thread-safe: this is how worker threads touch the UI.
inline void task_post(RTaskCallback task, void* user_data) { Rtask_post(task, user_data); }
inline void task_post(std::function<void()> task) {
	Rtask_post([](void* kept_) -> void { return (*static_cast<std::function<void()>*>(kept_))(); }, detail::keep(0, 35, std::move(task)));
}

/// Runs a function on a worker thread of the job system. Thread-safe.
inline void task_run_async(RTaskCallback task, void* user_data) { Rtask_run_async(task, user_data); }
inline void task_run_async(std::function<void()> task) {
	Rtask_run_async([](void* kept_) -> void { return (*static_cast<std::function<void()>*>(kept_))(); }, detail::keep(0, 36, std::move(task)));
}

/// Splits [0, count) into ranges of at most `grain` items and runs them
/// on the worker threads and the calling thread. Returns when all are done.
inline void parallel_for(Ruint count, Ruint grain, RParallelRangeCallback function, void* user_data) { Rparallel_for(count, grain, function, user_data); }
inline void parallel_for(Ruint count, Ruint grain, std::function<void(Ruint, Ruint)> function) {
	Rparallel_for(count, grain, [](Ruint begin, Ruint end, void* kept_) -> void { return (*static_cast<std::function<void(Ruint, Ruint)>*>(kept_))(begin, end); }, detail::keep(0, 37, std::move(function)));
}

/// Wakes Rwait_frame and Rwait_events from any thread.
inline void wake_up() { Rwake_up(); }

inline int get_worker_thread_count() { return Rget_worker_thread_count(); }

inline int is_ui_thread() { return Ris_ui_thread(); }

inline void action_bind(const char* name, RActionCallback callback, void* user_data) { Raction_bind(name, callback, user_data); }
inline void action_bind(const char* name, std::function<void(Rid, const char*)> callback) {
	Raction_bind(name, [](Rid sender, const char* action, void* kept_) -> void { return (*static_cast<std::function<void(Rid, const char*)>*>(kept_))(sender, action); }, detail::keep(detail::key_of(name), 38, std::move(callback)));
}

inline void action_unbind(const char* name) { Raction_unbind(name); }

/// Fires an action as a widget would: the function bound to the
/// name is called, or the default handler. What a tool bar button, a
/// script or a test uses to do what a menu item does.
/// sender Passed to the function; R_INVALID_ID when there is none.
inline void action_fire(const char* name, Rid sender) { Raction_fire(name, sender); }

/// Receives every action that has no bound function (e.g. a script bridge).
inline void action_set_default_handler(RActionCallback callback, void* user_data) { Raction_set_default_handler(callback, user_data); }
inline void action_set_default_handler(std::function<void(Rid, const char*)> callback) {
	Raction_set_default_handler([](Rid sender, const char* action, void* kept_) -> void { return (*static_cast<std::function<void(Rid, const char*)>*>(kept_))(sender, action); }, detail::keep(0, 39, std::move(callback)));
}

inline void clipboard_set_text(const char* text) { Rclipboard_set_text(text); }

/// Returns: The clipboard text, or "" when empty. Valid until the next call.
inline const char* clipboard_get_text() { return Rclipboard_get_text(); }

/// Retrieves and clears the last recorded error.
/// detail   Optional output: human-readable description.
/// severity Optional output: severity of the error.
/// Returns: R_NO_ERROR when nothing was recorded.
inline RError error_get_last(const char** detail, RSeverity* severity) { return Rerror_get_last(detail, severity); }

inline const char* error_to_string(RError error) { return Rerror_to_string(error); }

inline const char* severity_to_string(RSeverity severity) { return Rseverity_to_string(severity); }

inline void error_set_callback(RErrorCallback callback) { Rerror_set_callback(callback); }

inline void pixels_free(uint8_t* pixels) { Rpixels_free(pixels); }

/// Switches to a built-in theme: replaces every theme color and
/// metric. Colors and metrics changed with Rtheme_set_color and
/// Rtheme_set_metric are lost; per-widget overrides are kept.
inline void theme_apply(RTheme theme) { Rtheme_apply(theme); }

inline void theme_set_color(RColorRole role, float r, float g, float b, float a) { Rtheme_set_color(role, r, g, b, a); }

inline void theme_get_color(RColorRole role, float* r, float* g, float* b, float* a) { Rtheme_get_color(role, r, g, b, a); }

inline void theme_set_metric(RMetric metric, float value) { Rtheme_set_metric(metric, value); }

inline float theme_get_metric(RMetric metric) { return Rtheme_get_metric(metric); }

/// Material of every widget that was not given one with
/// Rwidget_set_material. Rtheme_apply sets it: solid for the flat
/// themes, frosted glass for the glass themes.
inline void theme_set_material(RMaterial material) { Rtheme_set_material(material); }

inline RMaterial theme_get_material() { return Rtheme_get_material(); }

/// Font used by widgets that have no font of their own.
inline void theme_set_font(Rid font) { Rtheme_set_font(font); }

inline Rid theme_get_font() { return Rtheme_get_font(); }

/// Declares a language, or replaces the one that has this name: to
/// add words to a built-in language, read it with Rlanguage_get, change
/// what you want and register it again. The strings are copied.
inline int language_register(const RLanguageDesc* desc) { return Rlanguage_register(desc); }

/// Reads the description of a language. The strings belong to the library. Returns R_FALSE when it is unknown.
inline int language_get(const char* name, RLanguageDesc* desc) { return Rlanguage_get(name, desc); }

inline int language_get_count() { return Rlanguage_get_count(); }

inline const char* language_get_name(int index) { return Rlanguage_get_name(index); }

/// Returns: The name of the language of a file, from its extension, or NULL.
inline const char* language_for_file(const char* path) { return Rlanguage_for_file(path); }

/// Shows a widget as a dialog of a window. The window takes
/// ownership of `content`: it is deleted when the dialog closes, so read
/// what you need from it before, or in `closed`.
/// The Escape key dismisses the dialog (result 0).
inline void dialog_open(Rid window, Rid content, RDialogClosedCallback closed, void* user_data) { Rdialog_open(window, content, closed, user_data); }
inline void dialog_open(Rid window, Rid content, std::function<void(Rid, int)> closed) {
	Rdialog_open(window, content, [](Rid content, int result, void* kept_) -> void { return (*static_cast<std::function<void(Rid, int)>*>(kept_))(content, result); }, detail::keep(window, 40, std::move(closed)));
}

/// Closes the dialog that contains a widget (the content itself or
/// anything inside it: the button that was clicked).
/// result Passed to the closed callback and returned by Rdialog_run.
inline void dialog_close(Rid widget, int result) { Rdialog_close(widget, result); }

/// Shows a dialog and waits until it closes. Returns its result.
inline int dialog_run(Rid window, Rid content) { return Rdialog_run(window, content); }

/// Shows a message with buttons and waits for the answer.
/// window R_INVALID_ID: the first window of the application.
/// title  May be NULL.
inline RMessageResult message_box(Rid window, const char* title, const char* text, RMessageButtons buttons) { return Rmessage_box(window, title, text, buttons); }

/// Asks for a file to open.
/// desc May be NULL.
/// Returns: The path (UTF-8), valid until the next file dialog, or NULL when the user cancelled.
inline const char* file_dialog_open(const RFileDialogDesc* desc) { return Rfile_dialog_open(desc); }

/// Asks where to save a file. An existing file was confirmed by the user.
inline const char* file_dialog_save(const RFileDialogDesc* desc) { return Rfile_dialog_save(desc); }

/// Asks for a folder.
inline const char* file_dialog_select_folder(const RFileDialogDesc* desc) { return Rfile_dialog_select_folder(desc); }

/// What the last file dialog returned: several paths with R_FILE_DIALOG_MULTIPLE.
inline int file_dialog_get_result_count() { return Rfile_dialog_get_result_count(); }

inline const char* file_dialog_get_result(int index) { return Rfile_dialog_get_result(index); }

/// Whole files, with UTF-8 paths on every platform.
/// Returns: Rfile_read: the content followed by a '\0', to release with
/// Rmemory_free; NULL when the file cannot be read. `size` may be NULL.
inline char* file_read(const char* path, size_t* size) { return Rfile_read(path, size); }

inline int file_write(const char* path, const void* data, size_t size) { return Rfile_write(path, data, size); }

/// Pushes a spring away from its target (units per second): it bounces back.
inline void spring_kick(RSpring* spring, float velocity) { Rspring_kick(spring, velocity); }

/// Puts a spring at rest on a value, without animation.
inline void spring_set(RSpring* spring, float value) { Rspring_set(spring, value); }

/// Folder used to resolve relative paths found in XML (Source="...",
/// Font="..."). The *FromXmlFile functions use the folder of the file.
inline void xml_set_base_path(const char* path) { Rxml_set_base_path(path); }

/// Reads the <Widget Name="..."> declarations of a document (its
/// root, or the children of a <Widgets> root) so that the tags they
/// declare can be used in the documents loaded afterwards.
/// Returns: The number of tags declared.
inline int xml_register_widgets(const char* xml, size_t size) { return Rxml_register_widgets(xml, size); }

inline int xml_register_widgets_from_file(const char* path) { return Rxml_register_widgets_from_file(path); }

inline void xml_unregister_widget(const char* name) { Rxml_unregister_widget(name); }

inline int xml_is_widget_registered(const char* name) { return Rxml_is_widget_registered(name); }

/// The declared tags, for tools that list them.
inline int xml_get_registered_widget_count() { return Rxml_get_registered_widget_count(); }

inline const char* xml_get_registered_widget_name(int index) { return Rxml_get_registered_widget_name(index); }

/// The tags of the library, <Window> included. Those declared with <Widget> are listed by Rxml_get_registered_widget_name.
inline int xml_get_tag_count() { return Rxml_get_tag_count(); }

inline const char* xml_get_tag_name(int index) { return Rxml_get_tag_name(index); }

/// Returns: One line about a tag, "" when there is nothing to say.
inline const char* xml_get_tag_description(const char* tag) { return Rxml_get_tag_description(tag); }

/// Returns: 0: no child widget; 1: a single one; 2: any number. Declared and unknown tags give 2.
inline int xml_get_tag_children(const char* tag) { return Rxml_get_tag_children(tag); }

/// The attributes of a tag: its own first, then those every widget
/// has, then those of its place in its parent. For a tag declared with
/// <Widget>, its own are its parameters.
inline int xml_get_attribute_count(const char* tag) { return Rxml_get_attribute_count(tag); }

inline int xml_get_attribute_info(const char* tag, int index, RXmlAttributeInfo* info) { return Rxml_get_attribute_info(tag, index, info); }

} // namespace raven

#endif // RAVEN_HPP
