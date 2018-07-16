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

#define _GNU_SOURCE
// dgettext, gettext
#include <libintl.h>
// free, malloc
#include <stdlib.h>
// asprintf
#include <stdio.h>
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
#include <libstoriqone/format.h>
#include <libstoriqone/media.h>
#include <libstoriqone/string.h>
#include <libstoriqone/value.h>

static struct so_archive_status2 {
	unsigned long long hash;
	unsigned long long hash_translated;

	const char * name;
	const char * translation;
	const enum so_archive_status status;
} so_archive_status[] = {
	[so_archive_status_incomplete]    = { 0, 0, gettext_noop("imcomplete"),    NULL, so_archive_status_incomplete },
	[so_archive_status_data_complete] = { 0, 0, gettext_noop("data-complete"), NULL, so_archive_status_data_complete },
	[so_archive_status_complete]      = { 0, 0, gettext_noop("complete"),      NULL, so_archive_status_complete },
	[so_archive_status_error]         = { 0, 0, gettext_noop("error"),         NULL, so_archive_status_error },
};
static const unsigned int so_archive_nb_status = sizeof(so_archive_status) / sizeof(*so_archive_status);

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

static struct so_value * so_archive_file_convert(struct so_archive_files * file);
static void so_archive_file_free(struct so_archive_files * file);
static void so_archive_file_sync(struct so_archive_files * file, struct so_value * new_file);
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
	vol->min_version = vol->max_version = archive->current_version;

	archive->nb_volumes++;

	return vol;
}

struct so_value * so_archive_convert(struct so_archive * archive) {
	if (archive == NULL)
		return so_value_new_null();

	archive->size = 0;

	unsigned int i;
	struct so_value * volumes = so_value_new_array(archive->nb_volumes);
	for (i = 0; i < archive->nb_volumes; i++) {
		struct so_archive_volume * vol = archive->volumes + i;
		archive->size += vol->size;
		so_value_list_push(volumes, so_archive_volume_convert(vol), true);
	}

	return so_value_pack("{ssssszsosssssOsbsssbso}",
		"uuid", archive->uuid,
		"name", archive->name,

		"size", archive->size,

		"volumes", volumes,

		"creator", archive->creator,
		"owner", archive->owner,

		"metadata", archive->metadata,

		"can append", archive->can_append,
		"status", so_archive_status_to_string(archive->status, false),
		"deleted", archive->deleted,

		"pool", so_pool_convert(archive->pool)
	);
}

void so_archive_free(struct so_archive * archive) {
	if (archive == NULL)
		return;

	free(archive->name);

	unsigned int i;
	for (i = 0; i < archive->nb_volumes; i++)
		so_archive_volume_free(archive->volumes + i);
	free(archive->volumes);

	free(archive->creator);
	free(archive->owner);

	so_value_free(archive->metadata);

	so_pool_free(archive->pool);

	so_value_free(archive->db_data);
	free(archive);
}

void so_archive_free2(void * archive) {
	so_archive_free(archive);
}

static void so_archive_init() {
	unsigned int i;
	for (i = 0; i < so_archive_nb_status; i++) {
		so_archive_status[i].hash = so_string_compute_hash2(so_archive_status[i].name);
		so_archive_status[i].translation = dgettext("libstoriqone", so_archive_status[i].name);
		so_archive_status[i].hash_translated = so_string_compute_hash2(dgettext("libstoriqone", so_archive_status[i].name));
	}

	for (i = 0; i < so_archive_file_nb_data_types; i++) {
		so_archive_file_types[i].hash = so_string_compute_hash2(so_archive_file_types[i].name);
		so_archive_file_types[i].translation = dgettext("libstoriqone", so_archive_file_types[i].name);
		so_archive_file_types[i].hash_translated = so_string_compute_hash2(dgettext("libstoriqone", so_archive_file_types[i].name));
	}
}

struct so_archive * so_archive_new() {
	struct so_archive * archive = malloc(sizeof(struct so_archive));
	bzero(archive, sizeof(struct so_archive));
	archive->status = so_archive_status_incomplete;

	return archive;
}

const char * so_archive_status_to_string(enum so_archive_status status, bool translate) {
	if (translate)
		return so_archive_status[status].translation;
	else
		return so_archive_status[status].name;
}

enum so_archive_status so_archive_string_to_status(const char * status, bool translate) {
	if (status == NULL)
		return so_archive_status_error;

	unsigned int i;
	unsigned long long hash = so_string_compute_hash2(status);

	if (translate) {
		for (i = 0; i < so_archive_nb_status; i++)
			if (hash == so_archive_status[i].hash_translated)
				return so_archive_status[i].status;
	} else {
		for (i = 0; i < so_archive_nb_status; i++)
			if (hash == so_archive_status[i].hash)
				return so_archive_status[i].status;
	}

	return so_archive_status_error;
}

void so_archive_sync(struct so_archive * archive, struct so_value * new_archive) {
	free(archive->name);
	free(archive->creator);
	free(archive->owner);
	so_value_free(archive->metadata);

	struct so_value * uuid = NULL;
	struct so_value * volumes = NULL;
	archive->metadata = NULL;
	const char * archive_status = NULL;
	struct so_value * pool = NULL;

	so_value_unpack(new_archive, "{sossszsosssssOsbsSsbso}",
		"uuid", &uuid,
		"name", &archive->name,

		"size", &archive->size,

		"volumes", &volumes,

		"creator", &archive->creator,
		"owner", &archive->owner,

		"metadata", &archive->metadata,

		"can append", &archive->can_append,
		"status", &archive_status,
		"deleted", &archive->deleted,
		"pool", &pool
	);

	if (uuid->type == so_value_string) {
		const char * suuid = so_value_string_get(uuid);
		strncpy(archive->uuid, suuid, 36);
		archive->uuid[36] = '\0';
	} else
		archive->uuid[0] = '\0';

	archive->status = so_archive_string_to_status(archive_status, false);

	unsigned int nb_volumes = so_value_list_get_length(volumes);
	if (archive->nb_volumes != nb_volumes) {
		if (archive->nb_volumes == 0)
			archive->volumes = calloc(nb_volumes, sizeof(struct so_archive_volume));
		else if (archive->nb_volumes < nb_volumes) {
			void * new_volumes = realloc(archive->volumes, nb_volumes * sizeof(struct so_archive_volume));
			if (new_volumes != NULL) {
				archive->volumes = new_volumes;
				bzero(archive->volumes + archive->nb_volumes, (nb_volumes - archive->nb_volumes) * sizeof(struct so_archive_volume));
			}
		} else if (archive->nb_volumes > nb_volumes) {
			unsigned int i;
			for (i = nb_volumes; i < archive->nb_volumes; i++)
				so_archive_volume_free(archive->volumes + i);

			if (nb_volumes > 0) {
				void * new_volumes = realloc(archive->volumes, nb_volumes * sizeof(struct so_archive_volume));
				if (new_volumes != NULL) {
					archive->volumes = new_volumes;
					bzero(archive->volumes + archive->nb_volumes, (nb_volumes - archive->nb_volumes) * sizeof(struct so_archive_volume));
				}
			} else {
				free(volumes);
				archive->volumes = NULL;
			}
		}

		unsigned int i;
		for (i = archive->nb_volumes; i < nb_volumes; i++) {
			struct so_value * vvolume = so_value_list_get(volumes, i, false);
			struct so_archive_volume * vol = archive->volumes + i;

			vol->archive = archive;
			so_archive_volume_sync(vol, vvolume);
		}

		archive->nb_volumes = nb_volumes;
	}

	if (archive->pool != NULL && pool->type == so_value_hashtable)
		so_pool_sync(archive->pool, pool);
	else if (archive->pool == NULL && pool->type == so_value_hashtable) {
		archive->pool = malloc(sizeof(struct so_pool));
		bzero(archive->pool, sizeof(struct so_pool));
		so_pool_sync(archive->pool, pool);
	} else {
		so_pool_free(archive->pool);
		archive->pool = NULL;
	}
}


struct so_archive_file * so_archive_file_copy(struct so_archive_file * file) {
	struct so_archive_file * copy = malloc(sizeof(struct so_archive_file));
	bzero(copy, sizeof(struct so_archive_file));

	copy->path = strdup(file->path);
	if (file->restored_to != NULL)
		copy->restored_to = strdup(file->restored_to);
	copy->hash = strdup(file->hash);

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

struct so_archive_file * so_archive_file_import(struct so_format_file * file) {
	struct so_archive_file * new_file = malloc(sizeof(struct so_archive_file));
	bzero(new_file, sizeof(struct so_archive_file));

	new_file->path = strdup(file->filename);

	new_file->perm = file->mode & 07777;
	new_file->type = so_archive_file_mode_to_type(file->mode);
	new_file->ownerid = file->uid;
	new_file->owner = strdup(file->user);
	new_file->groupid = file->gid;
	new_file->group = strdup(file->group);

	new_file->create_time = file->ctime;
	new_file->modify_time = file->mtime;

	new_file->size = file->size;

	so_archive_file_update_hash(new_file);

	return new_file;
}


static struct so_value * so_archive_file_convert(struct so_archive_files * ptr_file) {
	struct so_archive_file * file = ptr_file->file;

	struct so_value * checksums;
	if (file->digests == NULL)
		checksums = so_value_new_hashtable2();
	else
		checksums = so_value_share(file->digests);

	return so_value_pack("{szsIs{sssssssssusssusssusssIsIsbsIsoszsisisOssss}}",
		"position", ptr_file->position,
		"archived time", (long long) ptr_file->archived_time,
		"file",
			"path", file->path,
			"alternate path", file->alternate_path,
			"restored to", file->restored_to,
			"hash", file->hash,

			"permission", file->perm,
			"type", so_archive_file_type_to_string(file->type, false),
			"owner id", file->ownerid,
			"owner", file->owner,
			"group id", file->groupid,
			"group", file->group,

			"create time", (long long) file->create_time,
			"modify time", (long long) file->modify_time,

			"checksum ok", file->check_ok,
			"checksum time", (long long) file->check_time,
			"checksums", checksums,

			"size", file->size,

			"min version", file->min_version,
			"max version", file->max_version,

			"metadata", file->metadata,
			"mime type", file->mime_type,
			"selected path", file->selected_path
	);
}

static void so_archive_file_free(struct so_archive_files * files) {
	struct so_archive_file * file = files->file;
	free(file->path);
	free(file->alternate_path);
	free(file->restored_to);
	free(file->hash);
	free(file->owner);
	free(file->group);
	so_value_free(file->metadata);
	free(file->mime_type);
	free(file->selected_path);
	so_value_free(file->digests);
	so_value_free(file->db_data);
	free(file);
}

static void so_archive_file_sync(struct so_archive_files * files, struct so_value * new_file) {
	long int archived_time = 0;
	long int permission = 0;
	const char * file_type = NULL;
	long int owner_id = 0;
	long int group_id = 0;
	long int create_time = 0;
	long int modify_time = 0;
	long int check_time = 0;
	int min_version = 0, max_version = 0;

	if (files->file == NULL) {
		files->file = malloc(sizeof(struct so_archive_file));
		bzero(files->file, sizeof(struct so_archive_file));
	} else {
		free(files->file->owner);
		free(files->file->group);
		so_value_free(files->file->digests);
		files->file->digests = NULL;
		so_value_free(files->file->metadata),
		free(files->file->mime_type);
		free(files->file->selected_path);
	}

	struct so_archive_file * file = files->file;

	so_value_unpack(new_file, "{szsis{sssssssssusSsusssusssisisbsisOszsisisOssss}}",
		"position", &files->position,
		"archived time", &archived_time,
		"file",
			"path", &file->path,
			"alternate path", &file->alternate_path,
			"restored to", &file->restored_to,
			"hash", &file->hash,

			"permission", &permission,
			"type", &file_type,
			"owner id", &owner_id,
			"owner", &file->owner,
			"group id", &group_id,
			"group", &file->group,

			"create time", &create_time,
			"modify time", &modify_time,

			"checksum ok", &file->check_ok,
			"checksum time", &check_time,
			"checksums", &file->digests,

			"size", &file->size,

			"min version", &min_version,
			"max version", &max_version,

			"metadata", &file->metadata,
			"mime type", &file->mime_type,
			"selected path", &file->selected_path
	);

	files->archived_time = archived_time;

	file->perm = permission;
	file->type = so_archive_file_string_to_type(file_type, false);
	file->ownerid = owner_id;
	file->groupid = group_id;
	file->create_time = create_time;
	file->modify_time = modify_time;
	file->check_time = check_time;
	file->min_version = min_version;
	file->max_version = max_version;
}

void so_archive_file_update_hash(struct so_archive_file * file) {
	if (file->type == so_archive_file_type_regular_file)
		asprintf(&file->hash, "%s_%ld_%zd", file->path, file->modify_time, file->size);
	else
		asprintf(&file->hash, "%s_%ld", file->path, file->modify_time);
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
	if (archive_format == NULL)
		return so_value_new_null();

	return so_value_pack("{sssbsb}",
		"name", archive_format->name,
		"readable", archive_format->readable,
		"writable", archive_format->writable
	);
}

struct so_archive_format * so_archive_format_dup(struct so_archive_format * archive_format) {
	struct so_archive_format * new_archive_format = malloc(sizeof(struct so_archive_format));
	bzero(new_archive_format, sizeof(struct so_archive_format));

	strncpy(new_archive_format->name, archive_format->name, 33);
	new_archive_format->readable = archive_format->readable;
	new_archive_format->writable = archive_format->writable;
	new_archive_format->db_data = NULL;

	return new_archive_format;
}

void so_archive_format_free(struct so_archive_format * archive_format) {
	if (archive_format != NULL) {
		so_value_free(archive_format->db_data);
		free(archive_format);
	}
}

void so_archive_format_sync(struct so_archive_format * archive_format, struct so_value * new_archive_format) {
	struct so_value * name = NULL;
	so_value_unpack(new_archive_format, "{sosbsb}",
		"name", &name,
		"readable", &archive_format->readable,
		"writable", &archive_format->writable
	);

	if (name->type == so_value_string)
		strncpy(archive_format->name, so_value_string_get(name), 32);
	else
		archive_format->name[0] = '\0';
}


struct so_value * so_archive_volume_convert(struct so_archive_volume * vol) {
	unsigned int i;
	struct so_value * files = so_value_new_array(vol->nb_files);
	for (i = 0; i < vol->nb_files; i++)
		so_value_list_push(files, so_archive_file_convert(vol->files + i), true);

	struct so_value * checksums;
	if (vol->digests == NULL)
		checksums = so_value_new_hashtable2();
	else
		checksums = so_value_share(vol->digests);

	return so_value_pack("{suszsIsIsbsIsososusisiso}",
		"sequence", vol->sequence,
		"size", vol->size,

		"start time", (long long) vol->start_time,
		"end time", (long long) vol->end_time,

		"checksum ok", vol->check_ok,
		"checksum time", (long long) vol->check_time,
		"checksums", checksums,

		"media", so_media_convert(vol->media),
		"media position", vol->media_position,

		"min version", vol->min_version,
		"max version", vol->max_version,

		"files", files
	);
}

void so_archive_volume_free(struct so_archive_volume * vol) {
	so_value_free(vol->digests);

	unsigned int i;
	for (i = 0; i < vol->nb_files; i++)
		so_archive_file_free(vol->files + i);
	free(vol->files);
	so_value_free(vol->db_data);
}

void so_archive_volume_sync(struct so_archive_volume * volume, struct so_value * new_volume) {
	struct so_value * media = NULL;
	struct so_value * files = NULL;

	if (volume->digests != NULL)
		so_value_free(volume->digests);
	volume->digests = NULL;

	long int sequence = 0;
	long int start_time = 0;
	long int end_time = 0;
	long int check_time = 0;
	long int media_position = 0;
	int min_version = 0, max_version = 0;

	so_value_unpack(new_volume, "{suszsisisbsisOsosusisiso}",
		"sequence", &sequence,
		"size", &volume->size,

		"start time", &start_time,
		"end time", &end_time,

		"checksum ok", &volume->check_ok,
		"checksum time", &check_time,
		"checksums", &volume->digests,

		"media", &media,
		"media position", &media_position,

		"min version", &min_version,
		"max version", &max_version,

		"files", &files
	);

	volume->sequence = sequence;
	volume->start_time = start_time;
	volume->end_time = end_time;
	volume->check_time = check_time;
	volume->min_version = min_version;
	volume->max_version = max_version;

	if (volume->media != NULL && media->type == so_value_null) {
		so_media_free(volume->media);
		volume->media = NULL;
	} else if (media->type != so_value_null) {
		if (volume->media == NULL) {
			volume->media = malloc(sizeof(struct so_media));
			bzero(volume->media, sizeof(struct so_media));
		}

		so_media_sync(volume->media, media);
	}
	volume->media_position = media_position;

	if (volume->files != NULL && files->type == so_value_null) {
		unsigned int i;
		for (i = 0; i < volume->nb_files; i++)
			so_archive_file_free(volume->files + i);
		free(volume->files);
		volume->files = NULL;
		volume->nb_files = 0;
	} else if (files->type != so_value_null) {
		unsigned int nb_files = so_value_list_get_length(files);
		if (volume->files == NULL)
			volume->files = calloc(nb_files, sizeof(struct so_archive_files));
		volume->nb_files = nb_files;

		unsigned int i;
		struct so_value_iterator * iter = so_value_list_get_iterator(files);
		for (i = 0; i < volume->nb_files && so_value_iterator_has_next(iter); i++) {
			struct so_value * vfile = so_value_iterator_get_value(iter, false);
			so_archive_file_sync(volume->files + i, vfile);
		}
		so_value_iterator_free(iter);
	}
}
