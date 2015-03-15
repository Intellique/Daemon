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

// dgettext, gettext
#include <libintl.h>
// free, malloc
#include <stdlib.h>
// bzero
#include <strings.h>
// S_*
#include <sys/stat.h>
// time
#include <time.h>

#define gettext_noop(String) String

#include <libstoriqone/archive.h>
#include <libstoriqone/media.h>
#include <libstoriqone/string.h>
#include <libstoriqone/value.h>

static struct so_archive_file_type2 {
	unsigned long long hash;
	unsigned long long hash_translated;

	const char * name;
	const char * translation;
	const enum so_archive_file_type type;
} so_archive_file_types[] = {
	[so_archive_file_type_block_device]     = { 0, 0, gettext_noop("block device"),     NULL, so_archive_file_type_block_device },
	[so_archive_file_type_character_device] = { 0, 0, gettext_noop("character device"), NULL, so_archive_file_type_character_device },
	[so_archive_file_type_directory]        = { 0, 0, gettext_noop("directory"),        NULL, so_archive_file_type_directory },
	[so_archive_file_type_fifo]             = { 0, 0, gettext_noop("fifo"),             NULL, so_archive_file_type_fifo },
	[so_archive_file_type_regular_file]     = { 0, 0, gettext_noop("regular file"),     NULL, so_archive_file_type_regular_file },
	[so_archive_file_type_socket]           = { 0, 0, gettext_noop("socket"),           NULL, so_archive_file_type_socket },
	[so_archive_file_type_symbolic_link]    = { 0, 0, gettext_noop("symbolic link"),    NULL, so_archive_file_type_symbolic_link },

	[so_archive_file_type_unknown] = { 0, 0, gettext_noop("unknown file type"), NULL, so_archive_file_type_unknown },
};
static const unsigned int so_archive_file_nb_data_types = sizeof(so_archive_file_types) / sizeof(*so_archive_file_types);

static void so_archive_init(void) __attribute__((constructor));


struct so_archive_volume * so_archive_add_volume(struct so_archive * archive) {
	void * new_addr = realloc(archive->volumes, (archive->nb_volumes + 1) * sizeof(struct so_archive_volume));
	if (new_addr == NULL)
		return NULL;

	archive->volumes = new_addr;
	struct so_archive_volume * vol = archive->volumes + archive->nb_volumes;

	bzero(vol, sizeof(struct so_archive_volume));
	vol->sequence = archive->nb_volumes;
	vol->start_time = time(NULL);
	vol->archive = archive;

	archive->nb_volumes++;

	return vol;
}

struct so_value * so_archive_convert(struct so_archive * archive) {
	archive->size = 0;

	unsigned int i;
	struct so_value * volumes = so_value_new_array(archive->nb_volumes);
	for (i = 0; i < archive->nb_volumes; i++) {
		struct so_archive_volume * vol = archive->volumes + i;

		archive->size += vol->size;

		unsigned int j;
		struct so_value * files = so_value_new_array(vol->nb_files);
		for (j = 0; j < vol->nb_files; j++) {
			struct so_archive_files * ptr_file = vol->files + j;
			struct so_archive_file * file = ptr_file->file;

			struct so_value * checksums;
			if (file->digests == NULL)
				checksums = so_value_new_hashtable2();
			else
				checksums = so_value_share(file->digests);

			so_value_list_push(files, so_value_pack("{sisis{sssisssisssisssisisbsisosiss}}",
				"position", ptr_file->position,
				"archived time", ptr_file->archived_time,
				"file",
					"name", file->name,
					"permission", (long) file->perm,
					"type", so_archive_file_type_to_string(file->type, false),
					"owner id", file->ownerid,
					"owner", file->owner,
					"group id", file->groupid,
					"group", file->group,

					"create time", file->create_time,
					"modify time", file->modify_time,

					"checksum ok", file->check_ok,
					"checksum time", file->check_time,
					"checksums", checksums,

					"size", file->size,

					"mime type", file->mime_type
			), true);
		}

		struct so_value * checksums;
		if (vol->digests == NULL)
			checksums = so_value_new_hashtable2();
		else
			checksums = so_value_share(vol->digests);

		so_value_list_push(volumes, so_value_pack("{sisisisisbsisososiso}",
			"sequence", (long) vol->sequence,
			"size", vol->size,

			"start time", vol->start_time,
			"end time", vol->end_time,

			"checksum ok", vol->check_ok,
			"checksum time", vol->check_time,
			"checksums", checksums,

			"media", so_media_convert(vol->media),
			"media position", (long) vol->media_position,

			"files", files
		), true);
	}

	return so_value_pack("{sssssisosssssb}",
		"uuid", archive->uuid,
		"name", archive->name,

		"size", archive->size,

		"volumes", volumes,

		"creator", archive->creator,
		"owner", archive->owner,

		"deleted", archive->deleted
	);
}

void so_archive_free(struct so_archive * archive) {
	if (archive == NULL)
		return;

	free(archive->name);

	unsigned int i;
	for (i = 0; i < archive->nb_volumes; i++) {
		struct so_archive_volume * vol = archive->volumes + i;
		so_value_free(vol->digests);

		unsigned int j;
		for (j = 0; j < vol->nb_files; j++) {
			struct so_archive_file * file = vol->files[j].file;
			free(file->name);
			free(file->owner);
			free(file->group);
			free(file->mime_type);
			free(file->selected_path);
			so_value_free(file->digests);
			so_value_free(file->db_data);
			free(file);
		}
		free(vol->files);
		so_value_free(vol->db_data);
	}
	free(archive->volumes);

	free(archive->creator);
	free(archive->owner);

	so_value_free(archive->db_data);
	free(archive);
}

static void so_archive_init() {
	unsigned int i;
	for (i = 0; i < so_archive_file_nb_data_types; i++) {
		so_archive_file_types[i].hash = so_string_compute_hash2(so_archive_file_types[i].name);
		so_archive_file_types[i].translation = dgettext("libstoriqone", so_archive_file_types[i].name);
		so_archive_file_types[i].hash_translated = so_string_compute_hash2(dgettext("libstoriqone", so_archive_file_types[i].name));
	}
}

struct so_archive * so_archive_new() {
	struct so_archive * archive = malloc(sizeof(struct so_archive));
	bzero(archive, sizeof(struct so_archive));

	return archive;
}


enum so_archive_file_type so_archive_file_mode_to_type(mode_t mode) {
	if (S_ISBLK(mode))
		return so_archive_file_type_block_device;
	if (S_ISCHR(mode))
		return so_archive_file_type_character_device;
	if (S_ISDIR(mode))
		return so_archive_file_type_directory;
	if (S_ISFIFO(mode))
		return so_archive_file_type_fifo;
	if (S_ISREG(mode))
		return so_archive_file_type_regular_file;
	if (S_ISSOCK(mode))
		return so_archive_file_type_socket;
	if (S_ISLNK(mode))
		return so_archive_file_type_symbolic_link;

	return so_archive_file_type_unknown;
}

enum so_archive_file_type so_archive_file_string_to_type(const char * type, bool translate) {
	if (type == NULL)
		return so_archive_file_type_unknown;

	unsigned int i;
	unsigned long long hash = so_string_compute_hash2(type);

	if (translate) {
		for (i = 0; i < so_archive_file_nb_data_types; i++)
			if (hash == so_archive_file_types[i].hash_translated)
				return so_archive_file_types[i].type;
	} else {
		for (i = 0; i < so_archive_file_nb_data_types; i++)
			if (hash == so_archive_file_types[i].hash)
				return so_archive_file_types[i].type;
	}

	return so_archive_file_type_unknown;
}

const char * so_archive_file_type_to_string(enum so_archive_file_type type, bool translate) {
	if (translate)
		return so_archive_file_types[type].translation;
	else
		return so_archive_file_types[type].name;
}

