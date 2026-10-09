/**
 * Raven - xml/xml_loader.c
 * Builds windows and widget trees from XML (see raven_xml.h and docs/XML.md).
 *
 *   element          -> widget of the class named by the tag
 *   attribute        -> rvn_widget_set_attribute
 *   child element    -> child widget, unless the class of the parent handles
 *                       the tag itself (<Item>, menu <Separator>)
 *   text content     -> the "Text" attribute
 */

#include "xml.h"
#include "../widgets/common.h"
#include "../render/renderer.h"

/* ---------------------------------------------------------------------------
 *  Resources named in attributes (Source="...", Font="...")
 * ------------------------------------------------------------------------- */

typedef struct rvn_xml_resource_t {
	char* path;
	Rid id;
} rvn_xml_resource_t;

static struct {
	rvn_xml_resource_t* textures;
	uint32_t texture_count;
	uint32_t texture_cap;
	rvn_xml_resource_t* fonts;
	uint32_t font_count;
	uint32_t font_cap;
	/** Folder of the file being loaded; overrides the base path meanwhile. */
	char* file_dir;
} g_xml;

static char* rvn_xml_resolve(const char* path) {
	return rvn_path_join(g_xml.file_dir ? g_xml.file_dir : g_rvn.xml_base_path, path);
}

Rid rvn_xml_texture(const char* path) {
	char* full = rvn_xml_resolve(path);
	Rid texture;
	uint32_t i;

	for (i = 0; i < g_xml.texture_count; i++) {
		if (strcmp(g_xml.textures[i].path, full) != 0) continue;
		/* still valid unless the application deleted it */
		if (rvn_texture_get(g_xml.textures[i].id)) {
			rvn_free(full);
			return g_xml.textures[i].id;
		}
		break;
	}
	texture = Rtexture_create_from_file(full);
	if (texture == R_INVALID_ID) {
		rvn_free(full);
		return R_INVALID_ID;
	}
	if (i < g_xml.texture_count) {
		g_xml.textures[i].id = texture;
		rvn_free(full);
	} else {
		RVN_ARRAY_RESERVE(g_xml.textures, g_xml.texture_cap, g_xml.texture_count + 1);
		g_xml.textures[g_xml.texture_count].path = full;
		g_xml.textures[g_xml.texture_count].id = texture;
		g_xml.texture_count++;
	}
	return texture;
}

Rid rvn_xml_font(const char* path) {
	char* full = rvn_xml_resolve(path);
	Rid font;
	uint32_t i;

	for (i = 0; i < g_xml.font_count; i++) {
		if (strcmp(g_xml.fonts[i].path, full) != 0) continue;
		if (Rfont_is_valid(g_xml.fonts[i].id)) {
			rvn_free(full);
			return g_xml.fonts[i].id;
		}
		break;
	}
	font = Rfont_create_from_file(full);
	if (font == R_INVALID_ID) {
		rvn_free(full);
		return R_INVALID_ID;
	}
	if (i < g_xml.font_count) {
		g_xml.fonts[i].id = font;
		rvn_free(full);
	} else {
		RVN_ARRAY_RESERVE(g_xml.fonts, g_xml.font_cap, g_xml.font_count + 1);
		g_xml.fonts[g_xml.font_count].path = full;
		g_xml.fonts[g_xml.font_count].id = font;
		g_xml.font_count++;
	}
	return font;
}

/** Forgets the resource cache. The textures and fonts themselves belong to the library pools. */
void rvn_xml_shutdown(void) {
	uint32_t i;
	for (i = 0; i < g_xml.texture_count; i++) rvn_free(g_xml.textures[i].path);
	for (i = 0; i < g_xml.font_count; i++) rvn_free(g_xml.fonts[i].path);
	rvn_free(g_xml.textures);
	rvn_free(g_xml.fonts);
	rvn_free(g_xml.file_dir);
	memset(&g_xml, 0, sizeof(g_xml));
}

/* ---------------------------------------------------------------------------
 *  Building
 * ------------------------------------------------------------------------- */

static void rvn_xml_error(const rvn_xml_node_t* node, RSeverity severity, const char* format, ...) {
	char message[256];
	va_list args;
	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);
	rvn_error(R_ERROR_XML, severity, "XML line %d, column %d: %s", node->line, node->column, message);
}

static rvn_widget_t* rvn_xml_build(const rvn_xml_node_t* node) {
	const rvn_widget_class_t* cls = rvn_widget_class_by_name(node->name);
	const rvn_xml_attribute_t* attribute;
	const rvn_xml_node_t* child;
	rvn_widget_t* widget;

	if (!cls) {
		rvn_xml_error(node, R_SEVERITY_ERROR, "unknown widget <%.60s>", node->name);
		return NULL;
	}
	widget = rvn_widget_create(cls->type);
	if (!widget) return NULL;

	/* children first: attributes such as Selected="1" or ActiveTab="2" refer to them */
	for (child = node->first_child; child; child = child->next) {
		rvn_widget_t* child_widget;
		/* the class may give the tag its own meaning (<Item>, <Separator> in a menu) */
		if (cls->xml_child && cls->xml_child(widget, child)) continue;
		if (cls->flags & RVN_CF_NO_CHILDREN) {
			rvn_xml_error(child, R_SEVERITY_ERROR, "<%.60s> cannot contain <%.60s>", node->name, child->name);
			rvn_widget_destroy(widget);
			return NULL;
		}
		child_widget = rvn_xml_build(child);
		if (!child_widget) {
			rvn_widget_destroy(widget);
			return NULL;
		}
		rvn_widget_add_child(widget, child_widget, -1);
	}

	/* <Label>Hello</Label> is the same as <Label Text="Hello"/> */
	if (node->text[0] && !rvn_xml_attribute(node, "Text") && !node->first_child) {
		rvn_widget_set_attribute(widget, "Text", node->text);
	}
	for (attribute = node->attributes; attribute; attribute = attribute->next) {
		if (!rvn_widget_set_attribute(widget, attribute->name, attribute->value)) {
			/* a typo should not prevent the rest of the interface from loading */
			rvn_xml_error(node, R_SEVERITY_WARNING, "<%.60s>: unknown attribute or invalid value %.60s=\"%.60s\"",
				node->name, attribute->name, attribute->value);
		}
	}
	return widget;
}

static int rvn_xml_load(rvn_xml_document_t* document, const char* xml, size_t size, const char* function) {
	if (!xml) {
		rvn_error(R_ERROR_INVALID_VALUE, R_SEVERITY_ERROR, "%s: NULL text", function);
		return R_FALSE;
	}
	if (!rvn_xml_parse(document, xml, size)) {
		rvn_error(R_ERROR_XML, R_SEVERITY_ERROR, "%s: line %d, column %d: %s", function,
			document->error_line, document->error_column, document->error);
		rvn_xml_free(document);
		return R_FALSE;
	}
	return R_TRUE;
}

static Rid rvn_xml_create_widget(const char* xml, size_t size, const char* function) {
	rvn_xml_document_t document;
	rvn_widget_t* widget;
	if (!rvn_xml_load(&document, xml, size, function)) return R_INVALID_ID;
	if (rvn_stricmp(document.root->name, "Window") == 0) {
		rvn_xml_error(document.root, R_SEVERITY_ERROR, "the root element is a <Window>: use Rwindow_create_from_xml");
		rvn_xml_free(&document);
		return R_INVALID_ID;
	}
	widget = rvn_xml_build(document.root);
	rvn_xml_free(&document);
	return widget ? widget->id : R_INVALID_ID;
}

static Rid rvn_xml_create_window(const char* xml, size_t size, const char* function) {
	rvn_xml_document_t document;
	const rvn_xml_node_t* root;
	const rvn_xml_node_t* child;
	const rvn_xml_attribute_t* attribute;
	rvn_widget_t* content = NULL;
	rvn_window_t* window_object;
	const char* title = "";
	int width = 800, height = 600, min_width = 0, min_height = 0, flag;
	Ruint flags = R_WINDOW_DEFAULT;
	rvn_color_t clear = 0;
	int has_clear = R_FALSE;
	Rid background = R_INVALID_ID;
	Rid window;

	if (!rvn_xml_load(&document, xml, size, function)) return R_INVALID_ID;
	root = document.root;
	if (rvn_stricmp(root->name, "Window") != 0) {
		rvn_xml_error(root, R_SEVERITY_ERROR, "the root element must be <Window>, found <%.60s>", root->name);
		rvn_xml_free(&document);
		return R_INVALID_ID;
	}

	for (attribute = root->attributes; attribute; attribute = attribute->next) {
		const char* name = attribute->name;
		const char* value = attribute->value;
		int ok = R_TRUE;
		if (RVN_ATTR("Title")) title = value;
		else if (RVN_ATTR("Width")) ok = rvn_parse_int(value, &width) && width > 0;
		else if (RVN_ATTR("Height")) ok = rvn_parse_int(value, &height) && height > 0;
		else if (RVN_ATTR("MinWidth")) ok = rvn_parse_int(value, &min_width);
		else if (RVN_ATTR("MinHeight")) ok = rvn_parse_int(value, &min_height);
		else if (RVN_ATTR("Resizable")) {
			ok = rvn_parse_bool(value, &flag);
			if (ok && !flag) flags |= R_WINDOW_NO_RESIZE;
		} else if (RVN_ATTR("Borderless")) {
			ok = rvn_parse_bool(value, &flag);
			if (ok && flag) flags |= R_WINDOW_BORDERLESS;
		} else if (RVN_ATTR("Maximized")) {
			ok = rvn_parse_bool(value, &flag);
			if (ok && flag) flags |= R_WINDOW_MAXIMIZED;
		} else if (RVN_ATTR("Hidden")) {
			ok = rvn_parse_bool(value, &flag);
			if (ok && flag) flags |= R_WINDOW_HIDDEN;
		} else if (RVN_ATTR("ClearColor")) {
			ok = rvn_parse_color(value, &clear);
			has_clear = ok;
		} else if (RVN_ATTR("Background")) {
			background = rvn_xml_texture(value);
			ok = background != R_INVALID_ID;
		} else {
			ok = R_FALSE;
		}
		if (!ok) {
			rvn_xml_error(root, R_SEVERITY_WARNING, "<Window>: unknown attribute or invalid value %.60s=\"%.60s\"", name, value);
		}
	}

	/* the content is built first: a mistake in it must not leave an empty window behind */
	for (child = root->first_child; child; child = child->next) {
		if (rvn_stricmp(child->name, "Shortcut") == 0) continue;
		if (content) {
			rvn_xml_error(child, R_SEVERITY_ERROR, "a <Window> has a single content element; wrap them in a panel");
			rvn_widget_destroy(content);
			rvn_xml_free(&document);
			return R_INVALID_ID;
		}
		content = rvn_xml_build(child);
		if (!content) {
			rvn_xml_free(&document);
			return R_INVALID_ID;
		}
	}

	window = Rwindow_create(title, width, height, flags);
	if (window == R_INVALID_ID) {
		if (content) rvn_widget_destroy(content);
		rvn_xml_free(&document);
		return R_INVALID_ID;
	}
	window_object = rvn_window_get(window);
	if (min_width > 0 || min_height > 0) Rwindow_set_min_size(window, min_width, min_height);
	if (has_clear) {
		float r, g, b, a;
		rvn_color_to_floats(clear, &r, &g, &b, &a);
		Rwindow_set_clear_color(window, r, g, b, a);
	}
	if (background != R_INVALID_ID) Rwindow_set_background(window, background);
	if (content) Rwindow_set_content(window, content->id);

	/* <Shortcut Keys="Ctrl+S" Action="file.save"/> */
	for (child = root->first_child; child; child = child->next) {
		const char* keys;
		const char* action;
		RKey key;
		uint32_t modifiers;
		if (rvn_stricmp(child->name, "Shortcut") != 0) continue;
		keys = rvn_xml_attribute(child, "Keys");
		action = rvn_xml_attribute(child, "Action");
		if (!keys || !action || !action[0] || !rvn_parse_shortcut(keys, &key, &modifiers)) {
			rvn_xml_error(child, R_SEVERITY_WARNING, "<Shortcut> needs Keys=\"Ctrl+S\" and Action=\"name\"");
			continue;
		}
		if (window_object) rvn_window_add_shortcut(window_object, key, modifiers, action);
	}

	rvn_xml_free(&document);
	return window;
}

/* ---------------------------------------------------------------------------
 *  Public API
 * ------------------------------------------------------------------------- */

Rid Rwidget_create_from_xml(const char* xml, size_t size) {
	RVN_API_GUARD(R_INVALID_ID);
	return rvn_xml_create_widget(xml, size, __func__);
}

Rid Rwindow_create_from_xml(const char* xml, size_t size) {
	RVN_API_GUARD(R_INVALID_ID);
	return rvn_xml_create_window(xml, size, __func__);
}

typedef Rid (*rvn_xml_create_fn)(const char* xml, size_t size, const char* function);

static Rid rvn_xml_create_from_file(const char* path, rvn_xml_create_fn create, const char* function) {
	size_t size = 0;
	char* text = rvn_read_file(path, &size);
	char* previous_dir = g_xml.file_dir;
	Rid result;
	if (!text) {
		rvn_error(R_ERROR_FILE_NOT_FOUND, R_SEVERITY_ERROR, "%s: cannot read '%s'", function, path ? path : "");
		return R_INVALID_ID;
	}
	/* relative paths inside the file are relative to the file */
	g_xml.file_dir = rvn_path_dir(path);
	result = create(text, size, function);
	rvn_free(g_xml.file_dir);
	g_xml.file_dir = previous_dir;
	rvn_free(text);
	return result;
}

Rid Rwidget_create_from_xml_file(const char* path) {
	RVN_API_GUARD(R_INVALID_ID);
	return rvn_xml_create_from_file(path, rvn_xml_create_widget, __func__);
}

Rid Rwindow_create_from_xml_file(const char* path) {
	RVN_API_GUARD(R_INVALID_ID);
	return rvn_xml_create_from_file(path, rvn_xml_create_window, __func__);
}

void Rxml_set_base_path(const char* path) {
	RVN_API_GUARD_VOID();
	rvn_str_set(&g_rvn.xml_base_path, (path && path[0]) ? path : NULL);
}
