/*************************************************************************\
*                            __________                                   *
*                           / __/_  __/__  ___  ___                       *
*                          _\ \  / / / _ \/ _ \/ -_)                      *
*                         /___/ /_/  \___/_//_/\__/                       *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  This file is a part of STone                                           *
*                                                                         *
*  STone is free software; you can redistribute it and/or                 *
*  modify it under the terms of the GNU General Public License            *
*  as published by the Free Software Foundation; either version 3         *
*  of the License, or (at your option) any later version.                 *
*                                                                         *
*  This program is distributed in the hope that it will be useful,        *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
*  GNU General Public License for more details.                           *
*                                                                         *
*  You should have received a copy of the GNU General Public License      *
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  Copyright (C) 2012, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Tue, 01 May 2012 20:58:04 +0200                         *
\*************************************************************************/

#include <jansson.h>
#include <string.h>

#include <stone/io.h>

#include "recv.h"


enum stad_query stad_recv(struct st_stream_reader * reader, char ** prompt, int * status, json_t * lines, json_t ** data) {
	ssize_t buffer_size = 4096;
	ssize_t buffer_pos = 0;
	char * buffer = malloc(buffer_size);
	enum stad_query query = stad_query_unknown;
	for (;;) {
		ssize_t nb_read = reader->ops->read(reader, buffer + buffer_pos, buffer_size - buffer_pos);

		if (nb_read < 0) {
			free(buffer);
			return 0;
		}

		buffer_pos += nb_read;

		json_error_t error;
		json_t * root = json_loadb(buffer, buffer_pos, 0, &error);

		if (!root && buffer_pos + 512 > buffer_size) {
			buffer_size <<= 1;
			buffer = realloc(buffer, buffer_size);
		}

		if (root) {
			free(buffer);

			json_t * jtype = json_object_get(root, "type");
			if (jtype) {
				const char * ctype = json_string_value(jtype);
				if (!ctype) {
					json_decref(root);
					return stad_query_unknown;
				}

				if (!strcmp(ctype, "getline"))
					query = stad_query_getline;
				else if (!strcmp(ctype, "getpassword"))
					query = stad_query_getpassword;
				else if (!strcmp(ctype, "status"))
					query = stad_query_status;
			}

			json_t * jprompt = json_object_get(root, "prompt");
			if (jprompt && prompt)
				*prompt = strdup(json_string_value(jprompt));

			json_t * jstatus = json_object_get(root, "status");
			if (jstatus && status)
				*status = json_integer_value(jstatus);

			json_t * jlines = json_object_get(root, "lines");
			if (jlines && lines)
				json_array_extend(lines, jlines);

			json_t * jdata = json_object_get(root, "data");
			if (jdata && data)
				*data = json_copy(jdata);

			return query;
		}
	}

	return stad_query_unknown;
}

int stad_send(struct st_stream_writer * writer, json_t * data) {
	char * message = json_dumps(data, 0);

	ssize_t nb_write = writer->ops->write(writer, message, strlen(message));

	free(message);

	return nb_write < 0;
}

