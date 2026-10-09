/**
 * Raven - xml/xml.h
 * Minimal XML reader: elements, attributes, text, comments, CDATA and the
 * predefined / numeric entities. No DTD, no namespaces.
 *
 * The document owns every string through one arena: releasing it frees all.
 */

#ifndef RVN_XML_INTERNAL_H
#define RVN_XML_INTERNAL_H

#include "../core/base.h"

typedef struct rvn_xml_attribute_t {
	const char* name;
	const char* value;
	struct rvn_xml_attribute_t* next;
} rvn_xml_attribute_t;

typedef struct rvn_xml_node_t {
	const char* name;
	/** Text content of the element with surrounding white space removed; "" when it has none. */
	const char* text;
	rvn_xml_attribute_t* attributes;
	struct rvn_xml_node_t* parent;
	struct rvn_xml_node_t* first_child;
	struct rvn_xml_node_t* last_child;
	struct rvn_xml_node_t* next;
	/** Position of the opening tag in the source, starting at 1. */
	int line;
	int column;
} rvn_xml_node_t;

typedef struct rvn_xml_document_t {
	rvn_arena_t arena;
	rvn_xml_node_t* root;
	/** Filled when parsing fails. */
	char error[160];
	int error_line;
	int error_column;
} rvn_xml_document_t;

/** Parses `size` bytes of UTF-8. Returns R_FALSE and fills `error` on failure. */
int  rvn_xml_parse(rvn_xml_document_t* document, const char* text, size_t size);
void rvn_xml_free(rvn_xml_document_t* document);
/** Value of an attribute (name compared without case), or NULL. */
const char* rvn_xml_attribute(const rvn_xml_node_t* node, const char* name);

#endif /* RVN_XML_INTERNAL_H */
