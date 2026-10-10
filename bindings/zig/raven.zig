// Raven for Zig.
//
// Generated from the C headers by tools/bindgen (Raven 0.1): do not edit.
//
// The C API as it is. Enumerations are c_int with constants, structures are
// extern structs whose fields default to zero (`.{}` is the zeroed structure
// of C), texts are `?[*:0]const u8`: a Zig string literal passes as it is.
//
//   const r = @import("raven.zig");
//
//   pub fn main() void {
//       _ = r.Rinit(r.R_BACKEND_VULKAN);
//       const xml = "<Window Title='Hi'><Button Text='Quit' OnClicked='quit'/></Window>";
//       _ = r.Rwindow_create_from_xml(xml, xml.len);
//       r.Raction_bind("quit", quit, null);
//       while (r.Rwait_frame(-1.0) != 0) {}
//       r.Rshutdown();
//   }
//
//   fn quit(sender: r.Rid, action: ?[*:0]const u8, user_data: ?*anyopaque) callconv(.C) void {
//       _ = sender; _ = action; _ = user_data;
//       r.Rquit(0);
//   }
//
// In build.zig: exe.linkSystemLibrary("raven"); exe.linkLibC();
// (Zig can also read the C header itself: @cImport(@cInclude("Raven.h")).)

pub const Rid = u32;
pub const Ruint = u32;
pub const RAVEN_VERSION_MAJOR: c_int = 0;
pub const RAVEN_VERSION_MINOR: c_int = 1;
pub const RAVEN_VERSION_PATCH: c_int = 0;
pub const RAVEN_API_VERSION = "0.1";
pub const R_FALSE: c_int = 0;
pub const R_TRUE: c_int = 1;
pub const R_INVALID_ID: Rid = 0xFFFFFFFF;

pub const RPainter = opaque {};

pub const RBackend = c_int;
/// Built-in Vulkan renderer.
pub const R_BACKEND_VULKAN: RBackend = 1;
/// Renders nothing. Used for headless runs and unit tests.
pub const R_BACKEND_NULL: RBackend = 2;
/// Backend supplied with Rbackend_register (see raven_backend.h).
pub const R_BACKEND_CUSTOM: RBackend = 3;

pub const RError = c_int;
pub const R_NO_ERROR: RError = 0x0070;
pub const R_ERROR_INVALID_ID: RError = 0x0071;
pub const R_ERROR_INVALID_ENUM: RError = 0x0072;
pub const R_ERROR_INVALID_VALUE: RError = 0x0073;
pub const R_ERROR_INVALID_OPERATION: RError = 0x0074;
pub const R_ERROR_BACKEND: RError = 0x0075;
pub const R_ERROR_PLATFORM: RError = 0x0076;
pub const R_ERROR_OUT_OF_MEMORY: RError = 0x0077;
pub const R_ERROR_INVALID_FILE_FORMAT: RError = 0x0078;
pub const R_ERROR_FILE_NOT_FOUND: RError = 0x0079;
pub const R_ERROR_XML: RError = 0x007A;
pub const R_ERROR_WRONG_THREAD: RError = 0x007B;

pub const RSeverity = c_int;
pub const R_SEVERITY_WEAK_WARNING: RSeverity = 0x0080;
pub const R_SEVERITY_WARNING: RSeverity = 0x0081;
pub const R_SEVERITY_ERROR: RSeverity = 0x0082;
pub const R_SEVERITY_FATAL: RSeverity = 0x0083;

pub const RWindowFlags = c_int;
pub const R_WINDOW_DEFAULT: RWindowFlags = 0;
pub const R_WINDOW_NO_RESIZE: RWindowFlags = 1;
pub const R_WINDOW_HIDDEN: RWindowFlags = 2;
pub const R_WINDOW_BORDERLESS: RWindowFlags = 4;
pub const R_WINDOW_MAXIMIZED: RWindowFlags = 8;
/// No title bar from the OS: the content covers the whole window and
/// draws its own header, with a R_WIDGET_HEADER_BAR (raven_shell.h) or
/// with any widget given to Rwidget_set_window_drag. The window is still
/// moved, resized, snapped and maximized by the OS, as it does for its own
/// title bars.
pub const R_WINDOW_CUSTOM_HEADER: RWindowFlags = 0x0010;

pub const RWindowSystem = c_int;
pub const R_WINDOW_SYSTEM_NONE: RWindowSystem = 0x0090;
pub const R_WINDOW_SYSTEM_WIN32: RWindowSystem = 0x0091;
pub const R_WINDOW_SYSTEM_X11: RWindowSystem = 0x0092;
pub const R_WINDOW_SYSTEM_WAYLAND: RWindowSystem = 0x0093;
pub const R_WINDOW_SYSTEM_COCOA: RWindowSystem = 0x0094;

/// Edges and corners of a window, for Rwindow_begin_resize.
pub const RWindowEdge = c_int;
pub const R_EDGE_LEFT: RWindowEdge = 0x00D0;
pub const R_EDGE_RIGHT: RWindowEdge = 0x00D1;
pub const R_EDGE_TOP: RWindowEdge = 0x00D2;
pub const R_EDGE_BOTTOM: RWindowEdge = 0x00D3;
pub const R_EDGE_TOP_LEFT: RWindowEdge = 0x00D4;
pub const R_EDGE_TOP_RIGHT: RWindowEdge = 0x00D5;
pub const R_EDGE_BOTTOM_LEFT: RWindowEdge = 0x00D6;
pub const R_EDGE_BOTTOM_RIGHT: RWindowEdge = 0x00D7;

pub const RMouseButton = c_int;
pub const R_MOUSE_LEFT: RMouseButton = 0;
pub const R_MOUSE_RIGHT: RMouseButton = 1;
pub const R_MOUSE_MIDDLE: RMouseButton = 2;
pub const R_MOUSE_X1: RMouseButton = 3;
pub const R_MOUSE_X2: RMouseButton = 4;

pub const RModifier = c_int;
pub const R_MOD_SHIFT: RModifier = 1;
pub const R_MOD_CTRL: RModifier = 2;
pub const R_MOD_ALT: RModifier = 4;
pub const R_MOD_SUPER: RModifier = 8;

pub const RCursor = c_int;
pub const R_CURSOR_ARROW: RCursor = 0x00B0;
pub const R_CURSOR_IBEAM: RCursor = 0x00B1;
pub const R_CURSOR_HAND: RCursor = 0x00B2;
pub const R_CURSOR_CROSSHAIR: RCursor = 0x00B3;
pub const R_CURSOR_RESIZE_H: RCursor = 0x00B4;
pub const R_CURSOR_RESIZE_V: RCursor = 0x00B5;
pub const R_CURSOR_RESIZE_NWSE: RCursor = 0x00B6;
pub const R_CURSOR_RESIZE_NESW: RCursor = 0x00B7;
pub const R_CURSOR_MOVE: RCursor = 0x00B8;
pub const R_CURSOR_NOT_ALLOWED: RCursor = 0x00B9;

/// Keyboard keys. Printable keys use their upper-case ASCII code.
pub const RKey = c_int;
pub const R_KEY_UNKNOWN: RKey = 0;
pub const R_KEY_SPACE: RKey = 0x0020;
pub const R_KEY_APOSTROPHE: RKey = 0x0027;
pub const R_KEY_COMMA: RKey = 0x002C;
pub const R_KEY_MINUS: RKey = 0x002D;
pub const R_KEY_PERIOD: RKey = 0x002E;
pub const R_KEY_SLASH: RKey = 0x002F;
pub const R_KEY_0: RKey = 0x0030;
pub const R_KEY_1: RKey = 0x0031;
pub const R_KEY_2: RKey = 0x0032;
pub const R_KEY_3: RKey = 0x0033;
pub const R_KEY_4: RKey = 0x0034;
pub const R_KEY_5: RKey = 0x0035;
pub const R_KEY_6: RKey = 0x0036;
pub const R_KEY_7: RKey = 0x0037;
pub const R_KEY_8: RKey = 0x0038;
pub const R_KEY_9: RKey = 0x0039;
pub const R_KEY_SEMICOLON: RKey = 0x003B;
pub const R_KEY_EQUAL: RKey = 0x003D;
pub const R_KEY_A: RKey = 0x0041;
pub const R_KEY_B: RKey = 0x0042;
pub const R_KEY_C: RKey = 0x0043;
pub const R_KEY_D: RKey = 0x0044;
pub const R_KEY_E: RKey = 0x0045;
pub const R_KEY_F: RKey = 0x0046;
pub const R_KEY_G: RKey = 0x0047;
pub const R_KEY_H: RKey = 0x0048;
pub const R_KEY_I: RKey = 0x0049;
pub const R_KEY_J: RKey = 0x004A;
pub const R_KEY_K: RKey = 0x004B;
pub const R_KEY_L: RKey = 0x004C;
pub const R_KEY_M: RKey = 0x004D;
pub const R_KEY_N: RKey = 0x004E;
pub const R_KEY_O: RKey = 0x004F;
pub const R_KEY_P: RKey = 0x0050;
pub const R_KEY_Q: RKey = 0x0051;
pub const R_KEY_R: RKey = 0x0052;
pub const R_KEY_S: RKey = 0x0053;
pub const R_KEY_T: RKey = 0x0054;
pub const R_KEY_U: RKey = 0x0055;
pub const R_KEY_V: RKey = 0x0056;
pub const R_KEY_W: RKey = 0x0057;
pub const R_KEY_X: RKey = 0x0058;
pub const R_KEY_Y: RKey = 0x0059;
pub const R_KEY_Z: RKey = 0x005A;
pub const R_KEY_LEFT_BRACKET: RKey = 0x005B;
pub const R_KEY_BACKSLASH: RKey = 0x005C;
pub const R_KEY_RIGHT_BRACKET: RKey = 0x005D;
pub const R_KEY_GRAVE: RKey = 0x0060;
pub const R_KEY_ESCAPE: RKey = 0x0100;
pub const R_KEY_ENTER: RKey = 0x0101;
pub const R_KEY_TAB: RKey = 0x0102;
pub const R_KEY_BACKSPACE: RKey = 0x0103;
pub const R_KEY_INSERT: RKey = 0x0104;
pub const R_KEY_DELETE: RKey = 0x0105;
pub const R_KEY_RIGHT: RKey = 0x0106;
pub const R_KEY_LEFT: RKey = 0x0107;
pub const R_KEY_DOWN: RKey = 0x0108;
pub const R_KEY_UP: RKey = 0x0109;
pub const R_KEY_PAGE_UP: RKey = 0x010A;
pub const R_KEY_PAGE_DOWN: RKey = 0x010B;
pub const R_KEY_HOME: RKey = 0x010C;
pub const R_KEY_END: RKey = 0x010D;
pub const R_KEY_CAPS_LOCK: RKey = 0x010E;
pub const R_KEY_PRINT_SCREEN: RKey = 0x010F;
pub const R_KEY_PAUSE: RKey = 0x0110;
pub const R_KEY_F1: RKey = 0x0111;
pub const R_KEY_F2: RKey = 0x0112;
pub const R_KEY_F3: RKey = 0x0113;
pub const R_KEY_F4: RKey = 0x0114;
pub const R_KEY_F5: RKey = 0x0115;
pub const R_KEY_F6: RKey = 0x0116;
pub const R_KEY_F7: RKey = 0x0117;
pub const R_KEY_F8: RKey = 0x0118;
pub const R_KEY_F9: RKey = 0x0119;
pub const R_KEY_F10: RKey = 0x011A;
pub const R_KEY_F11: RKey = 0x011B;
pub const R_KEY_F12: RKey = 0x011C;
pub const R_KEY_KP_0: RKey = 0x011D;
pub const R_KEY_KP_1: RKey = 0x011E;
pub const R_KEY_KP_2: RKey = 0x011F;
pub const R_KEY_KP_3: RKey = 0x0120;
pub const R_KEY_KP_4: RKey = 0x0121;
pub const R_KEY_KP_5: RKey = 0x0122;
pub const R_KEY_KP_6: RKey = 0x0123;
pub const R_KEY_KP_7: RKey = 0x0124;
pub const R_KEY_KP_8: RKey = 0x0125;
pub const R_KEY_KP_9: RKey = 0x0126;
pub const R_KEY_KP_DECIMAL: RKey = 0x0127;
pub const R_KEY_KP_DIVIDE: RKey = 0x0128;
pub const R_KEY_KP_MULTIPLY: RKey = 0x0129;
pub const R_KEY_KP_SUBTRACT: RKey = 0x012A;
pub const R_KEY_KP_ADD: RKey = 0x012B;
pub const R_KEY_KP_ENTER: RKey = 0x012C;
pub const R_KEY_LEFT_SHIFT: RKey = 0x012D;
pub const R_KEY_LEFT_CTRL: RKey = 0x012E;
pub const R_KEY_LEFT_ALT: RKey = 0x012F;
pub const R_KEY_LEFT_SUPER: RKey = 0x0130;
pub const R_KEY_RIGHT_SHIFT: RKey = 0x0131;
pub const R_KEY_RIGHT_CTRL: RKey = 0x0132;
pub const R_KEY_RIGHT_ALT: RKey = 0x0133;
pub const R_KEY_RIGHT_SUPER: RKey = 0x0134;
pub const R_KEY_MENU: RKey = 0x0135;
pub const R_KEY_COUNT: RKey = 0x0136;

pub const REventType = c_int;
pub const R_EVENT_NONE: REventType = 0x00C0;
pub const R_EVENT_MOUSE_MOVE: REventType = 0x00C1;
pub const R_EVENT_MOUSE_DOWN: REventType = 0x00C2;
pub const R_EVENT_MOUSE_UP: REventType = 0x00C3;
pub const R_EVENT_MOUSE_WHEEL: REventType = 0x00C4;
pub const R_EVENT_MOUSE_ENTER: REventType = 0x00C5;
pub const R_EVENT_MOUSE_LEAVE: REventType = 0x00C6;
pub const R_EVENT_KEY_DOWN: REventType = 0x00C7;
pub const R_EVENT_KEY_UP: REventType = 0x00C8;
/// A character was typed. `codepoint` holds the Unicode scalar value.
pub const R_EVENT_TEXT: REventType = 0x00C9;
pub const R_EVENT_FOCUS_GAINED: REventType = 0x00CA;
pub const R_EVENT_FOCUS_LOST: REventType = 0x00CB;

/// Moments of a drag coming from another application (files from the file manager, text...).
pub const RDropPhase = c_int;
/// The pointer came over the target.
pub const R_DROP_ENTER: RDropPhase = 0x00E0;
/// It moves over the target.
pub const R_DROP_MOVE: RDropPhase = 0x00E1;
/// It left the target without dropping.
pub const R_DROP_LEAVE: RDropPhase = 0x00E2;
/// The user dropped: `paths` and `text` hold what was dropped.
pub const R_DROP_DROPPED: RDropPhase = 0x00E3;

pub const RTheme = c_int;
/// Flat, dense themes for tools.
pub const R_THEME_DARK: RTheme = 0x00E0;
pub const R_THEME_LIGHT: RTheme = 0x00E1;
/// Glass themes: controls are capsules of translucent glass that refract
/// and blur what is behind them, catch the light on their rim and react
/// to the pointer with short animations. Windows get a soft colored
/// background unless they have their own (Rwindow_set_background,
/// Rwindow_set_clear_color).
pub const R_THEME_GLASS_DARK: RTheme = 0x00E2;
pub const R_THEME_GLASS_LIGHT: RTheme = 0x00E3;

/// What surfaces are made of, from the clearest to the most opaque. A theme
/// has one material for everything (Rtheme_set_material); any widget can be
/// given its own (Rwidget_set_material, Material="..." in XML).
///
/// The three glasses share the same anatomy (a tint over what is behind, a
/// rim that bends it and catches the light, a shadow) and differ by how much
/// they let through. Within one material, surfaces are also told apart by
/// their height above the window: bars lie flat, panels float a little,
/// controls a little more, menus and dialogs highest; each level has a
/// brighter rim and a deeper shadow than the one below.
pub const RMaterial = c_int;
/// Opaque: filled with the colors of the theme. The material of R_THEME_DARK and R_THEME_LIGHT.
pub const R_MATERIAL_SOLID: RMaterial = 0x02E0;
/// Clear glass: what is behind stays sharp enough to be recognized, strongly bent at the rim.
pub const R_MATERIAL_GLASS: RMaterial = 0x02E1;
/// Frosted glass: what is behind is blurred into soft colors. The material of the glass themes.
pub const R_MATERIAL_FROSTED_GLASS: RMaterial = 0x02E2;
/// Frosted glass colored by the accent color of the theme (or of the widget): for what must stand out.
pub const R_MATERIAL_TINTED_GLASS: RMaterial = 0x02E3;
/// Not a material: "the one of the theme". Removes the material given to a widget.
pub const R_MATERIAL_DEFAULT: RMaterial = 0x02E4;

/// Color roles. The comment lists the widgets reading each role.
pub const RColorRole = c_int;
/// Window clear color.
pub const R_COLOR_WINDOW: RColorRole = 0x0100;
/// Border background.
pub const R_COLOR_PANEL: RColorRole = 0x0101;
/// Border outline, list and input outlines.
pub const R_COLOR_BORDER: RColorRole = 0x0102;
/// Text. Inherited by the children of the widget it is set on.
pub const R_COLOR_TEXT: RColorRole = 0x0103;
pub const R_COLOR_TEXT_DISABLED: RColorRole = 0x0104;
pub const R_COLOR_TEXT_PLACEHOLDER: RColorRole = 0x0105;
/// Primary buttons, checked boxes, slider fill, progress fill.
pub const R_COLOR_ACCENT: RColorRole = 0x0106;
pub const R_COLOR_ACCENT_HOVERED: RColorRole = 0x0107;
pub const R_COLOR_ACCENT_PRESSED: RColorRole = 0x0108;
/// Text and marks drawn on top of the accent color.
pub const R_COLOR_ACCENT_TEXT: RColorRole = 0x0109;
/// Button, combo box, check box background.
pub const R_COLOR_CONTROL: RColorRole = 0x010A;
pub const R_COLOR_CONTROL_HOVERED: RColorRole = 0x010B;
pub const R_COLOR_CONTROL_PRESSED: RColorRole = 0x010C;
pub const R_COLOR_CONTROL_BORDER: RColorRole = 0x010D;
/// Text input, spin box, list and tree background.
pub const R_COLOR_INPUT: RColorRole = 0x010E;
/// Keyboard focus outline.
pub const R_COLOR_FOCUS: RColorRole = 0x010F;
/// Selected rows and selected text.
pub const R_COLOR_SELECTION: RColorRole = 0x0110;
pub const R_COLOR_ROW_HOVERED: RColorRole = 0x0111;
/// Slider track, progress background, scrollbar track.
pub const R_COLOR_TRACK: RColorRole = 0x0112;
/// Scrollbar thumb, slider handle.
pub const R_COLOR_HANDLE: RColorRole = 0x0113;
pub const R_COLOR_HANDLE_HOVERED: RColorRole = 0x0114;
pub const R_COLOR_SEPARATOR: RColorRole = 0x0115;
/// Menu bar, tab bar, expander header.
pub const R_COLOR_HEADER: RColorRole = 0x0116;
pub const R_COLOR_HEADER_HOVERED: RColorRole = 0x0117;
/// Active tab.
pub const R_COLOR_TAB_ACTIVE: RColorRole = 0x0118;
/// Menus, combo box lists.
pub const R_COLOR_POPUP: RColorRole = 0x0119;
pub const R_COLOR_POPUP_BORDER: RColorRole = 0x011A;
pub const R_COLOR_SHADOW: RColorRole = 0x011B;
pub const R_COLOR_TOOLTIP: RColorRole = 0x011C;
pub const R_COLOR_TOOLTIP_TEXT: RColorRole = 0x011D;
/// Track of a switch that is on.
pub const R_COLOR_SWITCH: RColorRole = 0x011E;

/// Sizes, in logical units.
pub const RMetric = c_int;
pub const R_METRIC_FONT_SIZE: RMetric = 0x0140;
pub const R_METRIC_CORNER_RADIUS: RMetric = 0x0141;
pub const R_METRIC_BORDER_WIDTH: RMetric = 0x0142;
/// Height of buttons, inputs, combo boxes, tabs, menu bars.
pub const R_METRIC_CONTROL_HEIGHT: RMetric = 0x0143;
/// Horizontal / vertical padding inside controls.
pub const R_METRIC_PADDING_X: RMetric = 0x0144;
pub const R_METRIC_PADDING_Y: RMetric = 0x0145;
/// Gap between an icon or a check mark and its text.
pub const R_METRIC_SPACING: RMetric = 0x0146;
pub const R_METRIC_SCROLLBAR_SIZE: RMetric = 0x0147;
/// Height of list, tree and menu rows.
pub const R_METRIC_ROW_HEIGHT: RMetric = 0x0148;
/// Indentation per tree level.
pub const R_METRIC_INDENT: RMetric = 0x0149;
pub const R_METRIC_SPLITTER_SIZE: RMetric = 0x014A;
pub const R_METRIC_CHECK_SIZE: RMetric = 0x014B;
pub const R_METRIC_SLIDER_HANDLE_SIZE: RMetric = 0x014C;
/// Corner radius of menus, drop-down lists and tooltips.
pub const R_METRIC_POPUP_RADIUS: RMetric = 0x014D;

pub const RFilterType = c_int;
pub const R_FILTER_LINEAR: RFilterType = 0x02C0;
/// No smoothing: keeps pixel art sharp.
pub const R_FILTER_NEAREST: RFilterType = 0x02C1;

pub const RWidgetType = c_int;
/// Stacks children top to bottom. Slate: SVerticalBox.
pub const R_WIDGET_VBOX: RWidgetType = 0x0200;
/// Stacks children left to right. Slate: SHorizontalBox.
pub const R_WIDGET_HBOX: RWidgetType = 0x0201;
/// Rows and columns with spans and fill coefficients. Slate: SGridPanel.
pub const R_WIDGET_GRID: RWidgetType = 0x0202;
/// Grid whose cells all have the same size. Slate: SUniformGridPanel.
pub const R_WIDGET_UNIFORM_GRID: RWidgetType = 0x0203;
/// Draws children on top of each other. Slate: SOverlay.
pub const R_WIDGET_OVERLAY: RWidgetType = 0x0204;
/// One child with a background, an outline and inner padding. Slate: SBorder.
pub const R_WIDGET_BORDER: RWidgetType = 0x0205;
/// One child with size constraints or a fixed aspect ratio. Slate: SBox.
pub const R_WIDGET_SIZE_BOX: RWidgetType = 0x0206;
/// Empty space. Slate: SSpacer.
pub const R_WIDGET_SPACER: RWidgetType = 0x0207;
/// Scrollable stack of children. Slate: SScrollBox.
pub const R_WIDGET_SCROLL_BOX: RWidgetType = 0x0208;
/// Children separated by draggable handles. Slate: SSplitter.
pub const R_WIDGET_SPLITTER: RWidgetType = 0x0209;
/// Flows children and wraps to the next line. Slate: SWrapBox.
pub const R_WIDGET_WRAP_BOX: RWidgetType = 0x020A;
/// Children placed with anchors and offsets. Slate: SConstraintCanvas.
pub const R_WIDGET_CANVAS: RWidgetType = 0x020B;
/// Shows one child at a time. Slate: SWidgetSwitcher.
pub const R_WIDGET_SWITCHER: RWidgetType = 0x020C;
/// Tab bar + one page per child.
pub const R_WIDGET_TAB_VIEW: RWidgetType = 0x020D;
/// Collapsible header + one child. Slate: SExpandableArea.
pub const R_WIDGET_EXPANDER: RWidgetType = 0x020E;
pub const R_WIDGET_LABEL: RWidgetType = 0x020F;
pub const R_WIDGET_BUTTON: RWidgetType = 0x0210;
pub const R_WIDGET_CHECKBOX: RWidgetType = 0x0211;
pub const R_WIDGET_SLIDER: RWidgetType = 0x0212;
pub const R_WIDGET_PROGRESSBAR: RWidgetType = 0x0213;
pub const R_WIDGET_IMAGE: RWidgetType = 0x0214;
pub const R_WIDGET_TEXT_INPUT: RWidgetType = 0x0215;
pub const R_WIDGET_SPIN_BOX: RWidgetType = 0x0216;
pub const R_WIDGET_COMBO_BOX: RWidgetType = 0x0217;
pub const R_WIDGET_SEPARATOR: RWidgetType = 0x0218;
pub const R_WIDGET_LIST_VIEW: RWidgetType = 0x0219;
pub const R_WIDGET_TREE_VIEW: RWidgetType = 0x021A;
pub const R_WIDGET_MENU_BAR: RWidgetType = 0x021B;
pub const R_WIDGET_MENU: RWidgetType = 0x021C;
pub const R_WIDGET_MENU_ITEM: RWidgetType = 0x021D;
/// Area showing content rendered by the application.
pub const R_WIDGET_VIEWPORT: RWidgetType = 0x021E;
/// Widget painted and measured by application callbacks.
pub const R_WIDGET_CUSTOM: RWidgetType = 0x021F;
/// Multi-line text editor.
pub const R_WIDGET_TEXT_EDITOR: RWidgetType = 0x0220;
/// Title bar of a window with a custom header, holding any widget.
pub const R_WIDGET_HEADER_BAR: RWidgetType = 0x0221;
/// Panels docked side by side or behind tabs, that the user rearranges.
pub const R_WIDGET_DOCK_AREA: RWidgetType = 0x0222;
/// A capsule that unfolds into a panel, at the top of a window.
pub const R_WIDGET_ISLAND: RWidgetType = 0x0223;
/// Explorer of the files of the computer.
pub const R_WIDGET_FILE_BROWSER: RWidgetType = 0x0224;

pub const RHAlign = c_int;
pub const R_HALIGN_FILL: RHAlign = 0x0240;
pub const R_HALIGN_LEFT: RHAlign = 0x0241;
pub const R_HALIGN_CENTER: RHAlign = 0x0242;
pub const R_HALIGN_RIGHT: RHAlign = 0x0243;

pub const RVAlign = c_int;
pub const R_VALIGN_FILL: RVAlign = 0x0250;
pub const R_VALIGN_TOP: RVAlign = 0x0251;
pub const R_VALIGN_CENTER: RVAlign = 0x0252;
pub const R_VALIGN_BOTTOM: RVAlign = 0x0253;

pub const RVisibility = c_int;
/// Drawn and laid out.
pub const R_VISIBLE: RVisibility = 0x0260;
/// Not drawn, but still takes its space in the layout.
pub const R_HIDDEN: RVisibility = 0x0261;
/// Not drawn and takes no space.
pub const R_COLLAPSED: RVisibility = 0x0262;

/// How a widget arrives. A transition animates what the widget and its
/// children draw; the layout is not affected, the widget is in place from the
/// first frame for its neighbours and for the mouse.
pub const RTransition = c_int;
pub const R_TRANSITION_NONE: RTransition = 0x0300;
/// Becomes opaque.
pub const R_TRANSITION_FADE: RTransition = 0x0301;
/// Grows from a smaller size and bounces a little past its own.
pub const R_TRANSITION_POP: RTransition = 0x0302;
/// Shrinks from a larger size.
pub const R_TRANSITION_ZOOM: RTransition = 0x0303;
/// Flies in from below / above / the right / the left, and settles with a bounce.
pub const R_TRANSITION_FLY_UP: RTransition = 0x0304;
pub const R_TRANSITION_FLY_DOWN: RTransition = 0x0305;
pub const R_TRANSITION_FLY_LEFT: RTransition = 0x0306;
pub const R_TRANSITION_FLY_RIGHT: RTransition = 0x0307;
/// Falls from above, squashes on landing and wobbles.
pub const R_TRANSITION_DROP: RTransition = 0x0308;
/// Short slide from the right / from the left, without bounce: pages that replace one another.
pub const R_TRANSITION_SLIDE_LEFT: RTransition = 0x0309;
pub const R_TRANSITION_SLIDE_RIGHT: RTransition = 0x030A;

pub const ROrientation = c_int;
pub const R_HORIZONTAL: ROrientation = 0x0270;
pub const R_VERTICAL: ROrientation = 0x0271;

pub const RTextAlign = c_int;
pub const R_TEXT_ALIGN_LEFT: RTextAlign = 0x0280;
pub const R_TEXT_ALIGN_CENTER: RTextAlign = 0x0281;
pub const R_TEXT_ALIGN_RIGHT: RTextAlign = 0x0282;

pub const RButtonStyle = c_int;
/// Filled with R_COLOR_CONTROL.
pub const R_BUTTON_DEFAULT: RButtonStyle = 0x0290;
/// Filled with R_COLOR_ACCENT.
pub const R_BUTTON_PRIMARY: RButtonStyle = 0x0291;
/// No background until hovered (toolbars).
pub const R_BUTTON_FLAT: RButtonStyle = 0x0292;

pub const RCheckboxStyle = c_int;
pub const R_CHECKBOX_CHECK: RCheckboxStyle = 0x02A0;
/// Round mark. Checking one unchecks the other radio boxes of the same parent.
pub const R_CHECKBOX_RADIO: RCheckboxStyle = 0x02A1;
/// On / off switch.
pub const R_CHECKBOX_SWITCH: RCheckboxStyle = 0x02A2;

pub const RStretch = c_int;
/// Stretches the image to the widget, ignoring its aspect ratio.
pub const R_STRETCH_FILL: RStretch = 0x02B0;
/// Largest size that fits entirely, keeping the aspect ratio.
pub const R_STRETCH_FIT: RStretch = 0x02B1;
/// Smallest size that covers the widget, keeping the aspect ratio (cropped).
pub const R_STRETCH_COVER: RStretch = 0x02B2;
/// Original size, centered.
pub const R_STRETCH_NONE: RStretch = 0x02B3;

/// Shorthands of Rtext_editor_set_language for two languages.
pub const RSyntax = c_int;
pub const R_SYNTAX_NONE: RSyntax = 0x0340;
pub const R_SYNTAX_XML: RSyntax = 0x0341;
pub const R_SYNTAX_C: RSyntax = 0x0342;

pub const RLanguageFlags = c_int;
/// Tags and attributes instead of code: XML, HTML. The words and the comments of the description are not used.
pub const R_LANGUAGE_MARKUP: RLanguageFlags = 1;
/// Selectors, properties and values: CSS.
pub const R_LANGUAGE_STYLE_SHEET: RLanguageFlags = 2;
/// A block comment may hold another one (Rust).
pub const R_LANGUAGE_NESTED_COMMENTS: RLanguageFlags = 4;
/// Between single quotes there is one character; any other single quote is not a text (C, Java, Rust).
pub const R_LANGUAGE_CHAR_LITERALS: RLanguageFlags = 8;
/// A line that ends with ':' indents the next one (Python).
pub const R_LANGUAGE_INDENT_ON_COLON: RLanguageFlags = 0x0010;
/// The words of the language are recognized whatever their case.
pub const R_LANGUAGE_IGNORE_CASE: RLanguageFlags = 0x0020;
/// Markup that holds JavaScript in script elements and CSS in style elements (HTML).
pub const R_LANGUAGE_EMBEDS: RLanguageFlags = 0x0040;

/// What a part of the text is, for the language of the editor (Rtext_editor_get_token).
pub const RToken = c_int;
pub const R_TOKEN_TEXT: RToken = 0x03A0;
pub const R_TOKEN_COMMENT: RToken = 0x03A1;
pub const R_TOKEN_STRING: RToken = 0x03A2;
pub const R_TOKEN_NUMBER: RToken = 0x03A3;
pub const R_TOKEN_KEYWORD: RToken = 0x03A4;
/// A type, or a name that is always there.
pub const R_TOKEN_TYPE: RToken = 0x03A5;
/// A name followed by an opening parenthesis.
pub const R_TOKEN_FUNCTION: RToken = 0x03A6;
/// The name of an element, in markup and in the selectors of a style sheet.
pub const R_TOKEN_TAG: RToken = 0x03A7;
/// The name of an attribute (markup), of a class or of a property (style sheets), of a key (JSON).
pub const R_TOKEN_ATTRIBUTE: RToken = 0x03A8;
pub const R_TOKEN_PUNCTUATION: RToken = 0x03A9;
/// A line for the preprocessor, an annotation, a declaration of a document.
pub const R_TOKEN_DIRECTIVE: RToken = 0x03AA;

/// What a marker says about the text it underlines.
pub const RMarkerKind = c_int;
pub const R_MARKER_ERROR: RMarkerKind = 0x0350;
pub const R_MARKER_WARNING: RMarkerKind = 0x0351;
pub const R_MARKER_INFO: RMarkerKind = 0x0352;

/// Buttons of a header bar.
pub const RHeaderButtons = c_int;
pub const R_HEADER_NO_BUTTONS: RHeaderButtons = 0;
pub const R_HEADER_CLOSE: RHeaderButtons = 1;
pub const R_HEADER_MINIMIZE: RHeaderButtons = 2;
pub const R_HEADER_MAXIMIZE: RHeaderButtons = 4;
pub const R_HEADER_ALL_BUTTONS: RHeaderButtons = 7;

/// Where a panel goes in a dock area, relative to another panel or to the whole area.
pub const RDockSide = c_int;
pub const R_DOCK_LEFT: RDockSide = 0x0360;
pub const R_DOCK_RIGHT: RDockSide = 0x0361;
pub const R_DOCK_TOP: RDockSide = 0x0362;
pub const R_DOCK_BOTTOM: RDockSide = 0x0363;
/// Behind the same tabs as the other panel.
pub const R_DOCK_TAB: RDockSide = 0x0364;

pub const RFileDialogFlags = c_int;
/// Several files can be chosen (Rfile_dialog_open): see Rfile_dialog_get_result.
pub const R_FILE_DIALOG_MULTIPLE: RFileDialogFlags = 1;
/// Never the dialog of the OS: the file browser of Raven, in a dialog of the parent window.
pub const R_FILE_DIALOG_RAVEN: RFileDialogFlags = 2;
pub const R_FILE_DIALOG_SHOW_HIDDEN: RFileDialogFlags = 4;

pub const RFileBrowserMode = c_int;
/// Choose an existing file.
pub const R_FILE_BROWSER_OPEN: RFileBrowserMode = 0x0380;
/// Choose where to save: a name is typed; an existing file must be confirmed.
pub const R_FILE_BROWSER_SAVE: RFileBrowserMode = 0x0381;
/// Choose a folder.
pub const R_FILE_BROWSER_FOLDER: RFileBrowserMode = 0x0382;

pub const RMessageButtons = c_int;
pub const R_MESSAGE_OK: RMessageButtons = 0x0390;
pub const R_MESSAGE_OK_CANCEL: RMessageButtons = 0x0391;
pub const R_MESSAGE_YES_NO: RMessageButtons = 0x0392;
pub const R_MESSAGE_YES_NO_CANCEL: RMessageButtons = 0x0393;

pub const RMessageResult = c_int;
/// Dismissed (Escape), or the Cancel button.
pub const R_MESSAGE_RESULT_CANCEL: RMessageResult = 0;
pub const R_MESSAGE_RESULT_OK: RMessageResult = 1;
pub const R_MESSAGE_RESULT_YES: RMessageResult = 2;
pub const R_MESSAGE_RESULT_NO: RMessageResult = 3;

/// How a spring moves. Animations in Raven are springs, as on iOS: a value is
/// pulled toward its target and may run past it before settling, which is what
/// makes a control feel alive rather than mechanical.
pub const RMotion = c_int;
/// Fast, no bounce: hover, highlights.
pub const R_MOTION_SNAPPY: RMotion = 0x0320;
/// Slower, no bounce: scrolling, fades, sizes.
pub const R_MOTION_SMOOTH: RMotion = 0x0321;
/// Runs a little past the target and comes back: switches, popups, pressed buttons.
pub const R_MOTION_BOUNCY: RMotion = 0x0322;
/// Swings several times around the target: things that fly in or get dropped.
pub const R_MOTION_WOBBLY: RMotion = 0x0323;

/// The kind of value an attribute takes.
pub const RXmlValueType = c_int;
pub const R_XML_TEXT: RXmlValueType = 0x03C0;
/// "true" or "false".
pub const R_XML_BOOL: RXmlValueType = 0x03C1;
pub const R_XML_NUMBER: RXmlValueType = 0x03C2;
/// One of the words of `choices`.
pub const R_XML_CHOICE: RXmlValueType = 0x03C3;
/// "#RGB", "#RRGGBB", "#RRGGBBAA", or a name: white, black, red, transparent...
pub const R_XML_COLOR: RXmlValueType = 0x03C4;
/// The name of an action (Raction_bind).
pub const R_XML_ACTION: RXmlValueType = 0x03C5;
/// The path of a file.
pub const R_XML_FILE: RXmlValueType = 0x03C6;
/// Several numbers separated by commas: "8,4", "0,0,1,1".
pub const R_XML_NUMBERS: RXmlValueType = 0x03C7;
/// Texts separated by '|': "One|Two|Three".
pub const R_XML_LIST: RXmlValueType = 0x03C8;

/// Registers a function called once per frame, before the layout.
/// The body of your loop does the same job; the callback is also called
/// while the OS keeps the thread in a loop of its own (on Windows, while
/// the user moves or resizes a window), when your loop is not running.
pub const RFrameCallback = ?*const fn (delta_seconds: f64, user_data: ?*anyopaque) callconv(.C) void;
pub const RTaskCallback = ?*const fn (user_data: ?*anyopaque) callconv(.C) void;
pub const RParallelRangeCallback = ?*const fn (begin: u32, end: u32, user_data: ?*anyopaque) callconv(.C) void;
pub const RTimerCallback = ?*const fn (timer: Rid, user_data: ?*anyopaque) callconv(.C) void;
/// Actions connect widgets to application code by name. A widget carries an
/// action name (set from XML with OnClicked="file.save", or from C with
/// Rwidget_set_action); when its main event fires, the function bound to that
/// name is called. Names are resolved when the event fires, so actions can be
/// bound before or after the widgets are created.
pub const RActionCallback = ?*const fn (sender: Rid, action: ?[*:0]const u8, user_data: ?*anyopaque) callconv(.C) void;
/// Registers a function called whenever an error is raised.
/// The function may be registered before Rinit. It is called on the
/// thread that raised the error: errors of the renderer (and the messages
/// of the Vulkan validation layers) come from the render thread.
pub const RErrorCallback = ?*const fn (code: RError, severity: RSeverity, detail: ?[*:0]const u8) callconv(.C) void;
/// Called for a drop target (a widget, or a window: `target` is its
/// handle). Return R_TRUE to accept: while the pointer is over the target this
/// shows the "can drop here" cursor and highlights the target; for
/// R_DROP_DROPPED it tells the source that the data was taken.
pub const RDropCallback = ?*const fn (target: Rid, event: ?*const RDropEvent, user_data: ?*anyopaque) callconv(.C) c_int;
/// Called when the user asks to close the window.
/// Return R_TRUE to let the window close, R_FALSE to keep it open.
pub const RWindowCloseCallback = ?*const fn (window: Rid, user_data: ?*anyopaque) callconv(.C) c_int;
/// Called after the window was resized. Sizes are in logical units.
pub const RWindowResizeCallback = ?*const fn (window: Rid, width: c_int, height: c_int, user_data: ?*anyopaque) callconv(.C) void;
/// Receives the input events of a widget before the widget itself.
/// Return R_TRUE to consume the event (the widget and its parents will
/// not see it), R_FALSE to let it continue.
pub const RWidgetEventCallback = ?*const fn (widget: Rid, event: ?*const REvent, user_data: ?*anyopaque) callconv(.C) c_int;
pub const RTabChangedCallback = ?*const fn (tab_view: Rid, index: c_int, user_data: ?*anyopaque) callconv(.C) void;
pub const RExpanderChangedCallback = ?*const fn (expander: Rid, expanded: c_int, user_data: ?*anyopaque) callconv(.C) void;
pub const RButtonClickedCallback = ?*const fn (button: Rid, user_data: ?*anyopaque) callconv(.C) void;
pub const RCheckboxChangedCallback = ?*const fn (checkbox: Rid, checked: c_int, user_data: ?*anyopaque) callconv(.C) void;
pub const RSliderChangedCallback = ?*const fn (slider: Rid, value: f32, user_data: ?*anyopaque) callconv(.C) void;
pub const RTextChangedCallback = ?*const fn (text_input: Rid, text: ?[*:0]const u8, user_data: ?*anyopaque) callconv(.C) void;
pub const RSpinBoxChangedCallback = ?*const fn (spin_box: Rid, value: f64, user_data: ?*anyopaque) callconv(.C) void;
pub const RComboBoxChangedCallback = ?*const fn (combo_box: Rid, index: c_int, user_data: ?*anyopaque) callconv(.C) void;
/// Virtual mode, for large or computed data: the list stores nothing and
/// asks for the text of the rows it draws. The returned string must stay
/// valid until the next call.
pub const RListItemTextCallback = ?*const fn (list_view: Rid, index: c_int, user_data: ?*anyopaque) callconv(.C) ?[*:0]const u8;
pub const RListEventCallback = ?*const fn (list_view: Rid, index: c_int, user_data: ?*anyopaque) callconv(.C) void;
pub const RTreeEventCallback = ?*const fn (tree_view: Rid, item: Rid, user_data: ?*anyopaque) callconv(.C) void;
pub const RMenuItemClickedCallback = ?*const fn (item: Rid, user_data: ?*anyopaque) callconv(.C) void;
/// Called each time the editor gathers its proposals, with the
/// start of the word being typed: add your own with
/// Rtext_editor_add_completion (the fields of a structure, the answers of
/// a language server...). They are listed before the others.
pub const RCompletionCallback = ?*const fn (editor: Rid, prefix: ?[*:0]const u8, user_data: ?*anyopaque) callconv(.C) void;
/// Called after each change made by the user (typing, pasting,
/// undoing...) or by Rtext_editor_insert_text. The action of the widget
/// (OnChanged in XML) fires at the same time.
pub const RTextEditorChangedCallback = ?*const fn (editor: Rid, user_data: ?*anyopaque) callconv(.C) void;
/// Called when the user closes a panel, before it is deleted.
/// Return R_FALSE to keep the panel.
pub const RDockCloseCallback = ?*const fn (area: Rid, panel: Rid, user_data: ?*anyopaque) callconv(.C) c_int;
/// Called after the user moved, resized, showed or closed a panel.
pub const RDockChangedCallback = ?*const fn (area: Rid, user_data: ?*anyopaque) callconv(.C) void;
/// Called when the user unfolds or folds the island. The action of the widget (OnChanged) fires too.
pub const RIslandChangedCallback = ?*const fn (island: Rid, expanded: c_int, user_data: ?*anyopaque) callconv(.C) void;
/// Called when a dialog closes. `result` is the value given to Rdialog_close, 0 when dismissed.
pub const RDialogClosedCallback = ?*const fn (content: Rid, result: c_int, user_data: ?*anyopaque) callconv(.C) void;
/// Called when the user chooses: a double click or Enter on a file,
/// the Open / Save button. `path` is the first chosen path; with several
/// files, read them with Rfile_browser_get_selection. The action of the
/// widget (OnActivated in XML) fires at the same time.
pub const RFileBrowserCallback = ?*const fn (browser: Rid, path: ?[*:0]const u8, user_data: ?*anyopaque) callconv(.C) void;
/// Called on the render thread each time the window is rendered.
/// State you bind (pipeline, descriptor sets, viewport, scissor) does not
/// need to be restored. NULL removes the callback.
pub const RViewportDrawCallback = ?*const fn (info: ?*const RViewportDrawInfo, user_data: ?*anyopaque) callconv(.C) void;
/// Called on the UI thread when the pixel size changes: resize your render target here.
pub const RViewportResizeCallback = ?*const fn (viewport: Rid, width: c_int, height: c_int, user_data: ?*anyopaque) callconv(.C) void;
pub const RPaintCallback = ?*const fn (widget: Rid, painter: ?*RPainter, width: f32, height: f32, user_data: ?*anyopaque) callconv(.C) void;
pub const RMeasureCallback = ?*const fn (widget: Rid, width: ?*f32, height: ?*f32, user_data: ?*anyopaque) callconv(.C) void;

/// Optional settings for Rinit_ex. A zero-initialized structure selects the
/// default for every field, so new fields can be added without breaking code.
pub const RInitDesc = extern struct {
    /// Rendering backend. 0 selects R_BACKEND_VULKAN.
    backend: RBackend = 0,
    /// Application name reported to the graphics driver. May be NULL.
    app_name: ?[*:0]const u8 = null,
    /// Worker threads of the job system. 0 = one per spare CPU core, negative = none.
    worker_threads: c_int = 0,
    /// R_TRUE renders on the UI thread instead of a dedicated render thread.
    disable_render_thread: c_int = 0,
    /// R_TRUE presents without waiting for the vertical blank.
    disable_vsync: c_int = 0,
    /// R_TRUE enables the backend debug layer (Vulkan validation) when installed.
    enable_validation: c_int = 0,
    /// R_TRUE opens no OS window (null platform). Combine with R_BACKEND_NULL for tests.
    headless: c_int = 0,
    /// Global UI scale multiplier applied on top of the monitor scale. 0 = 1.0.
    ui_scale: f32 = 0,
};

/// Memory functions of the application, to replace the allocator of Raven. See Rset_allocator.
pub const RAllocator = extern struct {
    alloc: ?*const anyopaque = null,
    realloc: ?*const anyopaque = null,
    free: ?*const anyopaque = null,
    user_data: ?*anyopaque = null,
};

/// What the library holds. See Rmemory_get_stats.
pub const RMemoryStats = extern struct {
    /// Bytes in the blocks currently allocated. 0 with the allocator of the application.
    used_bytes: usize = 0,
    /// Highest value of used_bytes since the start.
    peak_bytes: usize = 0,
    /// Bytes taken from the OS: used_bytes, plus what is free in the heap.
    reserved_bytes: usize = 0,
    /// Blocks currently allocated. 0 after Rshutdown: anything else is a leak of the library.
    allocation_count: usize = 0,
    /// Allocations made since the start.
    total_allocations: u64 = 0,
};

/// Timings and counters of the last frame produced for a window.
pub const RFrameStats = extern struct {
    layout_ms: f64 = 0,
    paint_ms: f64 = 0,
    render_ms: f64 = 0,
    widget_count: u32 = 0,
    instance_count: u32 = 0,
    draw_call_count: u32 = 0,
    /// Glass: times the frame was copied and blurred to be seen through (0 without glass).
    backdrop_count: u32 = 0,
    paint_job_count: u32 = 0,
    frame_index: u64 = 0,
};

/// Native handles of a window or of a native viewport.
///   Win32 : display = HINSTANCE, window = HWND
///   X11   : display = Display*,  window = Window (XID), visual = VisualID
pub const RNativeWindow = extern struct {
    system: RWindowSystem = 0,
    display: ?*anyopaque = null,
    window: usize = 0,
    visual: usize = 0,
};

/// Input event delivered to a widget. Mouse and key events bubble from the
/// target widget to its parents until one of them handles the event.
pub const REvent = extern struct {
    type_: REventType = 0,
    window: Rid = 0,
    /// Widget the event was first sent to.
    target: Rid = 0,
    /// Cursor position in the window, in logical units.
    x: f32 = 0,
    /// Cursor position in the window, in logical units.
    y: f32 = 0,
    /// Cursor position relative to the widget receiving the event.
    local_x: f32 = 0,
    /// Cursor position relative to the widget receiving the event.
    local_y: f32 = 0,
    /// Mouse movement since the previous event (R_EVENT_MOUSE_MOVE).
    delta_x: f32 = 0,
    /// Mouse movement since the previous event (R_EVENT_MOUSE_MOVE).
    delta_y: f32 = 0,
    /// Scroll amount in lines, positive = up / left (R_EVENT_MOUSE_WHEEL).
    wheel_x: f32 = 0,
    /// Scroll amount in lines, positive = up / left (R_EVENT_MOUSE_WHEEL).
    wheel_y: f32 = 0,
    button: RMouseButton = 0,
    /// 1 = single click, 2 = double click, ... (R_EVENT_MOUSE_DOWN).
    clicks: c_int = 0,
    key: RKey = 0,
    /// Bitmask of RModifier.
    modifiers: u32 = 0,
    /// R_TRUE when the key event comes from auto-repeat.
    repeat: c_int = 0,
    codepoint: u32 = 0,
};

/// What a drop target receives. The files and the text are only known when
/// the drop happens (R_DROP_DROPPED), and are valid during the callback.
pub const RDropEvent = extern struct {
    phase: RDropPhase = 0,
    window: Rid = 0,
    /// Pointer position in the window, and relative to the target widget.
    x: f32 = 0,
    /// Pointer position in the window, and relative to the target widget.
    y: f32 = 0,
    local_x: f32 = 0,
    local_y: f32 = 0,
    /// Dropped files and folders, as UTF-8 paths.
    paths: ?[*]const ?[*:0]const u8 = null,
    path_count: c_int = 0,
    /// Dropped text when it is not files, or NULL.
    text: ?[*:0]const u8 = null,
};

/// What the editor knows of a language. Every field but the name may be NULL.
/// Lists of words are separated by spaces.
pub const RLanguageDesc = extern struct {
    /// "Rust". Compared without case.
    name: ?[*:0]const u8 = null,
    /// Extensions of its files, separated by ';': "rs".
    extensions: ?[*:0]const u8 = null,
    /// Words of the language itself: if, while, return...
    keywords: ?[*:0]const u8 = null,
    /// Names of types and of what is always there: int, String, print...
    types: ?[*:0]const u8 = null,
    /// Values with a name: true, false, null...
    constants: ?[*:0]const u8 = null,
    /// What starts a comment that ends with its line: two slashes, a hash.
    line_comment: ?[*:0]const u8 = null,
    /// What opens and closes a comment that may span lines.
    block_comment_start: ?[*:0]const u8 = null,
    block_comment_end: ?[*:0]const u8 = null,
    /// Characters that open and close a text on one line: a double quote, a single quote.
    string_quotes: ?[*:0]const u8 = null,
    /// Marks of the texts that span lines, separated by spaces: three double
    /// quotes, a backquote. When what closes such a text is not what opens it,
    /// write both around three dots: R"(...)" . Nothing is escaped in those.
    multiline_strings: ?[*:0]const u8 = null,
    /// What starts a line for the preprocessor: "#".
    directive: ?[*:0]const u8 = null,
    /// What starts an annotation: "@" (@Override), or "#[" up to its closing bracket.
    annotation: ?[*:0]const u8 = null,
    /// Bitmask of RLanguageFlags.
    flags: u32 = 0,
};

/// A kind of file in a file dialog.
pub const RFileFilter = extern struct {
    /// Shown to the user: "Images".
    name: ?[*:0]const u8 = null,
    /// Patterns separated by ';': "*.png;*.jpg". "*" matches everything.
    patterns: ?[*:0]const u8 = null,
};

/// Every field may be left to zero.
pub const RFileDialogDesc = extern struct {
    /// Window the dialog belongs to. R_INVALID_ID: none (a window is opened when Raven needs one).
    parent: Rid = 0,
    /// NULL: "Open", "Save as"...
    title: ?[*:0]const u8 = null,
    /// Folder shown first. NULL: the one of the previous dialog, else the home of the user.
    directory: ?[*:0]const u8 = null,
    /// Name proposed to the user (save dialogs).
    file_name: ?[*:0]const u8 = null,
    filters: ?*const RFileFilter = null,
    filter_count: c_int = 0,
    /// Save dialogs: added to a name typed without extension ("xml").
    default_extension: ?[*:0]const u8 = null,
    /// Bitmask of RFileDialogFlags.
    flags: u32 = 0,
};

/// Passed to the draw callback of a viewport. Sizes are in pixels.
pub const RViewportDrawInfo = extern struct {
    backend: RBackend = 0,
    viewport: Rid = 0,
    window: Rid = 0,
    /// Area of the viewport in the window framebuffer.
    x: c_int = 0,
    /// Area of the viewport in the window framebuffer.
    y: c_int = 0,
    /// Area of the viewport in the window framebuffer.
    width: c_int = 0,
    /// Area of the viewport in the window framebuffer.
    height: c_int = 0,
    /// Visible part of that area. Already set as the scissor rectangle.
    clip_x: c_int = 0,
    /// Visible part of that area. Already set as the scissor rectangle.
    clip_y: c_int = 0,
    /// Visible part of that area. Already set as the scissor rectangle.
    clip_width: c_int = 0,
    /// Visible part of that area. Already set as the scissor rectangle.
    clip_height: c_int = 0,
    framebuffer_width: c_int = 0,
    framebuffer_height: c_int = 0,
    frame_index: u64 = 0,
    /// Vulkan: VkCommandBuffer being recorded, inside the render pass.
    command_buffer: ?*anyopaque = null,
    /// Vulkan: VkRenderPass in use (one color attachment, no depth).
    render_pass: ?*anyopaque = null,
    /// Vulkan: VkFormat of the color attachment.
    color_format: u32 = 0,
};

/// State of one animated value. Zero-initialize it and keep it with the widget.
pub const RSpring = extern struct {
    value: f32 = 0,
    velocity: f32 = 0,
    started: c_int = 0,
};

/// Where and why a text could not be read (Rxml_document_parse).
pub const RXmlError = extern struct {
    /// Both start at 1.
    line: c_int = 0,
    column: c_int = 0,
    message: [160]u8 = [_]u8{0} ** 160,
};

/// One attribute of a tag (Rxml_get_attribute_info). The strings belong to the library.
pub const RXmlAttributeInfo = extern struct {
    name: ?[*:0]const u8 = null,
    type_: RXmlValueType = 0,
    /// R_XML_CHOICE: the words, separated by '|'. NULL otherwise.
    choices: ?[*:0]const u8 = null,
    /// What it belongs to: the tag itself ("Button"), "Widget" for what every widget has, "Slot" for the place in the parent.
    group: ?[*:0]const u8 = null,
    /// One line saying what it does.
    description: ?[*:0]const u8 = null,
};

/// Starts the library with default settings on the given backend.
/// The calling thread becomes the UI thread.
/// Returns: R_TRUE on success. On failure, see Rerror_get_last.
pub extern fn Rinit(backend: RBackend) c_int;
/// Starts the library with explicit settings.
/// desc Settings, or NULL for the defaults.
pub extern fn Rinit_ex(desc: ?*const RInitDesc) c_int;
/// Destroys every window, widget and resource, stops the worker and
/// render threads and releases the backend.
pub extern fn Rshutdown() void;
pub extern fn Ris_initialized() c_int;
/// Returns: The library version, e.g. "0.1.0".
pub extern fn Rget_version() ?[*:0]const u8;
/// Replaces the allocator of Raven with the functions of the
/// application (an engine allocator, a tracker...). NULL selects the
/// allocator of Raven again. Must be called before Rinit, and before any
/// other function that allocates. The functions are called from several
/// threads; alloc must return memory aligned for any type (16 bytes).
pub extern fn Rset_allocator(allocator: ?*const RAllocator) void;
/// Returns: An allocator that forwards to malloc, realloc and free of the C
/// runtime: Rset_allocator(Rallocator_get_system()) is how memory tools
/// that follow malloc (Valgrind, heap profilers) see the blocks of Raven.
/// The environment variable RAVEN_ALLOCATOR=system does the same without
/// recompiling, and it is the default in a build with AddressSanitizer.
pub extern fn Rallocator_get_system() ?*const RAllocator;
/// Memory from the allocator of Raven (or from the one given to
/// Rset_allocator), for the application: custom widgets, pixel buffers.
/// Aligned on 16 bytes. Never returns NULL: running out of memory is a
/// fatal error. Thread-safe.
pub extern fn Rmemory_alloc(size: usize) ?*anyopaque;
/// Resizes a block, keeping its content. A NULL block is allocated. Thread-safe.
pub extern fn Rmemory_realloc(ptr: ?*anyopaque, size: usize) ?*anyopaque;
/// Frees a block. NULL is ignored. Thread-safe.
pub extern fn Rmemory_free(ptr: ?*anyopaque) void;
/// What the library holds right now. Thread-safe.
pub extern fn Rmemory_get_stats(stats: ?*RMemoryStats) void;
/// Walks the whole heap of Raven and verifies its structure.
/// Returns: R_FALSE when it is corrupted (a buffer overrun wrote over a block
/// header). Slow: for tests and for hunting a bug. Thread-safe.
pub extern fn Rmemory_check() c_int;
/// Returns to the OS the regions of the heap that hold nothing. Thread-safe.
pub extern fn Rmemory_trim() void;
/// Produces one frame: processes the pending OS events without
/// waiting, runs the posted tasks and the timers, then lays out, paints and
/// submits every window that changed. A window in which nothing changed is
/// not drawn again, so the call is cheap when the UI is idle.
/// Returns: R_FALSE once Rquit was called or the last window is closed.
pub extern fn Rdraw_frame() c_int;
/// Same as Rdraw_frame, but first sleeps until there is something to
/// do: an OS event, a posted task, a timer, an animation, a pending redraw.
/// timeout_seconds Longest sleep. Negative sleeps without limit, 0
/// does not sleep.
pub extern fn Rwait_frame(timeout_seconds: f64) c_int;
/// Makes Rdraw_frame and Rwait_frame return R_FALSE. Thread-safe.
pub extern fn Rquit(exit_code: c_int) void;
/// Returns: The code given to Rquit, 0 when the last window was closed.
pub extern fn Rget_exit_code() c_int;
/// Returns: R_FALSE once Rquit was called or every window is closed.
pub extern fn Ris_running() c_int;
/// Processes the pending OS events without producing a frame.
/// Rdraw_frame does it already: only useful to keep the windows responsive
/// during a long operation.
pub extern fn Rpoll_events() void;
/// Sleeps until an OS event, a posted task, a timer or a pending
/// redraw, and processes the OS events. Rwait_frame does it already.
/// timeout_seconds Maximum wait. Negative waits without limit.
pub extern fn Rwait_events(timeout_seconds: f64) void;
pub extern fn Rset_frame_callback(callback: RFrameCallback, user_data: ?*anyopaque) void;
/// Returns: Seconds between the last two frames.
pub extern fn Rget_delta_time() f64;
/// Returns: Seconds elapsed since Rinit, from a monotonic clock. Thread-safe.
pub extern fn Rget_time() f64;
/// R_FALSE makes every change immediate: for users who ask for
/// reduced motion, and for tests that read the state right after an input.
pub extern fn Rset_animations_enabled(enabled: c_int) void;
pub extern fn Rget_animations_enabled() c_int;
/// Multiplies the speed of every animation. 1 is the default, 0.25 a slow motion.
pub extern fn Rset_animation_speed(speed: f32) void;
/// When positive, each frame advances the animations by exactly this
/// many seconds instead of the time really elapsed: frames rendered one by
/// one for a video or a screenshot, deterministic tests. 0 follows the clock.
pub extern fn Rset_animation_time_step(seconds: f64) void;
/// Queues a function to run on the UI thread during the next frame
/// and wakes Rwait_frame. Thread-safe: this is how worker threads touch the UI.
pub extern fn Rtask_post(task: RTaskCallback, user_data: ?*anyopaque) void;
/// Runs a function on a worker thread of the job system. Thread-safe.
pub extern fn Rtask_run_async(task: RTaskCallback, user_data: ?*anyopaque) void;
/// Splits [0, count) into ranges of at most `grain` items and runs them
/// on the worker threads and the calling thread. Returns when all are done.
pub extern fn Rparallel_for(count: u32, grain: u32, function: RParallelRangeCallback, user_data: ?*anyopaque) void;
/// Wakes Rwait_frame and Rwait_events from any thread.
pub extern fn Rwake_up() void;
pub extern fn Rget_worker_thread_count() c_int;
pub extern fn Ris_ui_thread() c_int;
/// Calls a function on the UI thread after `interval_seconds`.
/// repeat R_TRUE to fire repeatedly, R_FALSE to fire once and delete itself.
pub extern fn Rtimer_create(interval_seconds: f64, repeat: c_int, callback: RTimerCallback, user_data: ?*anyopaque) Rid;
pub extern fn Rtimer_destroy(timer: Rid) void;
pub extern fn Rtimer_is_valid(timer: Rid) c_int;
pub extern fn Raction_bind(name: ?[*:0]const u8, callback: RActionCallback, user_data: ?*anyopaque) void;
pub extern fn Raction_unbind(name: ?[*:0]const u8) void;
/// Fires an action as a widget would: the function bound to the
/// name is called, or the default handler. What a tool bar button, a
/// script or a test uses to do what a menu item does.
/// sender Passed to the function; R_INVALID_ID when there is none.
pub extern fn Raction_fire(name: ?[*:0]const u8, sender: Rid) void;
/// Receives every action that has no bound function (e.g. a script bridge).
pub extern fn Raction_set_default_handler(callback: RActionCallback, user_data: ?*anyopaque) void;
pub extern fn Rclipboard_set_text(text: ?[*:0]const u8) void;
/// Returns: The clipboard text, or "" when empty. Valid until the next call.
pub extern fn Rclipboard_get_text() ?[*:0]const u8;
/// Retrieves and clears the last recorded error.
/// detail   Optional output: human-readable description.
/// severity Optional output: severity of the error.
/// Returns: R_NO_ERROR when nothing was recorded.
pub extern fn Rerror_get_last(detail: ?*?[*:0]const u8, severity: ?*RSeverity) RError;
pub extern fn Rerror_to_string(error_: RError) ?[*:0]const u8;
pub extern fn Rseverity_to_string(severity: RSeverity) ?[*:0]const u8;
pub extern fn Rerror_set_callback(callback: RErrorCallback) void;
pub extern fn Rwindow_get_frame_stats(window: Rid, stats: ?*RFrameStats) void;
/// Opens a top-level window.
/// width,height Client size in logical units.
/// flags        Bitmask of RWindowFlags.
pub extern fn Rwindow_create(title: ?[*:0]const u8, width: c_int, height: c_int, flags: u32) Rid;
/// Closes a window and deletes its content widgets.
pub extern fn Rwindow_destroy(window: Rid) void;
pub extern fn Rwindow_is_valid(window: Rid) c_int;
/// Sets the root widget of the window. The window takes ownership of
/// the widget; the previous content is deleted.
pub extern fn Rwindow_set_content(window: Rid, widget: Rid) void;
pub extern fn Rwindow_get_content(window: Rid) Rid;
pub extern fn Rwindow_set_title(window: Rid, title: ?[*:0]const u8) void;
pub extern fn Rwindow_get_title(window: Rid) ?[*:0]const u8;
pub extern fn Rwindow_set_size(window: Rid, width: c_int, height: c_int) void;
pub extern fn Rwindow_get_size(window: Rid, width: ?*c_int, height: ?*c_int) void;
pub extern fn Rwindow_set_min_size(window: Rid, width: c_int, height: c_int) void;
pub extern fn Rwindow_set_position(window: Rid, x: c_int, y: c_int) void;
pub extern fn Rwindow_get_position(window: Rid, x: ?*c_int, y: ?*c_int) void;
/// Size of the drawable area in pixels.
pub extern fn Rwindow_get_framebuffer_size(window: Rid, width: ?*c_int, height: ?*c_int) void;
/// Returns: Pixels per logical unit (monitor scale x global UI scale).
pub extern fn Rwindow_get_scale(window: Rid) f32;
pub extern fn Rwindow_show(window: Rid) void;
pub extern fn Rwindow_hide(window: Rid) void;
pub extern fn Rwindow_maximize(window: Rid) void;
pub extern fn Rwindow_minimize(window: Rid) void;
pub extern fn Rwindow_restore(window: Rid) void;
pub extern fn Rwindow_is_maximized(window: Rid) c_int;
pub extern fn Rwindow_is_minimized(window: Rid) c_int;
/// Returns: R_TRUE while the window is the one the user is working in.
pub extern fn Rwindow_is_active(window: Rid) c_int;
/// Asks to close the window, as the close button of its title bar
/// does: the close callback is asked first and may refuse.
pub extern fn Rwindow_close(window: Rid) void;
/// Custom headers. Call these from a mouse-down handler, while the left
/// button is held: the window follows the pointer until the button is
/// released (the widget does not receive the release). A
/// R_WIDGET_HEADER_BAR and the widgets given to Rwidget_set_window_drag do
/// it by themselves.
pub extern fn Rwindow_begin_move(window: Rid) void;
pub extern fn Rwindow_begin_resize(window: Rid, edge: RWindowEdge) void;
/// Color behind the content. By default it is the theme color
/// R_COLOR_WINDOW (and, with a glass theme, the wallpaper of the theme).
pub extern fn Rwindow_set_clear_color(window: Rid, r: f32, g: f32, b: f32, a: f32) void;
/// Image drawn behind the content, scaled to cover the window:
/// what the glass of a glass theme looks through. R_INVALID_ID removes it.
pub extern fn Rwindow_set_background(window: Rid, texture: Rid) void;
/// Asks for a new frame even if no widget changed. Thread-safe.
pub extern fn Rwindow_request_redraw(window: Rid) void;
/// R_TRUE redraws the window every frame (games, live viewports).
pub extern fn Rwindow_set_continuous_redraw(window: Rid, enabled: c_int) void;
pub extern fn Rwindow_set_close_callback(window: Rid, callback: RWindowCloseCallback, user_data: ?*anyopaque) void;
pub extern fn Rwindow_set_resize_callback(window: Rid, callback: RWindowResizeCallback, user_data: ?*anyopaque) void;
/// Takes what is dropped on the window from other applications,
/// where no widget takes it (Rwidget_set_drop_callback). NULL removes it.
pub extern fn Rwindow_set_drop_callback(window: Rid, callback: RDropCallback, user_data: ?*anyopaque) void;
pub extern fn Rwindow_set_user_handle(window: Rid, handle: ?*anyopaque) void;
pub extern fn Rwindow_get_user_handle(window: Rid) ?*anyopaque;
/// Fills `native` with the OS handles of the window.
pub extern fn Rwindow_get_native_handle(window: Rid, native: ?*RNativeWindow) c_int;
/// Reads back the last rendered frame as tightly packed RGBA8 pixels.
/// Blocks until the frame is rendered. Free the result with Rpixels_free.
/// Returns: NULL when the backend cannot capture.
pub extern fn Rwindow_capture(window: Rid, width: ?*c_int, height: ?*c_int) ?[*]u8;
pub extern fn Rpixels_free(pixels: ?[*]u8) void;
/// Widget that has the keyboard focus in the window, or R_INVALID_ID.
pub extern fn Rwindow_get_focused_widget(window: Rid) Rid;
/// Deepest widget under the cursor, or R_INVALID_ID.
pub extern fn Rwindow_get_hovered_widget(window: Rid) Rid;
pub extern fn Rwindow_get_mouse_position(window: Rid, x: ?*f32, y: ?*f32) void;
/// Inject input as if it came from the OS. Positions are in logical units.
/// Useful for automated tests, replays and when Raven is driven by a host
/// application.
pub extern fn Rwindow_inject_mouse_move(window: Rid, x: f32, y: f32) void;
pub extern fn Rwindow_inject_mouse_button(window: Rid, button: RMouseButton, pressed: c_int) void;
pub extern fn Rwindow_inject_mouse_wheel(window: Rid, wheel_x: f32, wheel_y: f32) void;
pub extern fn Rwindow_inject_key(window: Rid, key: RKey, pressed: c_int, modifiers: u32) void;
pub extern fn Rwindow_inject_text(window: Rid, text: ?[*:0]const u8) void;
/// A drag from another application moves over the window
/// (`inside` R_TRUE) or leaves it.
/// Returns: R_TRUE when a drop at this position would be accepted.
pub extern fn Rwindow_inject_drag(window: Rid, x: f32, y: f32, inside: c_int) c_int;
/// Drops files and / or text at a position. Returns R_TRUE when a target took them.
pub extern fn Rwindow_inject_drop(window: Rid, x: f32, y: f32, paths: ?[*]const ?[*:0]const u8, path_count: c_int, text: ?[*:0]const u8) c_int;
/// Registers a keyboard shortcut active while the window has focus and
/// no widget handled the key.
/// action Name of the action to fire (see Raction_bind).
pub extern fn Rwindow_register_shortcut(window: Rid, key: RKey, modifiers: u32, action: ?[*:0]const u8) void;
pub extern fn Rwindow_unregister_shortcut(window: Rid, key: RKey, modifiers: u32) void;
/// Switches to a built-in theme: replaces every theme color and
/// metric. Colors and metrics changed with Rtheme_set_color and
/// Rtheme_set_metric are lost; per-widget overrides are kept.
pub extern fn Rtheme_apply(theme: RTheme) void;
pub extern fn Rtheme_set_color(role: RColorRole, r: f32, g: f32, b: f32, a: f32) void;
pub extern fn Rtheme_get_color(role: RColorRole, r: ?*f32, g: ?*f32, b: ?*f32, a: ?*f32) void;
pub extern fn Rtheme_set_metric(metric: RMetric, value: f32) void;
pub extern fn Rtheme_get_metric(metric: RMetric) f32;
/// Material of every widget that was not given one with
/// Rwidget_set_material. Rtheme_apply sets it: solid for the flat
/// themes, frosted glass for the glass themes.
pub extern fn Rtheme_set_material(material: RMaterial) void;
pub extern fn Rtheme_get_material() RMaterial;
/// Font used by widgets that have no font of their own.
pub extern fn Rtheme_set_font(font: Rid) void;
pub extern fn Rtheme_get_font() Rid;
/// Creates a font from the content of a .ttf / .otf file.
/// The buffer is copied. Glyphs are rasterized on demand, at the exact
/// pixel size needed by each window scale, into a shared atlas.
/// Returns: R_INVALID_ID on failure.
pub extern fn Rfont_create(data: ?[*:0]const u8, data_size: usize) Rid;
pub extern fn Rfont_create_from_file(path: ?[*:0]const u8) Rid;
pub extern fn Rfont_destroy(font: Rid) void;
pub extern fn Rfont_is_valid(font: Rid) c_int;
/// Returns: The font embedded in the library (Roboto, Latin and Latin Extended-A).
pub extern fn Rfont_get_default() Rid;
/// Returns: The fixed-width font embedded in the library (Roboto Mono, same
/// characters), used by default by the text editors. In XML: Font="monospace".
pub extern fn Rfont_get_monospace() Rid;
/// Font used for the characters missing from `font` (symbols, icons,
/// other scripts). Fallbacks can be chained.
pub extern fn Rfont_set_fallback(font: Rid, fallback: Rid) void;
/// Measures a UTF-8 string, in logical units, at scale 1.
/// font R_INVALID_ID uses the theme font.
/// size Font size; 0 uses R_METRIC_FONT_SIZE.
pub extern fn Rfont_measure_text(font: Rid, size: f32, text: ?[*:0]const u8, width: ?*f32, height: ?*f32) void;
/// Creates a texture from the content of an image file (PNG, JPEG,
/// BMP, TGA, GIF, PSD). Decoding happens on the calling thread.
/// Returns: R_INVALID_ID on failure.
pub extern fn Rtexture_create(data: ?[*:0]const u8, data_size: usize) Rid;
/// Same as Rtexture_create, decoded on a worker thread. The handle
/// is usable at once; widgets showing it draw nothing until it is ready.
/// The buffer is copied.
pub extern fn Rtexture_create_async(data: ?[*:0]const u8, data_size: usize) Rid;
pub extern fn Rtexture_create_from_file(path: ?[*:0]const u8) Rid;
/// Reads and decodes the file on a worker thread.
pub extern fn Rtexture_create_from_file_async(path: ?[*:0]const u8) Rid;
/// Creates a texture from tightly packed RGBA8 pixels (straight alpha).
pub extern fn Rtexture_create_from_pixels(rgba: ?[*]const u8, width: c_int, height: c_int) Rid;
/// Replaces the pixels of a texture. The size may change. Use it to
/// show frames produced on the CPU or read back from another renderer.
pub extern fn Rtexture_update(texture: Rid, rgba: ?[*]const u8, width: c_int, height: c_int) void;
/// Returns: R_TRUE once an asynchronous texture is decoded (or failed).
pub extern fn Rtexture_is_ready(texture: Rid) c_int;
/// Blocks until an asynchronous texture is ready.
pub extern fn Rtexture_wait(texture: Rid) void;
pub extern fn Rtexture_destroy(texture: Rid) void;
pub extern fn Rtexture_is_valid(texture: Rid) c_int;
pub extern fn Rtexture_get_size(texture: Rid, width: ?*c_int, height: ?*c_int) void;
pub extern fn Rtexture_set_filter(texture: Rid, filter: RFilterType) void;
/// Creates a widget. It is not shown until it is added to a parent
/// that belongs to a window (Rwidget_add_child, Rwindow_set_content).
pub extern fn Rwidget_create(type_: RWidgetType) Rid;
/// Deletes a widget and all its descendants.
pub extern fn Rwidget_destroy(widget: Rid) void;
pub extern fn Rwidget_is_valid(widget: Rid) c_int;
pub extern fn Rwidget_get_type(widget: Rid) RWidgetType;
/// Appends a child to a panel. The parent takes ownership.
/// A widget that already has a parent is moved.
/// Returns: `child`, so calls can be nested:
///         Rid ok = Rwidget_add_child(row, Rbutton_create("OK"));
pub extern fn Rwidget_add_child(parent: Rid, child: Rid) Rid;
pub extern fn Rwidget_insert_child(parent: Rid, child: Rid, index: c_int) Rid;
/// Detaches a child without deleting it. The caller owns it again.
pub extern fn Rwidget_remove_child(parent: Rid, child: Rid) void;
/// Deletes every child of a panel.
pub extern fn Rwidget_clear_children(parent: Rid) void;
pub extern fn Rwidget_get_parent(widget: Rid) Rid;
pub extern fn Rwidget_get_child_count(widget: Rid) c_int;
pub extern fn Rwidget_get_child(widget: Rid, index: c_int) Rid;
/// Returns: The window showing the widget, or R_INVALID_ID.
pub extern fn Rwidget_get_window(widget: Rid) Rid;
pub extern fn Rwidget_set_name(widget: Rid, name: ?[*:0]const u8) void;
pub extern fn Rwidget_get_name(widget: Rid) ?[*:0]const u8;
/// Finds a widget by name.
/// scope A window or a widget; the search covers its whole subtree.
/// Returns: R_INVALID_ID when no widget has that name.
pub extern fn Rwidget_find(scope: Rid, name: ?[*:0]const u8) Rid;
/// Space kept around the child, inside its slot. All panels.
pub extern fn Rslot_set_padding(child: Rid, left: f32, top: f32, right: f32, bottom: f32) void;
/// Alignment of the child inside its slot. Default: fill. All panels.
pub extern fn Rslot_set_halign(child: Rid, align_: RHAlign) void;
pub extern fn Rslot_set_valign(child: Rid, align_: RVAlign) void;
/// The slot takes the desired size of the child along the panel
/// direction (Slate AutoWidth / AutoHeight). This is the default.
/// VBox, HBox.
pub extern fn Rslot_set_auto(child: Rid) void;
/// The slot shares the space left by the auto slots, in proportion
/// to `coefficient` (Slate FillWidth / FillHeight). VBox, HBox.
/// In a Splitter, the coefficient is the relative size of the pane.
pub extern fn Rslot_set_fill(child: Rid, coefficient: f32) void;
/// Upper bound of the slot along the panel direction. 0 = none. VBox, HBox.
pub extern fn Rslot_set_max_size(child: Rid, max_size: f32) void;
/// Cell of the child. Grid, UniformGrid.
pub extern fn Rslot_set_grid(child: Rid, column: c_int, row: c_int) void;
pub extern fn Rslot_set_grid_span(child: Rid, column_span: c_int, row_span: c_int) void;
/// Canvas placement. Anchors are fractions of the canvas size (0..1).
///   - anchors equal on an axis: the child is at anchor + position, with
///     the slot size (or its desired size when the slot size is <= 0);
///   - anchors different on an axis: the child stretches between them,
///     position and size are then the near and far margins.
/// Alignment is the pivot of the child (0,0 = top-left, 0.5,0.5 = center).
pub extern fn Rslot_set_anchors(child: Rid, min_x: f32, min_y: f32, max_x: f32, max_y: f32) void;
pub extern fn Rslot_set_position(child: Rid, x: f32, y: f32) void;
pub extern fn Rslot_set_size(child: Rid, width: f32, height: f32) void;
pub extern fn Rslot_set_alignment(child: Rid, x: f32, y: f32) void;
/// Title of the tab showing the child. TabView.
pub extern fn Rslot_set_title(child: Rid, title: ?[*:0]const u8) void;
pub extern fn Rwidget_set_visibility(widget: Rid, visibility: RVisibility) void;
pub extern fn Rwidget_get_visibility(widget: Rid) RVisibility;
/// A disabled widget ignores input and is drawn dimmed, with its descendants.
pub extern fn Rwidget_set_enabled(widget: Rid, enabled: c_int) void;
pub extern fn Rwidget_is_enabled(widget: Rid) c_int;
/// R_FALSE lets mouse events pass through the widget (not its children).
pub extern fn Rwidget_set_hit_testable(widget: Rid, hit_testable: c_int) void;
/// Opacity multiplier applied to the widget and its descendants (0..1).
pub extern fn Rwidget_set_opacity(widget: Rid, opacity: f32) void;
/// Gives the widget a transition that plays each time it appears:
/// when its window is first drawn, when it is added to a window that is
/// shown, and when it becomes visible again (Rwidget_set_visibility, a
/// tab or a switcher showing it). In XML: Appear="FlyUp" AppearDelay="0.1".
/// delay_seconds Time before it starts. Give growing delays to the
/// items of a group to make them arrive one after the other.
pub extern fn Rwidget_set_transition(widget: Rid, transition: RTransition, delay_seconds: f32) void;
/// Plays a transition once, now, whatever the widget was given with Rwidget_set_transition.
pub extern fn Rwidget_play_transition(widget: Rid, transition: RTransition, delay_seconds: f32) void;
/// R_TRUE clips the descendants to the bounds of the widget.
pub extern fn Rwidget_set_clipping(widget: Rid, clip: c_int) void;
/// Text shown when the cursor rests on the widget. NULL removes it.
pub extern fn Rwidget_set_tooltip(widget: Rid, text: ?[*:0]const u8) void;
/// Returns: The tooltip text of the widget, "" when it has none.
pub extern fn Rwidget_get_tooltip(widget: Rid) ?[*:0]const u8;
pub extern fn Rwidget_set_cursor(widget: Rid, cursor: RCursor) void;
/// Size constraints, in logical units. They apply to the desired size the
/// widget reports to its parent. A negative value removes the constraint
/// on that axis.
pub extern fn Rwidget_set_size(widget: Rid, width: f32, height: f32) void;
pub extern fn Rwidget_set_min_size(widget: Rid, width: f32, height: f32) void;
pub extern fn Rwidget_set_max_size(widget: Rid, width: f32, height: f32) void;
/// Bounds of the widget in its window, in logical units, after the last layout.
pub extern fn Rwidget_get_rect(widget: Rid, x: ?*f32, y: ?*f32, width: ?*f32, height: ?*f32) void;
pub extern fn Rwidget_get_desired_size(widget: Rid, width: ?*f32, height: ?*f32) void;
pub extern fn Rwidget_is_hovered(widget: Rid) c_int;
pub extern fn Rwidget_set_focusable(widget: Rid, focusable: c_int) void;
/// Gives the keyboard focus to the widget.
pub extern fn Rwidget_focus(widget: Rid) void;
pub extern fn Rwidget_is_focused(widget: Rid) c_int;
pub extern fn Rwidget_set_user_handle(widget: Rid, handle: ?*anyopaque) void;
pub extern fn Rwidget_get_user_handle(widget: Rid) ?*anyopaque;
pub extern fn Rwidget_set_event_callback(widget: Rid, callback: RWidgetEventCallback, user_data: ?*anyopaque) void;
/// Name of the action fired by the main event of the widget: button
/// click, value change, text commit, item activation (see Raction_bind).
pub extern fn Rwidget_set_action(widget: Rid, action: ?[*:0]const u8) void;
pub extern fn Rwidget_get_action(widget: Rid) ?[*:0]const u8;
/// Makes the widget a target for what is dragged from other
/// applications: files from the file manager, text. It is highlighted
/// while a drop on it would be accepted. NULL removes the callback.
pub extern fn Rwidget_set_drop_callback(widget: Rid, callback: RDropCallback, user_data: ?*anyopaque) void;
/// In a window created with R_WINDOW_CUSTOM_HEADER: dragging the
/// widget moves the window and a double click maximizes it, as a title bar
/// does. The buttons, fields and other controls inside it keep working.
/// In XML: WindowDrag="true".
pub extern fn Rwidget_set_window_drag(widget: Rid, enabled: c_int) void;
/// Deepest widget of a subtree under a point of its window, or R_INVALID_ID.
pub extern fn Rwidget_hit_test(root: Rid, x: f32, y: f32) Rid;
/// Deepest visible widget of a subtree under a point of its window,
/// whether it reacts to the pointer or not: a label, a panel, a disabled
/// button. What a design tool selects with a click.
pub extern fn Rwidget_pick(root: Rid, x: f32, y: f32) Rid;
/// Menu opened by a right click on the widget. The widget does not own it.
pub extern fn Rwidget_set_context_menu(widget: Rid, menu: Rid) void;
/// Asks for the widget to be laid out and painted again.
pub extern fn Rwidget_invalidate(widget: Rid) void;
/// Overrides a theme color for one widget. R_COLOR_TEXT is also
/// inherited by its descendants.
pub extern fn Rwidget_set_color(widget: Rid, role: RColorRole, r: f32, g: f32, b: f32, a: f32) void;
pub extern fn Rwidget_clear_color(widget: Rid, role: RColorRole) void;
/// Gives one widget a material of its own, whatever the theme:
/// a solid button in a glass window, a panel of clear glass in a flat one.
/// A widget that draws no surface (a box, a label) gets one of this
/// material behind it: any widget can become a panel of glass.
/// In XML: Material="Glass | FrostedGlass | TintedGlass | Solid".
/// material R_MATERIAL_DEFAULT follows the theme again.
pub extern fn Rwidget_set_material(widget: Rid, material: RMaterial) void;
/// Returns: The material the widget is drawn with: its own, or the one of the theme.
pub extern fn Rwidget_get_material(widget: Rid) RMaterial;
/// Overrides a theme metric for one widget.
pub extern fn Rwidget_set_metric(widget: Rid, metric: RMetric, value: f32) void;
pub extern fn Rwidget_clear_metric(widget: Rid, metric: RMetric) void;
/// Gap inserted between neighbouring children.
pub extern fn Rpanel_set_spacing(panel: Rid, spacing: f32) void;
/// Lets a column / row grow to take a share of the free space
/// (Slate FillColumn / FillRow). 0 sizes it to its content (default).
pub extern fn Rgrid_set_column_fill(grid: Rid, column: c_int, coefficient: f32) void;
pub extern fn Rgrid_set_row_fill(grid: Rid, row: c_int, coefficient: f32) void;
/// Places children automatically, left to right then top to bottom,
/// in the given number of columns. 0 uses the grid cell of each slot (default).
pub extern fn Runiform_grid_set_columns(grid: Rid, columns: c_int) void;
pub extern fn Runiform_grid_set_min_cell_size(grid: Rid, width: f32, height: f32) void;
/// Space between the outline and the child.
pub extern fn Rborder_set_padding(border: Rid, left: f32, top: f32, right: f32, bottom: f32) void;
/// Background color (R_COLOR_PANEL of this widget). Alpha 0 draws no background.
pub extern fn Rborder_set_background(border: Rid, r: f32, g: f32, b: f32, a: f32) void;
/// Outline width and color. Width 0 draws no outline (default).
pub extern fn Rborder_set_outline(border: Rid, width: f32, r: f32, g: f32, b: f32, a: f32) void;
pub extern fn Rborder_set_corner_radius(border: Rid, top_left: f32, top_right: f32, bottom_right: f32, bottom_left: f32) void;
/// Soft drop shadow behind the border. Blur 0 disables it (default).
pub extern fn Rborder_set_shadow(border: Rid, blur: f32, offset_x: f32, offset_y: f32) void;
/// What the border is made of. R_MATERIAL_GLASS turns it into a
/// panel of glass tinted by its background color (the alpha of the color
/// is the strength of the tint): floating tool bars, side bars, cards.
pub extern fn Rborder_set_material(border: Rid, material: RMaterial) void;
/// Keeps the child at a fixed width / height ratio, as large as fits.
/// 0 disables it (default). The fixed, minimum and maximum sizes are set
/// with Rwidget_set_size, Rwidget_set_min_size and Rwidget_set_max_size.
pub extern fn Rsize_box_set_aspect_ratio(size_box: Rid, ratio: f32) void;
pub extern fn Rspacer_set_size(spacer: Rid, width: f32, height: f32) void;
/// Direction in which children are stacked and scrolled. Default: R_VERTICAL.
pub extern fn Rscroll_box_set_orientation(scroll_box: Rid, orientation: ROrientation) void;
/// R_TRUE also scrolls across the stacking direction when the content is too large.
pub extern fn Rscroll_box_set_scroll_both(scroll_box: Rid, enabled: c_int) void;
pub extern fn Rscroll_box_set_offset(scroll_box: Rid, x: f32, y: f32) void;
pub extern fn Rscroll_box_get_offset(scroll_box: Rid, x: ?*f32, y: ?*f32) void;
pub extern fn Rscroll_box_scroll_to_end(scroll_box: Rid) void;
/// Scrolls every ScrollBox above the widget so that the widget is visible.
pub extern fn Rwidget_scroll_into_view(widget: Rid) void;
/// R_HORIZONTAL puts the panes side by side (default), R_VERTICAL stacks them.
pub extern fn Rsplitter_set_orientation(splitter: Rid, orientation: ROrientation) void;
pub extern fn Rsplitter_set_min_pane_size(splitter: Rid, size: f32) void;
pub extern fn Rswitcher_set_index(switcher: Rid, index: c_int) void;
pub extern fn Rswitcher_get_index(switcher: Rid) c_int;
/// Adds a page. Same as Rwidget_add_child followed by Rslot_set_title.
/// Returns: The index of the new tab, or -1.
pub extern fn Rtab_view_add_tab(tab_view: Rid, title: ?[*:0]const u8, content: Rid) c_int;
pub extern fn Rtab_view_set_active(tab_view: Rid, index: c_int) void;
pub extern fn Rtab_view_get_active(tab_view: Rid) c_int;
pub extern fn Rtab_view_set_changed_callback(tab_view: Rid, callback: RTabChangedCallback, user_data: ?*anyopaque) void;
pub extern fn Rexpander_set_title(expander: Rid, title: ?[*:0]const u8) void;
pub extern fn Rexpander_set_expanded(expander: Rid, expanded: c_int) void;
pub extern fn Rexpander_is_expanded(expander: Rid) c_int;
pub extern fn Rexpander_set_changed_callback(expander: Rid, callback: RExpanderChangedCallback, user_data: ?*anyopaque) void;
/// Shortcut for Rwidget_create(R_WIDGET_LABEL) + Rlabel_set_text.
pub extern fn Rlabel_create(text: ?[*:0]const u8) Rid;
/// Sets the text. '\n' starts a new line.
pub extern fn Rlabel_set_text(label: Rid, text: ?[*:0]const u8) void;
pub extern fn Rlabel_get_text(label: Rid) ?[*:0]const u8;
/// R_INVALID_ID uses the theme font.
pub extern fn Rlabel_set_font(label: Rid, font: Rid) void;
/// Font size in logical units. 0 uses R_METRIC_FONT_SIZE.
pub extern fn Rlabel_set_text_size(label: Rid, size: f32) void;
pub extern fn Rlabel_set_text_align(label: Rid, align_: RTextAlign) void;
/// R_TRUE wraps the text on the width given by the parent.
pub extern fn Rlabel_set_wrap(label: Rid, wrap: c_int) void;
/// Text color (R_COLOR_TEXT of this widget).
pub extern fn Rlabel_set_color(label: Rid, r: f32, g: f32, b: f32, a: f32) void;
/// Shortcut for Rwidget_create(R_WIDGET_BUTTON) + Rbutton_set_text.
pub extern fn Rbutton_create(text: ?[*:0]const u8) Rid;
/// Sets the content of the button to a label. A button can instead
/// hold any single child widget (Rwidget_add_child), e.g. an HBox with an icon.
pub extern fn Rbutton_set_text(button: Rid, text: ?[*:0]const u8) void;
pub extern fn Rbutton_set_style(button: Rid, style: RButtonStyle) void;
pub extern fn Rbutton_set_clicked_callback(button: Rid, callback: RButtonClickedCallback, user_data: ?*anyopaque) void;
pub extern fn Rcheckbox_create(text: ?[*:0]const u8) Rid;
pub extern fn Rcheckbox_set_text(checkbox: Rid, text: ?[*:0]const u8) void;
pub extern fn Rcheckbox_set_checked(checkbox: Rid, checked: c_int) void;
pub extern fn Rcheckbox_is_checked(checkbox: Rid) c_int;
pub extern fn Rcheckbox_set_style(checkbox: Rid, style: RCheckboxStyle) void;
pub extern fn Rcheckbox_set_changed_callback(checkbox: Rid, callback: RCheckboxChangedCallback, user_data: ?*anyopaque) void;
pub extern fn Rslider_set_range(slider: Rid, minimum: f32, maximum: f32) void;
pub extern fn Rslider_set_value(slider: Rid, value: f32) void;
pub extern fn Rslider_get_value(slider: Rid) f32;
/// Snaps the value to multiples of `step`. 0 = continuous (default).
pub extern fn Rslider_set_step(slider: Rid, step: f32) void;
pub extern fn Rslider_set_orientation(slider: Rid, orientation: ROrientation) void;
pub extern fn Rslider_set_changed_callback(slider: Rid, callback: RSliderChangedCallback, user_data: ?*anyopaque) void;
/// Progress between 0 and 1.
pub extern fn Rprogress_bar_set_value(progress_bar: Rid, value: f32) void;
pub extern fn Rprogress_bar_get_value(progress_bar: Rid) f32;
pub extern fn Rimage_set_texture(image: Rid, texture: Rid) void;
pub extern fn Rimage_set_tint_color(image: Rid, r: f32, g: f32, b: f32, a: f32) void;
pub extern fn Rimage_set_stretch(image: Rid, stretch: RStretch) void;
/// Part of the texture to show, in UV coordinates. Default: 0, 0, 1, 1.
pub extern fn Rimage_set_region(image: Rid, min_u: f32, min_v: f32, max_u: f32, max_v: f32) void;
pub extern fn Rimage_set_corner_radius(image: Rid, radius: f32) void;
pub extern fn Rtext_input_set_text(text_input: Rid, text: ?[*:0]const u8) void;
pub extern fn Rtext_input_get_text(text_input: Rid) ?[*:0]const u8;
/// Dimmed text shown while the input is empty.
pub extern fn Rtext_input_set_placeholder(text_input: Rid, text: ?[*:0]const u8) void;
pub extern fn Rtext_input_set_password(text_input: Rid, password: c_int) void;
pub extern fn Rtext_input_set_read_only(text_input: Rid, read_only: c_int) void;
/// Maximum number of characters. 0 = no limit (default).
pub extern fn Rtext_input_set_max_length(text_input: Rid, max_length: c_int) void;
pub extern fn Rtext_input_select_all(text_input: Rid) void;
/// Called on every edit.
pub extern fn Rtext_input_set_changed_callback(text_input: Rid, callback: RTextChangedCallback, user_data: ?*anyopaque) void;
/// Called when Enter is pressed. This also fires the action of the widget.
pub extern fn Rtext_input_set_committed_callback(text_input: Rid, callback: RTextChangedCallback, user_data: ?*anyopaque) void;
pub extern fn Rspin_box_set_range(spin_box: Rid, minimum: f64, maximum: f64) void;
pub extern fn Rspin_box_set_value(spin_box: Rid, value: f64) void;
pub extern fn Rspin_box_get_value(spin_box: Rid) f64;
pub extern fn Rspin_box_set_step(spin_box: Rid, step: f64) void;
/// Digits shown after the decimal point. Default: 2.
pub extern fn Rspin_box_set_decimals(spin_box: Rid, decimals: c_int) void;
pub extern fn Rspin_box_set_changed_callback(spin_box: Rid, callback: RSpinBoxChangedCallback, user_data: ?*anyopaque) void;
/// Returns: The index of the new item.
pub extern fn Rcombo_box_add_item(combo_box: Rid, text: ?[*:0]const u8) c_int;
pub extern fn Rcombo_box_clear_items(combo_box: Rid) void;
pub extern fn Rcombo_box_get_item_count(combo_box: Rid) c_int;
pub extern fn Rcombo_box_get_item_text(combo_box: Rid, index: c_int) ?[*:0]const u8;
/// Selects an item. -1 selects nothing.
pub extern fn Rcombo_box_set_selected(combo_box: Rid, index: c_int) void;
pub extern fn Rcombo_box_get_selected(combo_box: Rid) c_int;
pub extern fn Rcombo_box_set_changed_callback(combo_box: Rid, callback: RComboBoxChangedCallback, user_data: ?*anyopaque) void;
/// R_HORIZONTAL draws a horizontal line (default).
pub extern fn Rseparator_set_orientation(separator: Rid, orientation: ROrientation) void;
/// Returns: The index of the new item.
pub extern fn Rlist_view_add_item(list_view: Rid, text: ?[*:0]const u8) c_int;
pub extern fn Rlist_view_set_item_text(list_view: Rid, index: c_int, text: ?[*:0]const u8) void;
pub extern fn Rlist_view_get_item_text(list_view: Rid, index: c_int) ?[*:0]const u8;
pub extern fn Rlist_view_remove_item(list_view: Rid, index: c_int) void;
pub extern fn Rlist_view_clear_items(list_view: Rid) void;
pub extern fn Rlist_view_get_item_count(list_view: Rid) c_int;
pub extern fn Rlist_view_set_virtual_items(list_view: Rid, count: c_int, callback: RListItemTextCallback, user_data: ?*anyopaque) void;
/// Selects a row. -1 selects nothing.
pub extern fn Rlist_view_set_selected(list_view: Rid, index: c_int) void;
pub extern fn Rlist_view_get_selected(list_view: Rid) c_int;
pub extern fn Rlist_view_scroll_to_item(list_view: Rid, index: c_int) void;
pub extern fn Rlist_view_set_selection_changed_callback(list_view: Rid, callback: RListEventCallback, user_data: ?*anyopaque) void;
/// Called on double click or Enter. This also fires the action of the widget.
pub extern fn Rlist_view_set_activated_callback(list_view: Rid, callback: RListEventCallback, user_data: ?*anyopaque) void;
/// Adds an item.
/// parent_item Item to add under, or R_INVALID_ID for a top-level item.
/// Returns: The handle of the new item.
pub extern fn Rtree_view_add_item(tree_view: Rid, parent_item: Rid, text: ?[*:0]const u8) Rid;
/// Deletes an item and its descendants.
pub extern fn Rtree_item_destroy(item: Rid) void;
pub extern fn Rtree_view_clear_items(tree_view: Rid) void;
pub extern fn Rtree_item_is_valid(item: Rid) c_int;
pub extern fn Rtree_item_set_text(item: Rid, text: ?[*:0]const u8) void;
pub extern fn Rtree_item_get_text(item: Rid) ?[*:0]const u8;
pub extern fn Rtree_item_set_expanded(item: Rid, expanded: c_int) void;
pub extern fn Rtree_item_is_expanded(item: Rid) c_int;
pub extern fn Rtree_item_set_user_handle(item: Rid, handle: ?*anyopaque) void;
pub extern fn Rtree_item_get_user_handle(item: Rid) ?*anyopaque;
/// Top-level items of a tree.
pub extern fn Rtree_view_get_item_count(tree_view: Rid) c_int;
pub extern fn Rtree_view_get_item(tree_view: Rid, index: c_int) Rid;
/// Returns: The parent item, or R_INVALID_ID for a top-level item.
pub extern fn Rtree_item_get_parent(item: Rid) Rid;
pub extern fn Rtree_item_get_child_count(item: Rid) c_int;
pub extern fn Rtree_item_get_child(item: Rid, index: c_int) Rid;
/// Selects an item. R_INVALID_ID selects nothing.
pub extern fn Rtree_view_set_selected_item(tree_view: Rid, item: Rid) void;
pub extern fn Rtree_view_get_selected_item(tree_view: Rid) Rid;
pub extern fn Rtree_view_set_selection_changed_callback(tree_view: Rid, callback: RTreeEventCallback, user_data: ?*anyopaque) void;
/// Called on double click or Enter. This also fires the action of the widget.
pub extern fn Rtree_view_set_activated_callback(tree_view: Rid, callback: RTreeEventCallback, user_data: ?*anyopaque) void;
/// Creates a menu inside a menu bar or as a sub-menu of another menu.
pub extern fn Rmenu_add_menu(menu_bar_or_menu: Rid, title: ?[*:0]const u8) Rid;
pub extern fn Rmenu_set_title(menu: Rid, title: ?[*:0]const u8) void;
pub extern fn Rmenu_add_item(menu: Rid, text: ?[*:0]const u8) Rid;
pub extern fn Rmenu_add_separator(menu: Rid) Rid;
pub extern fn Rmenu_item_set_text(item: Rid, text: ?[*:0]const u8) void;
/// Shows a shortcut next to the item and triggers the item when the
/// keys are pressed in the window of its menu bar.
pub extern fn Rmenu_item_set_shortcut(item: Rid, key: RKey, modifiers: u32) void;
pub extern fn Rmenu_item_set_checkable(item: Rid, checkable: c_int) void;
pub extern fn Rmenu_item_set_checked(item: Rid, checked: c_int) void;
pub extern fn Rmenu_item_is_checked(item: Rid) c_int;
pub extern fn Rmenu_item_set_clicked_callback(item: Rid, callback: RMenuItemClickedCallback, user_data: ?*anyopaque) void;
/// Opens a menu as a popup at a position of the window (logical units).
pub extern fn Rmenu_open(menu: Rid, window: Rid, x: f32, y: f32) void;
/// Closes every open menu and popup of the window.
pub extern fn Rwindow_close_popups(window: Rid) void;
/// text Initial content, or NULL.
pub extern fn Rtext_editor_create(text: ?[*:0]const u8) Rid;
/// Replaces the whole text. The history of undo is cleared; the changed callback is not called.
pub extern fn Rtext_editor_set_text(editor: Rid, text: ?[*:0]const u8) void;
/// Returns: The whole text, lines separated by '\n'. Valid until the next call or the next edit.
pub extern fn Rtext_editor_get_text(editor: Rid) ?[*:0]const u8;
/// Returns: The number of bytes of Rtext_editor_get_text.
pub extern fn Rtext_editor_get_length(editor: Rid) c_int;
pub extern fn Rtext_editor_get_line_count(editor: Rid) c_int;
/// Returns: One line, without its line break. Valid until the next edit. NULL when `line` is out of range.
pub extern fn Rtext_editor_get_line(editor: Rid, line: c_int) ?[*:0]const u8;
/// Types a text at the cursor, replacing the selection, as the user
/// would: it can be undone and the changed callback is called.
pub extern fn Rtext_editor_insert_text(editor: Rid, text: ?[*:0]const u8) void;
pub extern fn Rtext_editor_set_cursor(editor: Rid, line: c_int, column: c_int) void;
pub extern fn Rtext_editor_get_cursor(editor: Rid, line: ?*c_int, column: ?*c_int) void;
/// Selects from one position to another; the cursor is on the second.
pub extern fn Rtext_editor_select(editor: Rid, from_line: c_int, from_column: c_int, to_line: c_int, to_column: c_int) void;
pub extern fn Rtext_editor_select_all(editor: Rid) void;
/// Start and end of the selection, in the order of the text. Returns R_FALSE when nothing is selected.
pub extern fn Rtext_editor_get_selection(editor: Rid, from_line: ?*c_int, from_column: ?*c_int, to_line: ?*c_int, to_column: ?*c_int) c_int;
/// Returns: The selected text, "" when there is none. Valid until the next call or the next edit.
pub extern fn Rtext_editor_get_selected_text(editor: Rid) ?[*:0]const u8;
/// Scrolls so that a line is visible.
pub extern fn Rtext_editor_scroll_to_line(editor: Rid, line: c_int) void;
pub extern fn Rtext_editor_undo(editor: Rid) void;
pub extern fn Rtext_editor_redo(editor: Rid) void;
pub extern fn Rtext_editor_can_undo(editor: Rid) c_int;
pub extern fn Rtext_editor_can_redo(editor: Rid) c_int;
/// A read-only editor can still be scrolled, selected and copied.
pub extern fn Rtext_editor_set_read_only(editor: Rid, read_only: c_int) void;
/// Long lines continue on the next row instead of scrolling sideways. Off by default.
pub extern fn Rtext_editor_set_word_wrap(editor: Rid, enabled: c_int) void;
/// Shows the line numbers in a margin. Off by default.
pub extern fn Rtext_editor_set_line_numbers(editor: Rid, enabled: c_int) void;
/// Width of a tab, in spaces (4 by default).
pub extern fn Rtext_editor_set_tab_size(editor: Rid, spaces: c_int) void;
/// R_TRUE makes the Tab key insert spaces instead of a tab character. Off by default.
pub extern fn Rtext_editor_set_tab_inserts_spaces(editor: Rid, enabled: c_int) void;
/// The same as Rtext_editor_set_language with "XML", "C" or NULL.
pub extern fn Rtext_editor_set_syntax(editor: Rid, syntax: RSyntax) void;
/// font R_INVALID_ID uses the font of the theme (or the fixed-width
/// font, with a language).
/// size 0 uses the font size of the theme.
pub extern fn Rtext_editor_set_font(editor: Rid, font: Rid, size: f32) void;
/// Text shown, dimmed, while the editor is empty.
pub extern fn Rtext_editor_set_placeholder(editor: Rid, text: ?[*:0]const u8) void;
/// Declares a language, or replaces the one that has this name: to
/// add words to a built-in language, read it with Rlanguage_get, change
/// what you want and register it again. The strings are copied.
pub extern fn Rlanguage_register(desc: ?*const RLanguageDesc) c_int;
/// Reads the description of a language. The strings belong to the library. Returns R_FALSE when it is unknown.
pub extern fn Rlanguage_get(name: ?[*:0]const u8, desc: ?*RLanguageDesc) c_int;
pub extern fn Rlanguage_get_count() c_int;
pub extern fn Rlanguage_get_name(index: c_int) ?[*:0]const u8;
/// Returns: The name of the language of a file, from its extension, or NULL.
pub extern fn Rlanguage_for_file(path: ?[*:0]const u8) ?[*:0]const u8;
/// Gives the editor a language, by name ("C++", "Python"...), by
/// another usual name ("cpp", "js") or by the extension of its files
/// ("rs"). NULL or "" removes it. An editor with a language uses the
/// fixed-width font unless it was given a font.
pub extern fn Rtext_editor_set_language(editor: Rid, name: ?[*:0]const u8) void;
/// Returns: The name of the language of the editor, or "".
pub extern fn Rtext_editor_get_language(editor: Rid) ?[*:0]const u8;
/// What the language of the editor makes of the character at a
/// position: the colors come from it. R_TOKEN_TEXT without a language.
pub extern fn Rtext_editor_get_token(editor: Rid, line: c_int, column: c_int) RToken;
/// R_FALSE stops the editor from proposing words by itself; Ctrl+Space still does. On by default.
pub extern fn Rtext_editor_set_completion(editor: Rid, enabled: c_int) void;
/// Proposes words for what is before the cursor, as Ctrl+Space does.
pub extern fn Rtext_editor_show_completions(editor: Rid) void;
pub extern fn Rtext_editor_is_completing(editor: Rid) c_int;
pub extern fn Rtext_editor_set_completion_callback(editor: Rid, callback: RCompletionCallback, user_data: ?*anyopaque) void;
/// Adds a proposal. Only from a completion callback. Proposals that
/// do not start with the prefix are left out; the others keep the order
/// they were added in.
/// detail A few words shown beside it ("function", "int"), or NULL.
pub extern fn Rtext_editor_add_completion(editor: Rid, text: ?[*:0]const u8, detail: ?[*:0]const u8) void;
/// R_FALSE stops the editor from closing brackets and quotes and
/// from indenting after them. On by default; it only acts with a language.
pub extern fn Rtext_editor_set_auto_pairs(editor: Rid, enabled: c_int) void;
/// Underlines a part of a line and attaches a message to it, shown
/// when the pointer rests there: an error of a compiler, a warning.
/// Markers follow their line when text is added or removed above it, and
/// go away when their own line is edited.
/// to_column -1 for the end of the line.
pub extern fn Rtext_editor_add_marker(editor: Rid, line: c_int, from_column: c_int, to_column: c_int, kind: RMarkerKind, message: ?[*:0]const u8) void;
pub extern fn Rtext_editor_clear_markers(editor: Rid) void;
pub extern fn Rtext_editor_get_marker_count(editor: Rid) c_int;
pub extern fn Rtext_editor_set_changed_callback(editor: Rid, callback: RTextEditorChangedCallback, user_data: ?*anyopaque) void;
/// title Text shown in the bar; NULL shows the title of the window.
pub extern fn Rheader_bar_create(title: ?[*:0]const u8) Rid;
/// title NULL shows the title of the window.
pub extern fn Rheader_bar_set_title(bar: Rid, title: ?[*:0]const u8) void;
/// buttons Bitmask of RHeaderButtons. All by default; the maximize button only shows on a resizable window.
pub extern fn Rheader_bar_set_buttons(bar: Rid, buttons: u32) void;
/// side R_HALIGN_RIGHT (default): minimize, maximize, close, as on
/// Windows. R_HALIGN_LEFT: three round colored buttons, as on macOS.
pub extern fn Rheader_bar_set_button_side(bar: Rid, side: RHAlign) void;
/// align R_HALIGN_CENTER (default) or R_HALIGN_LEFT.
pub extern fn Rheader_bar_set_title_align(bar: Rid, align_: RHAlign) void;
/// R_FALSE removes the background of the bar: for a header that
/// floats above the content (in an Overlay) or sits inside a glass panel.
pub extern fn Rheader_bar_set_background(bar: Rid, enabled: c_int) void;
pub extern fn Rdock_area_create() Rid;
/// Adds a panel. The dock area takes ownership of the widget.
/// title    Text of its tab.
/// side     Where it goes.
/// relative A panel of the area to split or to share tabs with, or
///                 R_INVALID_ID for the whole area (with R_DOCK_TAB: the
///                 first group of tabs).
/// size     Part of the split space given to the new panel, 0..1
///                 (0 = a half, or a quarter along the whole area).
pub extern fn Rdock_area_add_panel(area: Rid, panel: Rid, title: ?[*:0]const u8, side: RDockSide, relative: Rid, size: f32) void;
/// Moves a panel of the area somewhere else in it.
pub extern fn Rdock_area_move_panel(area: Rid, panel: Rid, side: RDockSide, relative: Rid, size: f32) void;
/// Removes a panel and deletes it.
pub extern fn Rdock_area_close_panel(area: Rid, panel: Rid) void;
pub extern fn Rdock_area_set_panel_title(area: Rid, panel: Rid, title: ?[*:0]const u8) void;
/// R_FALSE removes the close button of the tab of a panel. Panels can be closed by default.
pub extern fn Rdock_area_set_panel_closable(area: Rid, panel: Rid, closable: c_int) void;
/// Brings the tab of a panel to the front.
pub extern fn Rdock_area_show_panel(area: Rid, panel: Rid) void;
/// Returns: R_TRUE when the panel is the one shown in its group of tabs.
pub extern fn Rdock_area_is_panel_shown(area: Rid, panel: Rid) c_int;
/// R_FALSE stops the user from moving the panels (the borders can still be dragged).
pub extern fn Rdock_area_set_movable(area: Rid, movable: c_int) void;
/// Describes how the panels are arranged, as a short text to keep
/// in a settings file. Panels are named by their widget name
/// (Rwidget_set_name): unnamed panels are not part of the description.
/// Returns: A string valid until the next call.
pub extern fn Rdock_area_save_layout(area: Rid) ?[*:0]const u8;
/// Arranges the panels as a saved description says. Panels of the
/// area that the description does not name keep a place; names it has
/// that match no panel are ignored.
/// Returns: R_FALSE when the text is not a layout (nothing is changed).
pub extern fn Rdock_area_load_layout(area: Rid, layout: ?[*:0]const u8) c_int;
pub extern fn Rdock_area_set_close_callback(area: Rid, callback: RDockCloseCallback, user_data: ?*anyopaque) void;
pub extern fn Rdock_area_set_changed_callback(area: Rid, callback: RDockChangedCallback, user_data: ?*anyopaque) void;
pub extern fn Risland_create() Rid;
/// Unfolds or folds the island. The changed callback is not called.
pub extern fn Risland_set_expanded(island: Rid, expanded: c_int) void;
pub extern fn Risland_is_expanded(island: Rid) c_int;
/// Unfolds the island, and folds it back by itself after a while:
/// to tell something to the user. It stays open while the pointer is on it.
pub extern fn Risland_expand_for(island: Rid, seconds: f64) void;
/// R_TRUE (default): the island unfolds under the pointer and folds back when it leaves.
pub extern fn Risland_set_expand_on_hover(island: Rid, enabled: c_int) void;
/// R_TRUE (default): a click on the island unfolds or folds it.
pub extern fn Risland_set_expand_on_click(island: Rid, enabled: c_int) void;
/// R_TRUE gives the island square upper corners: a notch that hangs
/// from the edge it touches, instead of a capsule that floats under it.
pub extern fn Risland_set_attached(island: Rid, attached: c_int) void;
pub extern fn Risland_set_changed_callback(island: Rid, callback: RIslandChangedCallback, user_data: ?*anyopaque) void;
/// Shows a widget (usually an island) floating at the top of a
/// window, in the middle, above its content. The window takes ownership;
/// the previous one is deleted. R_INVALID_ID removes it. The padding of
/// its slot (Rslot_set_padding) is its distance from the top edge.
pub extern fn Rwindow_set_island(window: Rid, widget: Rid) void;
pub extern fn Rwindow_get_island(window: Rid) Rid;
/// Shows a widget as a dialog of a window. The window takes
/// ownership of `content`: it is deleted when the dialog closes, so read
/// what you need from it before, or in `closed`.
/// The Escape key dismisses the dialog (result 0).
pub extern fn Rdialog_open(window: Rid, content: Rid, closed: RDialogClosedCallback, user_data: ?*anyopaque) void;
/// Closes the dialog that contains a widget (the content itself or
/// anything inside it: the button that was clicked).
/// result Passed to the closed callback and returned by Rdialog_run.
pub extern fn Rdialog_close(widget: Rid, result: c_int) void;
/// Shows a dialog and waits until it closes. Returns its result.
pub extern fn Rdialog_run(window: Rid, content: Rid) c_int;
/// Returns: R_TRUE while a dialog is shown in the window.
pub extern fn Rwindow_has_dialog(window: Rid) c_int;
/// Shows a message with buttons and waits for the answer.
/// window R_INVALID_ID: the first window of the application.
/// title  May be NULL.
pub extern fn Rmessage_box(window: Rid, title: ?[*:0]const u8, text: ?[*:0]const u8, buttons: RMessageButtons) RMessageResult;
/// Asks for a file to open.
/// desc May be NULL.
/// Returns: The path (UTF-8), valid until the next file dialog, or NULL when the user cancelled.
pub extern fn Rfile_dialog_open(desc: ?*const RFileDialogDesc) ?[*:0]const u8;
/// Asks where to save a file. An existing file was confirmed by the user.
pub extern fn Rfile_dialog_save(desc: ?*const RFileDialogDesc) ?[*:0]const u8;
/// Asks for a folder.
pub extern fn Rfile_dialog_select_folder(desc: ?*const RFileDialogDesc) ?[*:0]const u8;
/// What the last file dialog returned: several paths with R_FILE_DIALOG_MULTIPLE.
pub extern fn Rfile_dialog_get_result_count() c_int;
pub extern fn Rfile_dialog_get_result(index: c_int) ?[*:0]const u8;
/// Whole files, with UTF-8 paths on every platform.
/// Returns: Rfile_read: the content followed by a '\0', to release with
/// Rmemory_free; NULL when the file cannot be read. `size` may be NULL.
pub extern fn Rfile_read(path: ?[*:0]const u8, size: ?*usize) ?[*:0]u8;
pub extern fn Rfile_write(path: ?[*:0]const u8, data: ?*const anyopaque, size: usize) c_int;
pub extern fn Rfile_browser_create() Rid;
pub extern fn Rfile_browser_set_mode(browser: Rid, mode: RFileBrowserMode) void;
/// Shows a folder. "~" is the home of the user. Returns R_FALSE when it cannot be read.
pub extern fn Rfile_browser_set_directory(browser: Rid, path: ?[*:0]const u8) c_int;
pub extern fn Rfile_browser_get_directory(browser: Rid) ?[*:0]const u8;
/// Reads the folder again.
pub extern fn Rfile_browser_refresh(browser: Rid) void;
/// Kinds of files the user chooses from; the first one is active. The strings are copied.
pub extern fn Rfile_browser_set_filters(browser: Rid, filters: ?*const RFileFilter, count: c_int) void;
pub extern fn Rfile_browser_set_show_hidden(browser: Rid, show: c_int) void;
/// R_TRUE lets several files be selected (Ctrl and Shift with a click).
pub extern fn Rfile_browser_set_multiple(browser: Rid, multiple: c_int) void;
/// R_FALSE hides the name field, the filter list and the buttons:
/// what remains is an explorer to put in a side panel. Shown by default.
pub extern fn Rfile_browser_set_controls_visible(browser: Rid, visible: c_int) void;
/// R_FALSE hides the list of places on the left.
pub extern fn Rfile_browser_set_places_visible(browser: Rid, visible: c_int) void;
/// Name typed in the name field (save mode).
pub extern fn Rfile_browser_set_file_name(browser: Rid, name: ?[*:0]const u8) void;
/// Added to a name typed without extension, in save mode.
pub extern fn Rfile_browser_set_default_extension(browser: Rid, extension: ?[*:0]const u8) void;
/// Paths of the selected entries. The strings are valid until the selection or the folder changes.
pub extern fn Rfile_browser_get_selection_count(browser: Rid) c_int;
pub extern fn Rfile_browser_get_selection(browser: Rid, index: c_int) ?[*:0]const u8;
/// Selects an entry of the folder by its name.
pub extern fn Rfile_browser_select(browser: Rid, name: ?[*:0]const u8) void;
pub extern fn Rfile_browser_set_accepted_callback(browser: Rid, callback: RFileBrowserCallback, user_data: ?*anyopaque) void;
/// Called by the Cancel button.
pub extern fn Rfile_browser_set_cancelled_callback(browser: Rid, callback: RFileBrowserCallback, user_data: ?*anyopaque) void;
/// Called when the selection changes; `path` is the entry under the cursor, or NULL.
pub extern fn Rfile_browser_set_selection_changed_callback(browser: Rid, callback: RFileBrowserCallback, user_data: ?*anyopaque) void;
/// Shows a texture. R_INVALID_ID shows nothing.
pub extern fn Rviewport_set_texture(viewport: Rid, texture: Rid) void;
/// How the texture is fitted in the widget. Default: R_STRETCH_FILL.
pub extern fn Rviewport_set_stretch(viewport: Rid, stretch: RStretch) void;
/// R_TRUE flips the texture vertically (OpenGL framebuffers are bottom-up).
pub extern fn Rviewport_set_flip_y(viewport: Rid, flip: c_int) void;
pub extern fn Rviewport_set_draw_callback(viewport: Rid, callback: RViewportDrawCallback, user_data: ?*anyopaque) void;
/// Creates (or destroys) the native child window of the viewport.
/// The viewport must already be in a window.
/// Returns: R_TRUE on success.
pub extern fn Rviewport_set_native(viewport: Rid, enabled: c_int) c_int;
/// Fills `native` with the handles of the child window.
pub extern fn Rviewport_get_native_handle(viewport: Rid, native: ?*RNativeWindow) c_int;
/// Size of the viewport in pixels after the last layout.
pub extern fn Rviewport_get_pixel_size(viewport: Rid, width: ?*c_int, height: ?*c_int) void;
pub extern fn Rviewport_set_resize_callback(viewport: Rid, callback: RViewportResizeCallback, user_data: ?*anyopaque) void;
/// R_TRUE redraws the window every frame while the viewport is
/// visible (game view). R_FALSE redraws only on Rwidget_invalidate (default).
pub extern fn Rviewport_set_realtime(viewport: Rid, realtime: c_int) void;
pub extern fn Rcustom_set_paint_callback(widget: Rid, callback: RPaintCallback, user_data: ?*anyopaque) void;
/// Reports the desired size of the widget. Without it the widget asks for 0 x 0.
pub extern fn Rcustom_set_measure_callback(widget: Rid, callback: RMeasureCallback, user_data: ?*anyopaque) void;
/// Filled rectangle with rounded corners (radius 0 = sharp).
pub extern fn Rdraw_rect(painter: ?*RPainter, x: f32, y: f32, width: f32, height: f32, radius: f32, r: f32, g: f32, b: f32, a: f32) void;
/// Outline of a rounded rectangle, drawn inside its bounds.
pub extern fn Rdraw_rect_outline(painter: ?*RPainter, x: f32, y: f32, width: f32, height: f32, radius: f32, thickness: f32, r: f32, g: f32, b: f32, a: f32) void;
pub extern fn Rdraw_circle(painter: ?*RPainter, center_x: f32, center_y: f32, radius: f32, r: f32, g: f32, b: f32, a: f32) void;
/// Anti-aliased line with round caps.
pub extern fn Rdraw_line(painter: ?*RPainter, x0: f32, y0: f32, x1: f32, y1: f32, thickness: f32, r: f32, g: f32, b: f32, a: f32) void;
pub extern fn Rdraw_triangle(painter: ?*RPainter, x0: f32, y0: f32, x1: f32, y1: f32, x2: f32, y2: f32, r: f32, g: f32, b: f32, a: f32) void;
/// Texture stretched to a rectangle and multiplied by a tint color.
pub extern fn Rdraw_image(painter: ?*RPainter, texture: Rid, x: f32, y: f32, width: f32, height: f32, r: f32, g: f32, b: f32, a: f32) void;
/// Draws UTF-8 text; (x, y) is the top-left corner of the first line.
/// font R_INVALID_ID uses the theme font.
/// size 0 uses R_METRIC_FONT_SIZE.
pub extern fn Rdraw_text(painter: ?*RPainter, font: Rid, size: f32, x: f32, y: f32, text: ?[*:0]const u8, r: f32, g: f32, b: f32, a: f32) void;
/// Rounded rectangle of glass. It shows what was drawn before it,
/// frosted and bent along its rim, under a tint: (r, g, b) is the color of
/// the tint and `a` its strength (0 = clear glass, 1 = opaque color).
/// Works with any theme. Needs a backend that implements glass (the Vulkan
/// backend does).
pub extern fn Rdraw_glass(painter: ?*RPainter, x: f32, y: f32, width: f32, height: f32, radius: f32, r: f32, g: f32, b: f32, a: f32) void;
/// Animates a value from the paint callback of a custom widget.
/// Moves the spring toward `target` by the time elapsed since the last
/// frame and returns its value. While it moves, the window keeps drawing
/// frames by itself. The first call puts the spring on its target.
///
///     static RSpring size;                                  (zeroed)
///     float s = Rdraw_spring(painter, &size, hovered ? 1.0f : 0.0f, R_MOTION_BOUNCY);
///     Rdraw_circle(painter, cx, cy, 20.0f + 8.0f * s, ...);
pub extern fn Rdraw_spring(painter: ?*RPainter, spring: ?*RSpring, target: f32, motion: RMotion) f32;
/// Pushes a spring away from its target (units per second): it bounces back.
pub extern fn Rspring_kick(spring: ?*RSpring, velocity: f32) void;
/// Puts a spring at rest on a value, without animation.
pub extern fn Rspring_set(spring: ?*RSpring, value: f32) void;
/// Restricts the following draws to a rectangle, intersected with the current one.
pub extern fn Rdraw_push_clip_rect(painter: ?*RPainter, x: f32, y: f32, width: f32, height: f32) void;
pub extern fn Rdraw_pop_clip_rect(painter: ?*RPainter) void;
/// Creates a window from a document whose root element is <Window>.
/// xml  UTF-8 text. It does not need to be null-terminated.
/// size Size of the text in bytes.
/// Returns: The window, or R_INVALID_ID. Parse errors are reported with
///         their line and column through the error callback (R_ERROR_XML).
pub extern fn Rwindow_create_from_xml(xml: ?[*:0]const u8, size: usize) Rid;
pub extern fn Rwindow_create_from_xml_file(path: ?[*:0]const u8) Rid;
/// Creates a widget tree from a document whose root element is a
/// widget. The caller owns the returned widget until it is added to a parent.
pub extern fn Rwidget_create_from_xml(xml: ?[*:0]const u8, size: usize) Rid;
pub extern fn Rwidget_create_from_xml_file(path: ?[*:0]const u8) Rid;
/// Folder used to resolve relative paths found in XML (Source="...",
/// Font="..."). The *FromXmlFile functions use the folder of the file.
pub extern fn Rxml_set_base_path(path: ?[*:0]const u8) void;
/// Reads the <Widget Name="..."> declarations of a document (its
/// root, or the children of a <Widgets> root) so that the tags they
/// declare can be used in the documents loaded afterwards.
/// Returns: The number of tags declared.
pub extern fn Rxml_register_widgets(xml: ?[*:0]const u8, size: usize) c_int;
pub extern fn Rxml_register_widgets_from_file(path: ?[*:0]const u8) c_int;
pub extern fn Rxml_unregister_widget(name: ?[*:0]const u8) void;
pub extern fn Rxml_is_widget_registered(name: ?[*:0]const u8) c_int;
/// The declared tags, for tools that list them.
pub extern fn Rxml_get_registered_widget_count() c_int;
pub extern fn Rxml_get_registered_widget_name(index: c_int) ?[*:0]const u8;
/// Sets a property from text, with the names and value syntax of the
/// XML attributes. Handy for scripting and data-driven tools.
/// Returns: R_TRUE when the attribute exists for this widget and the value is valid.
pub extern fn Rwidget_set_attribute(widget: Rid, name: ?[*:0]const u8, value: ?[*:0]const u8) c_int;
/// A new document with one element.
pub extern fn Rxml_document_create(root_tag: ?[*:0]const u8) Rid;
/// Reads a text into a document. Comments are kept.
/// error When not NULL, a mistake in the text is described there
/// and nothing is reported; when NULL it goes to the error callback.
/// Returns: The document, or R_INVALID_ID.
pub extern fn Rxml_document_parse(xml: ?[*:0]const u8, size: usize, error_: ?*RXmlError) Rid;
/// Reads a file. The paths written in it are relative to its folder.
pub extern fn Rxml_document_load(path: ?[*:0]const u8) Rid;
/// Releases a document and its nodes. What was built from it lives on.
pub extern fn Rxml_document_destroy(document: Rid) void;
pub extern fn Rxml_document_is_valid(document: Rid) c_int;
pub extern fn Rxml_document_get_root(document: Rid) Rid;
/// Writes the document as text, one element per line, indented
/// with tabs. Rxml_node_get_line then says where each node was written.
/// Returns: The text, valid until the next call for this document or its destruction.
pub extern fn Rxml_document_to_string(document: Rid) ?[*:0]const u8;
pub extern fn Rxml_document_save(document: Rid, path: ?[*:0]const u8) c_int;
/// Builds the widgets the document describes, as
/// Rwidget_create_from_xml does. When the root is a <Window>, builds what
/// the window would show (R_INVALID_ID when it is empty).
pub extern fn Rxml_document_create_widget(document: Rid) Rid;
/// Builds the window the document describes, as Rwindow_create_from_xml does.
pub extern fn Rxml_document_create_window(document: Rid) Rid;
pub extern fn Rxml_node_is_valid(node: Rid) c_int;
pub extern fn Rxml_node_get_document(node: Rid) Rid;
/// Returns: The tag of an element, "" for a comment.
pub extern fn Rxml_node_get_tag(node: Rid) ?[*:0]const u8;
pub extern fn Rxml_node_set_tag(node: Rid, tag: ?[*:0]const u8) void;
pub extern fn Rxml_node_is_comment(node: Rid) c_int;
/// The text inside an element (<Label>this</Label>), or the text of a comment.
pub extern fn Rxml_node_get_text(node: Rid) ?[*:0]const u8;
pub extern fn Rxml_node_set_text(node: Rid, text: ?[*:0]const u8) void;
pub extern fn Rxml_node_get_parent(node: Rid) Rid;
pub extern fn Rxml_node_get_child_count(node: Rid) c_int;
pub extern fn Rxml_node_get_child(node: Rid, index: c_int) Rid;
/// Returns: The place of a node among the children of its parent; 0 for the root.
pub extern fn Rxml_node_get_index(node: Rid) c_int;
/// index Where among the children; -1 for the end.
pub extern fn Rxml_node_add_child(parent: Rid, tag: ?[*:0]const u8, index: c_int) Rid;
pub extern fn Rxml_node_add_comment(parent: Rid, text: ?[*:0]const u8, index: c_int) Rid;
/// Reads a piece of XML (one element and what it contains) and
/// adds it: what a palette, a paste or a template inserts.
pub extern fn Rxml_node_add_xml(parent: Rid, xml: ?[*:0]const u8, size: usize, index: c_int) Rid;
/// Removes a node and what it contains. The root stays.
pub extern fn Rxml_node_remove(node: Rid) void;
/// Moves a node, with what it contains, under another parent of
/// the same document or elsewhere under its own.
/// index Where among the children of the new parent, counted once the node has left its place; -1 for the end.
/// Returns: R_FALSE when it cannot be done (the root, or a parent that is inside the node).
pub extern fn Rxml_node_move(node: Rid, new_parent: Rid, index: c_int) c_int;
/// Copies a node and what it contains, right after it. Returns the copy.
pub extern fn Rxml_node_duplicate(node: Rid) Rid;
/// The node as text. Valid until the next call for this document.
pub extern fn Rxml_node_to_string(node: Rid) ?[*:0]const u8;
pub extern fn Rxml_node_get_attribute_count(node: Rid) c_int;
pub extern fn Rxml_node_get_attribute_name(node: Rid, index: c_int) ?[*:0]const u8;
pub extern fn Rxml_node_get_attribute_value(node: Rid, index: c_int) ?[*:0]const u8;
/// Returns: The value of an attribute (names are compared without case), or NULL when the node does not have it.
pub extern fn Rxml_node_get_attribute(node: Rid, name: ?[*:0]const u8) ?[*:0]const u8;
/// Sets an attribute, added after the others when it is new. A NULL value removes it.
pub extern fn Rxml_node_set_attribute(node: Rid, name: ?[*:0]const u8, value: ?[*:0]const u8) void;
/// Returns: The line of the node, starting at 1, in the text the document
/// was read from or last written to; 0 for a node added since.
pub extern fn Rxml_node_get_line(node: Rid) c_int;
/// Returns: The widget (the window, for a <Window>) the node became the
/// last time the document was built, or R_INVALID_ID: never built,
/// destroyed since, or not a widget (an <Item>, a comment). A tag declared
/// with <Widget> gives the widget it stands for.
pub extern fn Rxml_node_get_widget(node: Rid) Rid;
/// Returns: The node a widget was built from, or R_INVALID_ID: not built
/// from a document, or the document is gone. The widgets inside a tag
/// declared with <Widget> all come from the node where the tag is used.
pub extern fn Rwidget_get_xml_node(widget: Rid) Rid;
/// The tags of the library, <Window> included. Those declared with <Widget> are listed by Rxml_get_registered_widget_name.
pub extern fn Rxml_get_tag_count() c_int;
pub extern fn Rxml_get_tag_name(index: c_int) ?[*:0]const u8;
/// Returns: One line about a tag, "" when there is nothing to say.
pub extern fn Rxml_get_tag_description(tag: ?[*:0]const u8) ?[*:0]const u8;
/// Returns: 0: no child widget; 1: a single one; 2: any number. Declared and unknown tags give 2.
pub extern fn Rxml_get_tag_children(tag: ?[*:0]const u8) c_int;
/// The attributes of a tag: its own first, then those every widget
/// has, then those of its place in its parent. For a tag declared with
/// <Widget>, its own are its parameters.
pub extern fn Rxml_get_attribute_count(tag: ?[*:0]const u8) c_int;
pub extern fn Rxml_get_attribute_info(tag: ?[*:0]const u8, index: c_int, info: ?*RXmlAttributeInfo) c_int;
