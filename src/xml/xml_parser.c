/**
 * HUI - xml/xml_parser.c
 * Recursive-descent XML reader.
 */

#include "xml.h"

#define HUI_XML_MAX_DEPTH 256

typedef struct hui_xml_reader_t {
	hui_xml_document_t* document;
	const char* cursor;
	const char* end;
	int line;
	int column;
	int failed;
	/* scratch buffer for decoded text and attribute values */
	char* buffer;
	uint32_t buffer_count;
	uint32_t buffer_cap;
} hui_xml_reader_t;

/* ---------------------------------------------------------------------------
 *  Reading
 * ------------------------------------------------------------------------- */

static void hui_xml_fail(hui_xml_reader_t* reader, const char* message) {
	if (reader->failed) return;
	reader->failed = HUI_TRUE;
	snprintf(reader->document->error, sizeof(reader->document->error), "%s", message);
	reader->document->error_line = reader->line;
	reader->document->error_column = reader->column;
}

static int hui_xml_eof(const hui_xml_reader_t* reader) {
	return reader->cursor >= reader->end;
}

static char hui_xml_peek(const hui_xml_reader_t* reader) {
	return reader->cursor < reader->end ? *reader->cursor : '\0';
}

static void hui_xml_advance(hui_xml_reader_t* reader) {
	if (reader->cursor >= reader->end) return;
	if (*reader->cursor == '\n') {
		reader->line++;
		reader->column = 1;
	} else if (((unsigned char)*reader->cursor & 0xC0u) != 0x80u) {
		/* count characters, not bytes */
		reader->column++;
	}
	reader->cursor++;
}

static int hui_xml_starts_with(const hui_xml_reader_t* reader, const char* literal) {
	size_t length = strlen(literal);
	return (size_t)(reader->end - reader->cursor) >= length && memcmp(reader->cursor, literal, length) == 0;
}

static void hui_xml_skip(hui_xml_reader_t* reader, size_t count) {
	while (count-- > 0) hui_xml_advance(reader);
}

static int hui_xml_is_space(char c) {
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static void hui_xml_skip_space(hui_xml_reader_t* reader) {
	while (!hui_xml_eof(reader) && hui_xml_is_space(*reader->cursor)) hui_xml_advance(reader);
}

/* Skips to just after `terminator`. Returns HUI_FALSE when it is missing. */
static int hui_xml_skip_until(hui_xml_reader_t* reader, const char* terminator) {
	while (!hui_xml_eof(reader)) {
		if (hui_xml_starts_with(reader, terminator)) {
			hui_xml_skip(reader, strlen(terminator));
			return HUI_TRUE;
		}
		hui_xml_advance(reader);
	}
	return HUI_FALSE;
}

static int hui_xml_is_name_char(char c) {
	unsigned char u = (unsigned char)c;
	return (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z') || (u >= '0' && u <= '9') ||
		u == '_' || u == '-' || u == '.' || u == ':' || u >= 0x80u;
}

static const char* hui_xml_read_name(hui_xml_reader_t* reader) {
	const char* start = reader->cursor;
	while (!hui_xml_eof(reader) && hui_xml_is_name_char(*reader->cursor)) hui_xml_advance(reader);
	if (reader->cursor == start) {
		hui_xml_fail(reader, "expected a name");
		return "";
	}
	return hui_arena_strndup(&reader->document->arena, start, (size_t)(reader->cursor - start));
}

/* ---------------------------------------------------------------------------
 *  Scratch buffer
 * ------------------------------------------------------------------------- */

static void hui_xml_buffer_push(hui_xml_reader_t* reader, const char* bytes, size_t count) {
	HUI_ARRAY_RESERVE(reader->buffer, reader->buffer_cap, reader->buffer_count + (uint32_t)count + 1);
	memcpy(reader->buffer + reader->buffer_count, bytes, count);
	reader->buffer_count += (uint32_t)count;
}

/* Decodes "&name;" at the cursor into the buffer. */
static void hui_xml_read_entity(hui_xml_reader_t* reader) {
	static const struct { const char* name; char value; } named[] = {
		{ "&lt;", '<' }, { "&gt;", '>' }, { "&amp;", '&' }, { "&quot;", '"' }, { "&apos;", '\'' }
	};
	size_t i;
	for (i = 0; i < HUI_COUNT_OF(named); i++) {
		if (hui_xml_starts_with(reader, named[i].name)) {
			hui_xml_buffer_push(reader, &named[i].value, 1);
			hui_xml_skip(reader, strlen(named[i].name));
			return;
		}
	}
	if (hui_xml_starts_with(reader, "&#")) {
		const char* p = reader->cursor + 2;
		uint32_t codepoint = 0;
		int digits = 0, hex = 0;
		char encoded[4];
		if (p < reader->end && (*p == 'x' || *p == 'X')) {
			hex = 1;
			p++;
		}
		while (p < reader->end && *p != ';' && digits < 8) {
			char c = *p;
			uint32_t digit;
			if (c >= '0' && c <= '9') digit = (uint32_t)(c - '0');
			else if (hex && c >= 'a' && c <= 'f') digit = (uint32_t)(c - 'a' + 10);
			else if (hex && c >= 'A' && c <= 'F') digit = (uint32_t)(c - 'A' + 10);
			else break;
			codepoint = codepoint * (hex ? 16u : 10u) + digit;
			digits++;
			p++;
		}
		if (digits > 0 && p < reader->end && *p == ';' && codepoint > 0 && codepoint <= 0x10FFFFu) {
			hui_xml_buffer_push(reader, encoded, (size_t)hui_utf8_encode(codepoint, encoded));
			hui_xml_skip(reader, (size_t)(p + 1 - reader->cursor));
			return;
		}
	}
	hui_xml_fail(reader, "unknown entity");
}

/* ---------------------------------------------------------------------------
 *  Grammar
 * ------------------------------------------------------------------------- */

static void hui_xml_read_attributes(hui_xml_reader_t* reader, hui_xml_node_t* node) {
	hui_xml_attribute_t* last = NULL;
	for (;;) {
		hui_xml_attribute_t* attribute;
		char quote;

		hui_xml_skip_space(reader);
		if (hui_xml_eof(reader) || hui_xml_peek(reader) == '>' || hui_xml_peek(reader) == '/' || reader->failed) return;

		attribute = (hui_xml_attribute_t*)hui_arena_alloc(&reader->document->arena, sizeof(hui_xml_attribute_t));
		attribute->next = NULL;
		attribute->value = "";
		attribute->name = hui_xml_read_name(reader);
		if (reader->failed) return;

		hui_xml_skip_space(reader);
		if (hui_xml_peek(reader) != '=') {
			hui_xml_fail(reader, "expected '=' after the attribute name");
			return;
		}
		hui_xml_advance(reader);
		hui_xml_skip_space(reader);
		quote = hui_xml_peek(reader);
		if (quote != '"' && quote != '\'') {
			hui_xml_fail(reader, "attribute values must be quoted");
			return;
		}
		hui_xml_advance(reader);

		reader->buffer_count = 0;
		while (!hui_xml_eof(reader) && hui_xml_peek(reader) != quote && !reader->failed) {
			if (hui_xml_peek(reader) == '&') {
				hui_xml_read_entity(reader);
			} else if (hui_xml_peek(reader) == '<') {
				hui_xml_fail(reader, "'<' is not allowed in an attribute value");
			} else {
				hui_xml_buffer_push(reader, reader->cursor, 1);
				hui_xml_advance(reader);
			}
		}
		if (reader->failed) return;
		if (hui_xml_eof(reader)) {
			hui_xml_fail(reader, "unterminated attribute value");
			return;
		}
		hui_xml_advance(reader);
		attribute->value = hui_arena_strndup(&reader->document->arena, reader->buffer ? reader->buffer : "", reader->buffer_count);

		if (last) last->next = attribute;
		else node->attributes = attribute;
		last = attribute;
	}
}

static char* hui_xml_trim(hui_xml_reader_t* reader, const char* text, size_t length) {
	while (length > 0 && hui_xml_is_space(text[0])) {
		text++;
		length--;
	}
	while (length > 0 && hui_xml_is_space(text[length - 1])) length--;
	return hui_arena_strndup(&reader->document->arena, text, length);
}

static hui_xml_node_t* hui_xml_read_element(hui_xml_reader_t* reader, hui_xml_node_t* parent, int depth) {
	hui_xml_node_t* node;
	char* text = NULL;
	uint32_t text_count = 0, text_cap = 0;

	if (depth > HUI_XML_MAX_DEPTH) {
		hui_xml_fail(reader, "elements are nested too deeply");
		return NULL;
	}
	node = (hui_xml_node_t*)hui_arena_alloc(&reader->document->arena, sizeof(hui_xml_node_t));
	memset(node, 0, sizeof(*node));
	node->text = "";
	node->parent = parent;
	node->line = reader->line;
	node->column = reader->column;

	hui_xml_advance(reader); /* '<' */
	node->name = hui_xml_read_name(reader);
	hui_xml_read_attributes(reader, node);
	if (reader->failed) return NULL;

	if (hui_xml_starts_with(reader, "/>")) {
		hui_xml_skip(reader, 2);
		return node;
	}
	if (hui_xml_peek(reader) != '>') {
		hui_xml_fail(reader, "expected '>' to close the tag");
		return NULL;
	}
	hui_xml_advance(reader);

	/* content: text, child elements, comments, CDATA, until the closing tag */
	while (!reader->failed) {
		if (hui_xml_eof(reader)) {
			hui_xml_fail(reader, "missing closing tag");
			break;
		}
		if (hui_xml_starts_with(reader, "<!--")) {
			if (!hui_xml_skip_until(reader, "-->")) hui_xml_fail(reader, "unterminated comment");
		} else if (hui_xml_starts_with(reader, "<![CDATA[")) {
			const char* start;
			hui_xml_skip(reader, 9);
			start = reader->cursor;
			if (!hui_xml_skip_until(reader, "]]>")) {
				hui_xml_fail(reader, "unterminated CDATA section");
			} else {
				size_t length = (size_t)(reader->cursor - 3 - start);
				HUI_ARRAY_RESERVE(text, text_cap, text_count + (uint32_t)length + 1);
				memcpy(text + text_count, start, length);
				text_count += (uint32_t)length;
			}
		} else if (hui_xml_starts_with(reader, "</")) {
			const char* name;
			hui_xml_skip(reader, 2);
			name = hui_xml_read_name(reader);
			hui_xml_skip_space(reader);
			if (!reader->failed && strcmp(name, node->name) != 0) {
				char message[160];
				snprintf(message, sizeof(message), "closing tag </%.40s> does not match <%.40s>", name, node->name);
				hui_xml_fail(reader, message);
			} else if (!reader->failed && hui_xml_peek(reader) != '>') {
				hui_xml_fail(reader, "expected '>' to close the tag");
			} else {
				hui_xml_advance(reader);
			}
			break;
		} else if (hui_xml_starts_with(reader, "<?")) {
			if (!hui_xml_skip_until(reader, "?>")) hui_xml_fail(reader, "unterminated processing instruction");
		} else if (hui_xml_peek(reader) == '<') {
			hui_xml_node_t* child = hui_xml_read_element(reader, node, depth + 1);
			if (child) {
				if (node->last_child) node->last_child->next = child;
				else node->first_child = child;
				node->last_child = child;
			}
		} else if (hui_xml_peek(reader) == '&') {
			reader->buffer_count = 0;
			hui_xml_read_entity(reader);
			if (!reader->failed) {
				HUI_ARRAY_RESERVE(text, text_cap, text_count + reader->buffer_count + 1);
				memcpy(text + text_count, reader->buffer, reader->buffer_count);
				text_count += reader->buffer_count;
			}
		} else {
			HUI_ARRAY_RESERVE(text, text_cap, text_count + 2);
			text[text_count++] = *reader->cursor;
			hui_xml_advance(reader);
		}
	}
	if (text_count > 0 && !reader->failed) node->text = hui_xml_trim(reader, text, text_count);
	hui_free(text);
	return reader->failed ? NULL : node;
}

int hui_xml_parse(hui_xml_document_t* document, const char* text, size_t size) {
	hui_xml_reader_t reader;

	memset(document, 0, sizeof(*document));
	hui_arena_init(&document->arena, 16384);
	memset(&reader, 0, sizeof(reader));
	reader.document = document;
	reader.cursor = text ? text : "";
	reader.end = reader.cursor + (text ? size : 0);
	reader.line = 1;
	reader.column = 1;

	/* UTF-8 byte order mark */
	if (hui_xml_starts_with(&reader, "\xEF\xBB\xBF")) reader.cursor += 3;

	/* prolog: declaration, comments, doctype */
	for (;;) {
		hui_xml_skip_space(&reader);
		if (hui_xml_starts_with(&reader, "<?")) {
			if (!hui_xml_skip_until(&reader, "?>")) hui_xml_fail(&reader, "unterminated XML declaration");
		} else if (hui_xml_starts_with(&reader, "<!--")) {
			if (!hui_xml_skip_until(&reader, "-->")) hui_xml_fail(&reader, "unterminated comment");
		} else if (hui_xml_starts_with(&reader, "<!")) {
			if (!hui_xml_skip_until(&reader, ">")) hui_xml_fail(&reader, "unterminated declaration");
		} else {
			break;
		}
		if (reader.failed) break;
	}

	if (!reader.failed) {
		if (hui_xml_peek(&reader) != '<') hui_xml_fail(&reader, "expected a root element");
		else document->root = hui_xml_read_element(&reader, NULL, 0);
	}
	if (!reader.failed) {
		/* only comments and white space may follow the root element */
		for (;;) {
			hui_xml_skip_space(&reader);
			if (hui_xml_starts_with(&reader, "<!--")) {
				if (!hui_xml_skip_until(&reader, "-->")) hui_xml_fail(&reader, "unterminated comment");
			} else {
				break;
			}
			if (reader.failed) break;
		}
		if (!reader.failed && !hui_xml_eof(&reader)) hui_xml_fail(&reader, "unexpected content after the root element");
	}

	hui_free(reader.buffer);
	if (reader.failed) {
		document->root = NULL;
		return HUI_FALSE;
	}
	return HUI_TRUE;
}

void hui_xml_free(hui_xml_document_t* document) {
	hui_arena_free(&document->arena);
	document->root = NULL;
}

const char* hui_xml_attribute(const hui_xml_node_t* node, const char* name) {
	const hui_xml_attribute_t* attribute;
	for (attribute = node->attributes; attribute; attribute = attribute->next) {
		if (hui_stricmp(attribute->name, name) == 0) return attribute->value;
	}
	return NULL;
}
