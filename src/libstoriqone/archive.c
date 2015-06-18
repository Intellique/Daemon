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
// strdup
#include <string.h>
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

			so_value_list_push(files, so_value_pack("{sisis{sssssisssisssisssisisbsisosiss}}",
				"position", ptr_file->position,
				"archived time", ptr_file->archived_time,
				"file",
					"path", file->path,
					"restored to", file->restored_to,

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

	return so_value_pack("{sssssisosssssbsb}",
		"uuid", archive->uuid,
		"name", archive->name,

		"size", archive->size,

		"volumes", volumes,

		"creator", archive->creator,
		"owner", archive->owner,

		"can append", archive->can_append,
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
			free(file->path);
			free(file->restored_to);
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

void so_archive_free2(void * archive) {
	so_archive_free(archive);
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


struct so_archive_file * so_archive_file_copy(struct so_archive_file * file) {
	struct so_archive_file * copy = malloc(sizeof(struct so_archive_file));
	bzero(copy, sizeof(struct so_archive_file));

	copy->path = strdup(file->path);
	if (file->restored_to != NULL)
		copy->restored_to = strdup(file->restored_to);

	copy->perm = file->perm;
	copy->type = file->type;
	copy->ownerid = file->ownerid;
	copy->owner = strdup(file->owner);
	copy->groupid = file->groupid;
	copy->group = strdup(file->group);

	copy->create_time = file->create_time;
	copy->modify_time = file->modify_time;

	copy->check_ok = file->check_ok;
	copy->check_time = file->check_time;

	copy->size = file->size;

	copy->mime_type = strdup(file->mime_type);
	if (file->selected_path != NULL)
		copy->selected_path = strdup(file->selected_path);

	copy->digests = so_value_copy(file->digests, true);

	return copy;
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


struct so_value * so_archive_format_convert(struct so_archive_format * archive_format) {
	return so_value_pack("{sssbsb}",
		"name", archive_format->name,
		"readable", archive_format->readable,
		"writable", archive_format->writable
	);
}

void so_archive_format_free(struct so_archive_format * archive_format) {
	if (archive_format != NULL) {
		so_value_free(archive_format->db_data);
		free(archive_format);
	}
}

void so_archive_format_sync(struct so_archive_format * archive_format, struct so_value * new_archive_format) {
	char * name = NULL;
	so_value_unpack(new_archive_format, "{sssbsb}",
		"name", name,
		"readable", &archive_format->readable,
		"writable", &archive_format->writable
	);

	if (name == NULL)
		archive_format->name[0] = '\0';
	else
		strncpy(archive_format->name, name, 32);

	free(name);
}

