/**
 * HUI - xml/xml.h
 * Minimal XML reader: elements, attributes, text, comments, CDATA and the
 * predefined / numeric entities. No DTD, no namespaces.
 *
 * The document owns every string through one arena: releasing it frees all.
 */

#ifndef HUI_XML_INTERNAL_H
#define HUI_XML_INTERNAL_H

#include "../core/base.h"

typedef struct hui_xml_attribute_t {
	const char* name;
	const char* value;
	struct hui_xml_attribute_t* next;
} hui_xml_attribute_t;

typedef struct hui_xml_node_t {
	const char* name;
	/** Text content of the element with surrounding white space removed; "" when it has none. */
	const char* text;
	hui_xml_attribute_t* attributes;
	struct hui_xml_node_t* parent;
	struct hui_xml_node_t* first_child;
	struct hui_xml_node_t* last_child;
	struct hui_xml_node_t* next;
	/** Position of the opening tag in the source, starting at 1. */
	int line;
	int column;
} hui_xml_node_t;

typedef struct hui_xml_document_t {
	hui_arena_t arena;
	hui_xml_node_t* root;
	/** Filled when parsing fails. */
	char error[160];
	int error_line;
	int error_column;
} hui_xml_document_t;

/** Parses `size` bytes of UTF-8. Returns HUI_FALSE and fills `error` on failure. */
int  hui_xml_parse(hui_xml_document_t* document, const char* text, size_t size);
void hui_xml_free(hui_xml_document_t* document);
/** Value of an attribute (name compared without case), or NULL. */
const char* hui_xml_attribute(const hui_xml_node_t* node, const char* name);

#endif /* HUI_XML_INTERNAL_H */
