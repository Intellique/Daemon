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
*  Copyright (C) 2013-2015, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#define _GNU_SOURCE
// asprintf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// strdup
#include <string.h>
// bzero
#include <strings.h>
// time
#include <time.h>

#include <libstoriqone/database.h>
#include <libstoriqone/file.h>
#include <libstoriqone/media.h>
#include <libstoriqone/value.h>

#include "device.h"
#include "util.h"

struct so_media * sochgr_vtl_media_create(const char * root_directory, const char * prefix, long long index, struct so_media_format * format, struct so_database_connection * db_connection) {
	char * media_dir;
	asprintf(&media_dir, "%s/medias/%s%03Ld", root_directory, prefix, index);
	so_file_mkdir(media_dir, 0700);

	char * serial_file;
	asprintf(&serial_file, "%s/medias/%s%03Ld/serial_number", root_directory, prefix, index);

	char * serial_number = sochgr_vtl_util_get_serial(serial_file);
	struct so_media * media = db_connection->ops->get_media(db_connection, serial_number, NULL, NULL);

	if (media != NULL)
		free(serial_number);
	else {
		media = malloc(sizeof(struct so_media));
		bzero(media, sizeof(struct so_media));
		asprintf(&media->label, "%s%03Ld", prefix, index);
		media->medium_serial_number = serial_number;
		media->name = strdup(media->label);
		media->status = so_media_status_new;
		media->first_used = time(NULL);
		media->use_before = media->first_used + format->life_span;
		media->block_size = format->block_size;
		media->free_block = media->total_block = format->capacity / format->block_size;
		media->append = true;
		media->type = so_media_type_rewritable;
		media->media_format = so_media_format_dup(format);
	}

	free(serial_file);
	free(media_dir);

	return media;
}

