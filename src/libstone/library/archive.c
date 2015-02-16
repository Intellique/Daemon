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
*  Last modified: Fri, 07 Feb 2014 10:23:34 +0100                            *
\****************************************************************************/

#define _XOPEN_SOURCE 500
// sscanf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// strcmp, strdup
#include <string.h>
// bzero
#include <strings.h>
// struct stat
#include <sys/stat.h>
// time
#include <time.h>
// uuid_generate, uuid_unparse_lower
#include <uuid/uuid.h>

#include <libstone/job.h>
#include <libstone/library/archive.h>
#include <libstone/library/media.h>
#include <libstone/user.h>
#include <libstone/util/file.h>
#include <libstone/util/hashtable.h>
#include <libstone/util/string.h>
#include <libstone/util/util.h>

static struct st_archive_file_type2 {
	char * name;
	enum st_archive_file_type type;
} st_archive_file_type[] = {
	{ "block device",     st_archive_file_type_block_device },
	{ "character device", st_archive_file_type_character_device },
	{ "directory",        st_archive_file_type_directory },
	{ "fifo",             st_archive_file_type_fifo },
	{ "regular file",     st_archive_file_type_regular_file },
	{ "socket",           st_archive_file_type_socket },
	{ "symbolic link",    st_archive_file_type_symbolic_link },
	{ "unknown",          st_archive_file_type_unknown },

	{ 0, st_archive_file_type_unknown },
};


struct st_archive_volume * st_archive_add_volume(struct st_archive * archive, struct st_media * media, long media_position, struct st_job * job) {
	archive->volumes = realloc(archive->volumes, (archive->nb_volumes + 1) * sizeof(struct st_archive_volume));
	struct st_archive_volume * volume = archive->volumes + archive->nb_volumes;
	unsigned int index_volume = archive->nb_volumes;
	archive->nb_volumes++;

	volume->sequence = index_volume;
	volume->size = 0;

	volume->start_time = time(NULL);
	volume->end_time = 0;

	volume->check_ok = false;
	volume->check_time = 0;

	volume->archive = archive;
	volume->media = media;
	volume->media_position = media_position;
	volume->job = job;

	volume->digests = NULL;

	volume->files = NULL;
	volume->nb_files = 0;

	volume->db_data = NULL;

	return volume;
}

void st_archive_free(struct st_archive * archive) {
	free(archive->name);

	unsigned int i;
	for (i = 0; i < archive->nb_volumes; i++)
		st_archive_volume_free(archive->volumes + i);
	free(archive->volumes);
	free(archive->metadatas);
	free(archive->db_data);

	free(archive);
}

struct st_archive * st_archive_new(const char * name, struct st_user * user) {
	uuid_t id;
	uuid_generate(id);

	struct st_archive * archive = malloc(sizeof(struct st_archive));
	bzero(archive, sizeof(struct st_archive));

	uuid_unparse_lower(id, archive->uuid);
	archive->name = strdup(name);

	archive->volumes = NULL;
	archive->nb_volumes = 0;

	archive->copy_of = NULL;
	archive->user = user;
	archive->metadatas = NULL;

	archive->db_data = NULL;

	return archive;
}

struct st_archive * st_archive_parse(json_t * jarchive, unsigned int position) {
	struct st_job_selected_path * path = malloc(sizeof(struct st_job_selected_path));
	path->path = strdup("/");
	path->db_data = NULL;

	struct st_archive * archive = malloc(sizeof(struct st_archive));
	bzero(archive, sizeof(struct st_archive));

	json_t * uuid = json_object_get(jarchive, "uuid");
	strncpy(archive->uuid, json_string_value(uuid), 36);
	archive->uuid[36] = '\0';

	json_t * name = json_object_get(jarchive, "name");
	archive->name = strdup(json_string_value(name));

	json_t * volumes = json_object_get(jarchive, "volumes");
	archive->nb_volumes = json_array_size(volumes);
	archive->volumes = calloc(archive->nb_volumes, sizeof(struct st_archive_volume));

	unsigned int i;
	bool ok = true;
	for (i = 0; i < archive->nb_volumes; i++) {
		struct st_archive_volume * vol = archive->volumes + i;
		json_t * jvol = json_array_get(volumes, i);

		json_unpack(jvol, "{sisi}",
			"sequence", &vol->sequence,
			"size", &vol->size,
			"nb files", &vol->nb_files
		);

		json_t * jtext = json_object_get(jvol, "created time");
		struct tm time;
		strptime(json_string_value(jtext), "%F %T", &time);
		vol->start_time = mktime(&time);

		jtext = json_object_get(jvol, "finish time");
		strptime(json_string_value(jtext), "%F %T", &time);
		vol->end_time = mktime(&time);

		vol->check_ok = false;
		vol->check_time = 0;
		vol->archive = archive;
		vol->job = NULL;

		json_t * jmedia = json_object_get(jvol, "media");
		name = json_object_get(jmedia, "medium serial number");

		vol->media = st_media_get_by_medium_serial_number(json_string_value(name));
		ok = vol->media != NULL;

		if (archive->nb_volumes == 1)
			vol->media_position = position;
		else
			ok = false;

		json_t * jdigests = json_object_get(jvol, "checksums");
		vol->digests = st_hashtable_new2(st_util_string_compute_hash, st_util_basic_free);

		const char * key;
		json_t * value;
		json_object_foreach(jdigests, key, value) {
			st_hashtable_put(vol->digests, strdup((char *) key), st_hashtable_val_string((char *) json_string_value(value)));
		}

		json_t * jfiles = json_object_get(jvol, "files");
		vol->nb_files = json_array_size(jfiles);

		unsigned int j;
		vol->files = calloc(vol->nb_files, sizeof(struct st_archive_files));
		for (j = 0; j < vol->nb_files; j++) {
			struct st_archive_files * files = vol->files + j;
			json_t * jfile = json_array_get(jfiles, j);

			json_unpack(jfile, "{si}", "block position", &files->position);

			struct st_archive_file * file = files->file = malloc(sizeof(struct st_archive_file));
			bzero(file, sizeof(struct st_archive_file));
			jfile = json_object_get(jfile, "file");

			jtext = json_object_get(jfile, "name");
			file->name = strdup(json_string_value(jtext));

			jtext = json_object_get(jfile, "permission");
			const char * text = json_string_value(jtext);
			sscanf(text, "%d", &file->perm);

			jtext = json_object_get(jfile, "type");
			file->type = st_archive_file_string_to_type(json_string_value(jtext));

			jtext = json_object_get(jfile, "owner");
			strncpy(file->owner, json_string_value(jtext), 32);

			jtext = json_object_get(jfile, "group");
			strncpy(file->group, json_string_value(jtext), 32);

			json_unpack(jfile, "{sisisi}",
				"owner id", &file->ownerid,
				"group id", &file->groupid,
				"size", &file->size
			);

			jtext = json_object_get(jfile, "created time");
			strptime(json_string_value(jtext), "%F %T", &time);
			file->create_time = mktime(&time);

			jtext = json_object_get(jfile, "modified time");
			strptime(json_string_value(jtext), "%F %T", &time);
			file->modify_time = mktime(&time);

			file->archived_time = 0;
			file->check_ok = false;
			file->check_time = 0;

			jtext = json_object_get(jfile, "mime type");
			file->mime_type = strdup(json_string_value(jtext));

			jdigests = json_object_get(jfile, "checksums");
			file->digests = st_hashtable_new2(st_util_string_compute_hash, st_util_basic_free);

			const char * key;
			json_t * value;
			json_object_foreach(jdigests, key, value) {
				st_hashtable_put(file->digests, strdup((char *) key), st_hashtable_val_string((char *) json_string_value(value)));
			}

			file->archive = archive;
			file->selected_path = path;
		}
	}

	name = json_object_get(jarchive, "user");
	archive->user = st_user_get(json_string_value(name));

	json_t * metas = json_object_get(jarchive, "metadatas");
	archive->metadatas = json_dumps(metas, JSON_COMPACT);

	if (ok)
		return archive;

	st_archive_free(archive);
	return NULL;
}

void st_archive_file_free(struct st_archive_file * file) {
	free(file->name);
	free(file->mime_type);
	st_hashtable_free(file->digests);
	free(file->db_data);
	free(file);
}

struct st_archive_file * st_archive_file_new(struct stat * file, const char * filename) {
	struct st_archive_file * f = malloc(sizeof(struct st_archive_file));
	f->name = strdup(filename);
	f->perm = file->st_mode & 07777;

	if (S_ISDIR(file->st_mode))
		f->type = st_archive_file_type_directory;
	else if (S_ISREG(file->st_mode))
		f->type = st_archive_file_type_regular_file;
	else if (S_ISLNK(file->st_mode))
		f->type = st_archive_file_type_symbolic_link;
	else if (S_ISCHR(file->st_mode))
		f->type = st_archive_file_type_character_device;
	else if (S_ISBLK(file->st_mode))
		f->type = st_archive_file_type_block_device;
	else if (S_ISFIFO(file->st_mode))
		f->type = st_archive_file_type_fifo;
	else if (S_ISSOCK(file->st_mode))
		f->type = st_archive_file_type_socket;

	f->ownerid = file->st_uid;
	st_util_file_uid2name(f->owner, 32, file->st_uid);
	f->groupid = file->st_gid;
	st_util_file_gid2name(f->group, 32, file->st_gid);
	f->create_time = file->st_ctime;
	f->modify_time = file->st_mtime;
	f->archived_time = time(NULL);
	f->check_ok = false;
	f->check_time = 0;
	f->size = file->st_size;

	f->mime_type = NULL;

	f->digests = NULL;

	f->archive = NULL;
	f->selected_path = NULL;

	f->db_data = NULL;

	return f;
}

enum st_archive_file_type st_archive_file_string_to_type(const char * type) {
	struct st_archive_file_type2 * ptr;
	for (ptr = st_archive_file_type; ptr->name; ptr++)
		if (!strcmp(ptr->name, type))
			return ptr->type;
	return ptr->type;
}

const char * st_archive_file_type_to_string(enum st_archive_file_type type) {
	struct st_archive_file_type2 * ptr;
	for (ptr = st_archive_file_type; ptr->name; ptr++)
		if (ptr->type == type)
			return ptr->name;
	return "unknown";
}

void st_archive_volume_free(struct st_archive_volume * volume) {
	st_hashtable_free(volume->digests);
	free(volume->files);
	free(volume->db_data);
}

