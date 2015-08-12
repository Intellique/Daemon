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

// dgettext
#include <libintl.h>
// sscanf
#include <stdio.h>
// free
#include <stdlib.h>
// strcmp, strncpy
#include <string.h>
// bzero
#include <strings.h>
// free, malloc, realloc
#include <stdlib.h>

#include <libstoriqone/format.h>
#include <libstoriqone/io.h>
#include <libstoriqone/log.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/string.h>
#include <libstoriqone/value.h>
#include <libstoriqone-drive/drive.h>

#include "format/ltfs/ltfs.h"
#include "media.h"
#include "xml.h"

static bool sodr_tape_drive_media_check_ltfs_header(struct so_media * media, const char * buffer);


bool sodr_tape_drive_media_check_header(struct so_media * media, const char * buffer) {
	return sodr_tape_drive_media_check_ltfs_header(media, buffer);
}

static bool sodr_tape_drive_media_check_ltfs_header(struct so_media * media __attribute__((unused)), const char * buffer) {
	char vol_id[7];
	int nb_params = sscanf(buffer, "VOL1%6sL             LTFS", vol_id);
	if (nb_params < 1)
		return false;

	so_log_write(so_log_level_debug,
		dgettext("storiqone-drive-tape", "Found LTFS header in media with (volume id: %s)"),
		vol_id);

	return true;
}

void sodr_tape_drive_media_free(struct sodr_tape_drive_media * media_data) {
	unsigned int i;
	switch (media_data->format) {
		case sodr_tape_drive_media_ltfs:
			for (i = 0; i < media_data->data.ltfs.nb_files; i++) {
				struct sodr_tape_drive_format_ltfs_file * file = media_data->data.ltfs.files + i;
				so_format_file_free(&file->file);
			}
			free(media_data->data.ltfs.files);
			break;

		default:
			break;
	}

	free(media_data);
}

void sodr_tape_drive_media_free2(void * private_data) {
	if (private_data != NULL)
		sodr_tape_drive_media_free(private_data);
}

struct sodr_tape_drive_media * sodr_tape_drive_media_new(enum sodr_tape_drive_media_format format) {
	struct sodr_tape_drive_media * mp = malloc(sizeof(struct sodr_tape_drive_media));
	bzero(mp, sizeof(struct sodr_tape_drive_media)); 
	mp->format = format;

	return mp;
}

enum sodr_tape_drive_media_format sodr_tape_drive_parse_label(const char * buffer) {
	char storiqone_version[65];
	int media_format_version = 0;

	int nb_params = sscanf(buffer, "Storiq One (%64[^)])\nMedia format: version=%d\n", storiqone_version, &media_format_version);
	if (nb_params < 2)
		nb_params = sscanf(buffer, "STone (%64[^)])\nTape format: version=%d\n", storiqone_version, &media_format_version);

	if (nb_params == 2)
		return media_format_version > 0 && media_format_version < 4 ? sodr_tape_drive_media_storiq_one : sodr_tape_drive_media_unknown;


	char vol_id[7];
	nb_params = sscanf(buffer, "VOL1%6sL             LTFS", vol_id);
	return nb_params == 1 ? sodr_tape_drive_media_ltfs : sodr_tape_drive_media_unknown;
}

int sodr_tape_drive_media_parse_ltfs_index(struct so_drive * drive, struct so_database_connection * db_connect) {
	struct so_media * media = drive->slot->media;
	struct sodr_tape_drive_media * mp = media->private_data;

	struct so_stream_reader * reader = drive->ops->get_raw_reader(0, db_connect);

	char buffer_label[82];
	ssize_t nb_read = reader->ops->read(reader, buffer_label, 82);

	reader->ops->close(reader);
	reader->ops->free(reader);

	strncpy(mp->data.ltfs.owner_identifier, buffer_label + 37, 14);
	mp->data.ltfs.owner_identifier[14] = '\0';
	if (mp->data.ltfs.owner_identifier[0] == ' ')
		mp->data.ltfs.owner_identifier[0] = '\0';
	else
		so_string_rtrim(mp->data.ltfs.owner_identifier, ' ');


	reader = drive->ops->get_raw_reader(3, db_connect);
	if (reader == NULL) {
		so_log_write(so_log_level_debug,
			dgettext("storiqone-drive-tape", "Failed to read LTFS index from media '%s'"),
			media->name);
		return 1;
	}

	ssize_t buffer_size = 65536, nb_total_read = 0;
	char * buffer = malloc(buffer_size);
	struct so_value * index = NULL;
	for (;;) {
		nb_read = reader->ops->read(reader, buffer + nb_total_read, buffer_size - nb_total_read);
		if (nb_read < 0) {
			so_log_write(so_log_level_debug,
				dgettext("storiqone-drive-tape", "Error while reading LTFS index from media '%s'"),
				media->name);

			free(buffer);
			reader->ops->free(reader);

			return 2;
		}

		if (nb_read == 0) {
			so_log_write(so_log_level_debug,
				dgettext("storiqone-drive-tape", "Found corrupted LTFS index on media '%s'"),
				media->name);

			free(buffer);
			reader->ops->free(reader);

			return 3;
		}

		nb_total_read += nb_read;
		buffer[nb_total_read] = '\0';

		index = sodr_tape_drive_xml_parse_string(buffer);
		if (index != NULL)
			break;

		buffer_size += 65536;
		void * addr = realloc(buffer, buffer_size);
		if (addr == NULL) {
			so_log_write(so_log_level_debug,
				dgettext("storiqone-drive-tape", "Error, not enough memory to read LTFS index from media '%s'"),
				media->name);

			free(buffer);
			reader->ops->free(reader);

			return 3;
		}

		buffer = addr;
	}

	reader->ops->close(reader);
	reader->ops->free(reader);
	free(buffer);

	if (index != NULL)
		sodr_tape_drive_format_ltfs_parse_index(media->private_data, index);

	return 0;
}

int sodr_tape_drive_media_parse_ltfs_label(struct so_drive * drive, struct so_database_connection * db_connect) {
	struct so_media * media = drive->slot->media;

	struct so_stream_reader * reader = drive->ops->get_raw_reader(1, db_connect);
	if (reader == NULL) {
		so_log_write(so_log_level_debug,
			dgettext("storiqone-drive-tape", "Failed to read LTFS label from media '%s'"),
			media->name);
		return 1;
	}

	struct so_value * label = NULL;
	ssize_t buffer_size = 65536, nb_total_read = 0;
	char * buffer = malloc(buffer_size);
	for (;;) {
		ssize_t nb_read = reader->ops->read(reader, buffer + nb_total_read, buffer_size - nb_total_read);
		if (nb_read < 0) {
			so_log_write(so_log_level_debug,
				dgettext("storiqone-drive-tape", "Error while reading LTFS label from media '%s'"),
				media->name);

			free(buffer);
			reader->ops->free(reader);

			return 2;
		}

		if (nb_read == 0) {
			so_log_write(so_log_level_debug,
				dgettext("storiqone-drive-tape", "Found corrupted LTFS label on media '%s'"),
				media->name);

			free(buffer);
			reader->ops->free(reader);

			return 3;
		}

		nb_total_read += nb_read;
		buffer[nb_total_read] = '\0';

		label = sodr_tape_drive_xml_parse_string(buffer);
		if (label != NULL)
			break;

		buffer_size += 65536;
		void * addr = realloc(buffer, buffer_size);
		if (addr == NULL) {
			so_log_write(so_log_level_debug,
				dgettext("storiqone-drive-tape", "Error, not enough memory to read LTFS label from media '%s'"),
				media->name);

			free(buffer);
			reader->ops->free(reader);

			return 3;
		}

		buffer = addr;
	}

	reader->ops->close(reader);
	reader->ops->free(reader);
	free(buffer);

	struct so_value * children = NULL;
	so_value_unpack(label, "{so}", "children", &children);

	struct so_value_iterator * iter = so_value_list_get_iterator(children);
	while (so_value_iterator_has_next(iter)) {
		struct so_value * node = so_value_iterator_get_value(iter, false);

		char * name = NULL, * value = NULL;
		so_value_unpack(node, "{ssss}", "name", &name, "value", &value);

		if (name == NULL)
			continue;

		if (!strcmp(name, "formattime")) {
			media->first_used = sodr_tape_drive_format_ltfs_parse_time(value);
			media->use_before = media->first_used + media->media_format->life_span;
		} else if (!strcmp(name, "volumeuuid"))
			strncpy(media->uuid, value, 37);
		else if (!strcmp(name, "blocksize"))
			sscanf(value, "%zd", &media->block_size);

		free(name);
		free(value);
	}
	so_value_iterator_free(iter);

	return 0;
}

