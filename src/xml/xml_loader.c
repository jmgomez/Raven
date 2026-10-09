/**
 * HUI - xml/xml_loader.c
 * Builds windows and widget trees from XML (see hui_xml.h and docs/XML.md).
 *
 *   element          -> widget of the class named by the tag
 *   attribute        -> hui_widget_set_attribute
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

typedef struct hui_xml_resource_t {
	char* path;
	HUI_id id;
} hui_xml_resource_t;

static struct {
	hui_xml_resource_t* textures;
	uint32_t texture_count;
	uint32_t texture_cap;
	hui_xml_resource_t* fonts;
	uint32_t font_count;
	uint32_t font_cap;
	/** Folder of the file being loaded; overrides the base path meanwhile. */
	char* file_dir;
} g_xml;

static char* hui_xml_resolve(const char* path) {
	return hui_path_join(g_xml.file_dir ? g_xml.file_dir : g_hui.xml_base_path, path);
}

HUI_id hui_xml_texture(const char* path) {
	char* full = hui_xml_resolve(path);
	HUI_id texture;
	uint32_t i;

	for (i = 0; i < g_xml.texture_count; i++) {
		if (strcmp(g_xml.textures[i].path, full) != 0) continue;
		/* still valid unless the application deleted it */
		if (hui_texture_get(g_xml.textures[i].id)) {
			hui_free(full);
			return g_xml.textures[i].id;
		}
		break;
	}
	texture = HUI_CreateTextureFromFile(full);
	if (texture == HUI_INVALID_ID) {
		hui_free(full);
		return HUI_INVALID_ID;
	}
	if (i < g_xml.texture_count) {
		g_xml.textures[i].id = texture;
		hui_free(full);
	} else {
		HUI_ARRAY_RESERVE(g_xml.textures, g_xml.texture_cap, g_xml.texture_count + 1);
		g_xml.textures[g_xml.texture_count].path = full;
		g_xml.textures[g_xml.texture_count].id = texture;
		g_xml.texture_count++;
	}
	return texture;
}

HUI_id hui_xml_font(const char* path) {
	char* full = hui_xml_resolve(path);
	HUI_id font;
	uint32_t i;

	for (i = 0; i < g_xml.font_count; i++) {
		if (strcmp(g_xml.fonts[i].path, full) != 0) continue;
		if (HUI_IsValidFont(g_xml.fonts[i].id)) {
			hui_free(full);
			return g_xml.fonts[i].id;
		}
		break;
	}
	font = HUI_CreateFontFromFile(full);
	if (font == HUI_INVALID_ID) {
		hui_free(full);
		return HUI_INVALID_ID;
	}
	if (i < g_xml.font_count) {
		g_xml.fonts[i].id = font;
		hui_free(full);
	} else {
		HUI_ARRAY_RESERVE(g_xml.fonts, g_xml.font_cap, g_xml.font_count + 1);
		g_xml.fonts[g_xml.font_count].path = full;
		g_xml.fonts[g_xml.font_count].id = font;
		g_xml.font_count++;
	}
	return font;
}

/** Forgets the resource cache. The textures and fonts themselves belong to the library pools. */
void hui_xml_shutdown(void) {
	uint32_t i;
	for (i = 0; i < g_xml.texture_count; i++) hui_free(g_xml.textures[i].path);
	for (i = 0; i < g_xml.font_count; i++) hui_free(g_xml.fonts[i].path);
	hui_free(g_xml.textures);
	hui_free(g_xml.fonts);
	hui_free(g_xml.file_dir);
	memset(&g_xml, 0, sizeof(g_xml));
}

/* ---------------------------------------------------------------------------
 *  Building
 * ------------------------------------------------------------------------- */

static void hui_xml_error(const hui_xml_node_t* node, HUI_ESeverity severity, const char* format, ...) {
	char message[256];
	va_list args;
	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);
	hui_error(HUI_ERROR_XML, severity, "XML line %d, column %d: %s", node->line, node->column, message);
}

static hui_widget_t* hui_xml_build(const hui_xml_node_t* node) {
	const hui_widget_class_t* cls = hui_widget_class_by_name(node->name);
	const hui_xml_attribute_t* attribute;
	const hui_xml_node_t* child;
	hui_widget_t* widget;

	if (!cls) {
		hui_xml_error(node, HUI_SEVERITY_ERROR, "unknown widget <%.60s>", node->name);
		return NULL;
	}
	widget = hui_widget_create(cls->type);
	if (!widget) return NULL;

	/* children first: attributes such as Selected="1" or ActiveTab="2" refer to them */
	for (child = node->first_child; child; child = child->next) {
		hui_widget_t* child_widget;
		/* the class may give the tag its own meaning (<Item>, <Separator> in a menu) */
		if (cls->xml_child && cls->xml_child(widget, child)) continue;
		if (cls->flags & HUI_CF_NO_CHILDREN) {
			hui_xml_error(child, HUI_SEVERITY_ERROR, "<%.60s> cannot contain <%.60s>", node->name, child->name);
			hui_widget_destroy(widget);
			return NULL;
		}
		child_widget = hui_xml_build(child);
		if (!child_widget) {
			hui_widget_destroy(widget);
			return NULL;
		}
		hui_widget_add_child(widget, child_widget, -1);
	}

	/* <Label>Hello</Label> is the same as <Label Text="Hello"/> */
	if (node->text[0] && !hui_xml_attribute(node, "Text") && !node->first_child) {
		hui_widget_set_attribute(widget, "Text", node->text);
	}
	for (attribute = node->attributes; attribute; attribute = attribute->next) {
		if (!hui_widget_set_attribute(widget, attribute->name, attribute->value)) {
			/* a typo should not prevent the rest of the interface from loading */
			hui_xml_error(node, HUI_SEVERITY_WARNING, "<%.60s>: unknown attribute or invalid value %.60s=\"%.60s\"",
				node->name, attribute->name, attribute->value);
		}
	}
	return widget;
}

static int hui_xml_load(hui_xml_document_t* document, const char* xml, size_t size, const char* function) {
	if (!xml) {
		hui_error(HUI_ERROR_INVALID_VALUE, HUI_SEVERITY_ERROR, "%s: NULL text", function);
		return HUI_FALSE;
	}
	if (!hui_xml_parse(document, xml, size)) {
		hui_error(HUI_ERROR_XML, HUI_SEVERITY_ERROR, "%s: line %d, column %d: %s", function,
			document->error_line, document->error_column, document->error);
		hui_xml_free(document);
		return HUI_FALSE;
	}
	return HUI_TRUE;
}

static HUI_id hui_xml_create_widget(const char* xml, size_t size, const char* function) {
	hui_xml_document_t document;
	hui_widget_t* widget;
	if (!hui_xml_load(&document, xml, size, function)) return HUI_INVALID_ID;
	if (hui_stricmp(document.root->name, "Window") == 0) {
		hui_xml_error(document.root, HUI_SEVERITY_ERROR, "the root element is a <Window>: use HUI_CreateWindowFromXml");
		hui_xml_free(&document);
		return HUI_INVALID_ID;
	}
	widget = hui_xml_build(document.root);
	hui_xml_free(&document);
	return widget ? widget->id : HUI_INVALID_ID;
}

static HUI_id hui_xml_create_window(const char* xml, size_t size, const char* function) {
	hui_xml_document_t document;
	const hui_xml_node_t* root;
	const hui_xml_node_t* child;
	const hui_xml_attribute_t* attribute;
	hui_widget_t* content = NULL;
	hui_window_t* window_object;
	const char* title = "";
	int width = 800, height = 600, min_width = 0, min_height = 0, flag;
	HUI_uint flags = HUI_WINDOW_DEFAULT;
	hui_color_t clear = 0;
	int has_clear = HUI_FALSE;
	HUI_id window;

	if (!hui_xml_load(&document, xml, size, function)) return HUI_INVALID_ID;
	root = document.root;
	if (hui_stricmp(root->name, "Window") != 0) {
		hui_xml_error(root, HUI_SEVERITY_ERROR, "the root element must be <Window>, found <%.60s>", root->name);
		hui_xml_free(&document);
		return HUI_INVALID_ID;
	}

	for (attribute = root->attributes; attribute; attribute = attribute->next) {
		const char* name = attribute->name;
		const char* value = attribute->value;
		int ok = HUI_TRUE;
		if (HUI_ATTR("Title")) title = value;
		else if (HUI_ATTR("Width")) ok = hui_parse_int(value, &width) && width > 0;
		else if (HUI_ATTR("Height")) ok = hui_parse_int(value, &height) && height > 0;
		else if (HUI_ATTR("MinWidth")) ok = hui_parse_int(value, &min_width);
		else if (HUI_ATTR("MinHeight")) ok = hui_parse_int(value, &min_height);
		else if (HUI_ATTR("Resizable")) {
			ok = hui_parse_bool(value, &flag);
			if (ok && !flag) flags |= HUI_WINDOW_NO_RESIZE;
		} else if (HUI_ATTR("Borderless")) {
			ok = hui_parse_bool(value, &flag);
			if (ok && flag) flags |= HUI_WINDOW_BORDERLESS;
		} else if (HUI_ATTR("Maximized")) {
			ok = hui_parse_bool(value, &flag);
			if (ok && flag) flags |= HUI_WINDOW_MAXIMIZED;
		} else if (HUI_ATTR("Hidden")) {
			ok = hui_parse_bool(value, &flag);
			if (ok && flag) flags |= HUI_WINDOW_HIDDEN;
		} else if (HUI_ATTR("ClearColor")) {
			ok = hui_parse_color(value, &clear);
			has_clear = ok;
		} else {
			ok = HUI_FALSE;
		}
		if (!ok) {
			hui_xml_error(root, HUI_SEVERITY_WARNING, "<Window>: unknown attribute or invalid value %.60s=\"%.60s\"", name, value);
		}
	}

	/* the content is built first: a mistake in it must not leave an empty window behind */
	for (child = root->first_child; child; child = child->next) {
		if (hui_stricmp(child->name, "Shortcut") == 0) continue;
		if (content) {
			hui_xml_error(child, HUI_SEVERITY_ERROR, "a <Window> has a single content element; wrap them in a panel");
			hui_widget_destroy(content);
			hui_xml_free(&document);
			return HUI_INVALID_ID;
		}
		content = hui_xml_build(child);
		if (!content) {
			hui_xml_free(&document);
			return HUI_INVALID_ID;
		}
	}

	window = HUI_CreateWindow(title, width, height, flags);
	if (window == HUI_INVALID_ID) {
		if (content) hui_widget_destroy(content);
		hui_xml_free(&document);
		return HUI_INVALID_ID;
	}
	window_object = hui_window_get(window);
	if (min_width > 0 || min_height > 0) HUI_SetWindowMinSize(window, min_width, min_height);
	if (has_clear) {
		float r, g, b, a;
		hui_color_to_floats(clear, &r, &g, &b, &a);
		HUI_SetWindowClearColor(window, r, g, b, a);
	}
	if (content) HUI_SetWindowContent(window, content->id);

	/* <Shortcut Keys="Ctrl+S" Action="file.save"/> */
	for (child = root->first_child; child; child = child->next) {
		const char* keys;
		const char* action;
		HUI_EKey key;
		uint32_t modifiers;
		if (hui_stricmp(child->name, "Shortcut") != 0) continue;
		keys = hui_xml_attribute(child, "Keys");
		action = hui_xml_attribute(child, "Action");
		if (!keys || !action || !action[0] || !hui_parse_shortcut(keys, &key, &modifiers)) {
			hui_xml_error(child, HUI_SEVERITY_WARNING, "<Shortcut> needs Keys=\"Ctrl+S\" and Action=\"name\"");
			continue;
		}
		if (window_object) hui_window_add_shortcut(window_object, key, modifiers, action);
	}

	hui_xml_free(&document);
	return window;
}

/* ---------------------------------------------------------------------------
 *  Public API
 * ------------------------------------------------------------------------- */

HUI_id HUI_CreateWidgetFromXml(const char* xml, size_t size) {
	HUI_API_GUARD(HUI_INVALID_ID);
	return hui_xml_create_widget(xml, size, __func__);
}

HUI_id HUI_CreateWindowFromXml(const char* xml, size_t size) {
	HUI_API_GUARD(HUI_INVALID_ID);
	return hui_xml_create_window(xml, size, __func__);
}

typedef HUI_id (*hui_xml_create_fn)(const char* xml, size_t size, const char* function);

static HUI_id hui_xml_create_from_file(const char* path, hui_xml_create_fn create, const char* function) {
	size_t size = 0;
	char* text = hui_read_file(path, &size);
	char* previous_dir = g_xml.file_dir;
	HUI_id result;
	if (!text) {
		hui_error(HUI_ERROR_FILE_NOT_FOUND, HUI_SEVERITY_ERROR, "%s: cannot read '%s'", function, path ? path : "");
		return HUI_INVALID_ID;
	}
	/* relative paths inside the file are relative to the file */
	g_xml.file_dir = hui_path_dir(path);
	result = create(text, size, function);
	hui_free(g_xml.file_dir);
	g_xml.file_dir = previous_dir;
	hui_free(text);
	return result;
}

HUI_id HUI_CreateWidgetFromXmlFile(const char* path) {
	HUI_API_GUARD(HUI_INVALID_ID);
	return hui_xml_create_from_file(path, hui_xml_create_widget, __func__);
}

HUI_id HUI_CreateWindowFromXmlFile(const char* path) {
	HUI_API_GUARD(HUI_INVALID_ID);
	return hui_xml_create_from_file(path, hui_xml_create_window, __func__);
}

void HUI_SetXmlBasePath(const char* path) {
	HUI_API_GUARD_VOID();
	hui_str_set(&g_hui.xml_base_path, (path && path[0]) ? path : NULL);
}
