/****************************************************************************\
*                             __________                                     *
*                            / __/_  __/__  ___  ___                         *
*                           _\ \  / / / _ \/ _ \/ -_)                        *
*                          /___/ /_/  \___/_//_/\__/                         *
*                                                                            *
*  ------------------------------------------------------------------------  *
*  This file is a part of STone                                              *
*                                                                            *
*  STone is free software; you can redistribute it and/or modify             *
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
*  Copyright (C) 2013-2015, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Fri, 17 Jan 2014 16:49:11 +0100                            *
\****************************************************************************/

#define _GNU_SOURCE
// glob, globfree
#include <glob.h>
// asprintf
#include <stdio.h>
// strdup
#include <string.h>
// malloc
#include <stdlib.h>
// stat
#include <sys/stat.h>
// stat
#include <sys/types.h>
// time
#include <time.h>
// stat
#include <unistd.h>

#include <libstone/library/media.h>
#include <libstone/library/ressource.h>
#include <libstone/util/file.h>

#include "common.h"

static unsigned int st_vtl_media_count_files(const char * base_dir);


static unsigned int st_vtl_media_count_files(const char * base_dir) {
	char * pattern;
	asprintf(&pattern, "%s/file_*", base_dir);

	glob_t gl;
	glob(pattern, 0, NULL, &gl);

	unsigned nb_files = gl.gl_pathc;

	globfree(&gl);

	return nb_files;
}

struct st_media * st_vtl_media_init(char * base_dir, char * prefix, unsigned int index, struct st_media_format * format) {
	char * serial_file;
	asprintf(&serial_file, "%s/serial_number", base_dir);

	char * serial = st_util_file_get_serial(serial_file);
	free(serial_file);

	struct st_media * media = st_media_get_by_medium_serial_number(serial);
	if (media != NULL) {
		free(serial);
	} else {
		media = malloc(sizeof(struct st_media));
		media->uuid[0] = '\0';
		asprintf(&media->label, "%s%03u", prefix, index);
		media->medium_serial_number = serial;
		asprintf(&media->name, "%s%03u", prefix, index);

		unsigned int nb_files = st_vtl_media_count_files(base_dir);

		media->status = nb_files > 0 ? st_media_status_in_use : st_media_status_new;
		media->location = st_media_location_online;

		media->first_used = time(NULL);
		media->use_before = media->first_used + format->life_span;
		media->last_read = media->last_write = 0;

		media->load_count = 0;
		media->read_count = 0;
		media->write_count = 0;
		media->operation_count = 0;

		media->nb_total_read = media->nb_total_write = 0;
		media->nb_read_errors = media->nb_write_errors = 0;

		media->block_size = format->block_size;
		media->free_block = format->capacity / format->block_size;
		media->total_block = media->free_block;

		media->nb_volumes = nb_files;
		media->type = st_media_type_rewritable;

		media->format = format;
		media->pool = NULL;

		media->lock = st_ressource_new(true);
		media->locked = false;

		media->db_data = NULL;

		st_media_add(media);
	}

	struct st_vtl_media * self = media->data = malloc(sizeof(struct st_vtl_media));
	self->path = base_dir;
	self->slot = NULL;
	self->prefix = strdup(prefix);
	self->used = false;

	st_vtl_media_update(media);

	return media;
}

void st_vtl_media_update(struct st_media * media) {
	struct st_vtl_media * self = media->data;

	char * pattern;
	asprintf(&pattern, "%s/file_*", self->path);

	glob_t gl;
	glob(pattern, 0, NULL, &gl);

	unsigned int i;
	size_t total_size = 0;
	for (i = 0; i < gl.gl_pathc; i++) {
		struct stat st;
		stat(gl.gl_pathv[i], &st);
		total_size += st.st_size;
	}

	media->free_block = media->total_block - total_size / media->block_size;

	globfree(&gl);
}

