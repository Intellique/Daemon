/****************************************************************************\
*                    ______           _      ____                            *
*                   / __/ /____  ____(_)__ _/ __ \___  ___                   *
*                  _\ \/ __/ _ \/ __/ / _ `/ /_/ / _ \/ -_)                  *
*                 /___/\__/\___/_/ /_/\_, /\____/_//_/\__/                   *
*                                      /_/                                   *
*  ------------------------------------------------------------------------  *
*  This file is a part of Storiq One                                         *
*                                                                            *
*  Storiq One is free software; you can redistribute it and/or modify        *
*  it under the terms of the GNU Affero General Public License               *
*  as published by the Free Software Foundation; either version 3            *
*  of the License, or (at your option) any later version.                    *
*                                                                            *
*  This program is distributed in the hope that it will be useful,           *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of            *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
*  GNU Affero General Public License for more details.                       *
*                                                                            *
*  You should have received a copy of the GNU Affero General Public License  *
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.     *
*                                                                            *
*  ------------------------------------------------------------------------  *
*  Copyright (C) 2013-2016, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// expat
#include <expat.h>
// dprintf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// strlen
#include <string.h>


#include <libstoriqone/value.h>

#include "xml.h"

struct sodr_tape_drive_xml_state {
	struct so_value * root;
	struct so_value * stack_object;
	struct so_value * current;
};

static void sodr_tape_drive_xml_character_data(void * userData, const XML_Char *s, int len);
ssize_t sodr_tape_drive_xml_encode_inner(struct so_value * xml, int fd);
static void sodr_tape_drive_xml_end_element(void * userData, const char * name);
static void sodr_tape_drive_xml_start_element(void * userData, const char * name, const char ** atts);


static void sodr_tape_drive_xml_character_data(void * userData, const XML_Char *s, int len) {
	struct sodr_tape_drive_xml_state * state = userData;

	char * content = NULL;
	so_value_unpack(state->current, "{ss}", "value", &content);

	if (content != NULL) {
		size_t content_length = strlen(content);
		size_t new_length = content_length + len;
		char * new_content = malloc(new_length + 1);
		strcpy(new_content, content);
		strncpy(new_content + content_length, s, len);
		new_content[new_length] = '\0';

		so_value_hashtable_put2(state->current, "value", so_value_new_string(new_content), true);

		free(content);
		free(new_content);
	} else
		so_value_hashtable_put2(state->current, "value", so_value_new_string2(s, len), true);
}

ssize_t sodr_tape_drive_xml_encode(struct so_value * xml, int fd) {
}

ssize_t sodr_tape_drive_xml_encode_inner(struct so_value * xml, int fd) {
	ssize_t nb_total_write = 0;

	char * name = NULL;
	so_value_unpack(xml, "{ss}", "name", &name);

	ssize_t nb_write = dprintf(fd, "<%s", name);
	if (nb_write < 0)
		return nb_write;
	else
		nb_total_write += nb_write;

	struct so_value * attributes = NULL;
	so_value_unpack(xml, "{so}", "attributes", &attributes);
	if (attributes != NULL) {
		struct so_value_iterator * iter = so_value_hashtable_get_iterator(attributes);
		while (so_value_iterator_has_next(iter)) {
			struct so_value * key = so_value_iterator_get_key(iter, false, false);
			struct so_value * value = so_value_iterator_get_value(iter, value);

			nb_write = dprintf(fd, " %s=\"%s\"", so_value_string_get(key), so_value_string_get(value));
			if (nb_write < 0) {
				so_value_iterator_free(iter);
				return nb_write;
			} else
				nb_total_write += nb_write;
		}
		so_value_iterator_free(iter);
	}

	nb_write += dprintf(fd, ">\n");
	if (nb_write < 0)
		return nb_write;
	else
		nb_total_write += nb_write;

	nb_write += dprintf(fd, "</%s>\n", name);
	if (nb_write < 0)
		return nb_write;
	else
		nb_total_write += nb_write;
}

static void sodr_tape_drive_xml_end_element(void * userData, const char * name __attribute__((unused))) {
	struct sodr_tape_drive_xml_state * state = userData;
	state->current = so_value_list_pop(state->stack_object);
}

struct so_value * sodr_tape_drive_xml_parse_string(const char * xml) {
	if (xml == NULL)
		return NULL;

	struct sodr_tape_drive_xml_state state = {
		.root         = NULL,
		.stack_object = so_value_new_linked_list(),
		.current      = NULL,
	};

	XML_Parser parser = XML_ParserCreate(NULL);
	XML_SetUserData(parser, &state);
	XML_SetCharacterDataHandler(parser, sodr_tape_drive_xml_character_data);
	XML_SetElementHandler(parser, sodr_tape_drive_xml_start_element, sodr_tape_drive_xml_end_element);

	int failed = XML_Parse(parser, xml, strlen(xml), true);
	XML_ParserFree(parser);

	so_value_free(state.stack_object);

	if (failed == XML_STATUS_OK)
		return state.root;

	so_value_free(state.root);
	return NULL;
}

static void sodr_tape_drive_xml_start_element(void * userData, const char * name, const char ** atts) {
	struct sodr_tape_drive_xml_state * state = userData;

	struct so_value * attributes = NULL;
	if (ptr_atts[0] != NULL) {
		attributes = so_value_new_hashtable2();
		const char ** ptr_atts;
		for (ptr_atts = atts; ptr_atts[0] != NULL; ptr_atts += 2)
			so_value_hashtable_put2(attributes, ptr_atts[0], so_value_new_string(ptr_atts[1]), true);
	}

	struct so_value * obj = so_value_pack("{sssos[]sn}",
		"name", name,
		"attributes", attributes,
		"children",
		"value"
	);

	if (state->root == NULL)
		state->root = obj;
	else {
		struct so_value * children;
		so_value_unpack(state->current, "{so}", "children", &children);
		so_value_list_push(children, obj, true);

		so_value_list_push(state->stack_object, state->current, true);
	}
	state->current = obj;
}

