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
*  Copyright (C) 2014, Clercin guillaume <gclercin@intellique.com>           *
*  Last modified: Fri, 24 Jan 2014 13:14:42 +0100                            *
\****************************************************************************/

// json_array, json_array_append_new, json_decref, json_dumps, json_integer,
// json_object, json_object_set, json_object_set_new, json_string
#include <jansson.h>
// free
#include <stdlib.h>
// strlen
#include <string.h>
// localtime_r, strftime
#include <time.h>
// uname
#include <sys/utsname.h>
// close, read, write
#include <unistd.h>

#include <libstone/library/archive.h>
#include <libstone/library/media.h>
#include <libstone/user.h>
#include <libstone/util/hashtable.h>

#include "../io.h"

#include "stone.version"

static struct utsname st_io_json_uname;

static json_t * st_io_json_file(struct st_archive_file * file);
static json_t * st_io_json_media(struct st_media * media);
static json_t * st_io_json_volume(struct st_archive_volume * volume);
static void st_io_json_volume_init(void) __attribute__((constructor));


json_t * st_io_json_archive(struct st_archive * archive) {
	json_t * jarchive = json_object();

	json_object_set_new(jarchive, "uuid", json_string(archive->uuid));
	json_object_set_new(jarchive, "name", json_string(archive->name));
	json_object_set_new(jarchive, "user", json_string(archive->user->login));

	json_t * volumes = json_array();
	unsigned int i;
	for (i = 0; i < archive->nb_volumes; i++)
		json_array_append_new(volumes, st_io_json_volume(archive->volumes + i));
	json_object_set_new(jarchive, "volumes", volumes);

	json_error_t error;
	json_t * meta = json_loads(archive->metadatas, JSON_REJECT_DUPLICATES | JSON_DECODE_ANY, &error);

	if (meta != NULL)
		json_object_set_new(jarchive, "metadatas", meta);

	return jarchive;
}

static json_t * st_io_json_file(struct st_archive_file * file) {
	json_t * jfile = json_object();

	json_object_set_new(jfile, "name", json_string(file->name));

	char perm[5];
	snprintf(perm, 5, "%03o", file->perm & 0x7777);
	json_object_set_new(jfile, "permission", json_string(perm));
	json_object_set_new(jfile, "type", json_string(st_archive_file_type_to_string(file->type)));
	json_object_set_new(jfile, "owner id", json_integer(file->ownerid));
	json_object_set_new(jfile, "owner", json_string(file->owner));
	json_object_set_new(jfile, "group id", json_integer(file->groupid));
	json_object_set_new(jfile, "group", json_string(file->group));

	char ctime[32];
	struct tm local_current;
	localtime_r(&file->create_time, &local_current);
	strftime(ctime, 32, "%F %T", &local_current);
	json_object_set_new(jfile, "created time", json_string(ctime));

	localtime_r(&file->modify_time, &local_current);
	strftime(ctime, 32, "%F %T", &local_current);
	json_object_set_new(jfile, "modified time", json_string(ctime));

	json_object_set_new(jfile, "size", json_integer(file->size));
	json_object_set_new(jfile, "mime type", json_string(file->mime_type));

	json_t * jchecksums = json_object();
	if (file->digests != NULL) {
		unsigned int i, nb_digests;
		const void ** checksums = st_hashtable_keys(file->digests, &nb_digests);
		for (i = 0; i < nb_digests; i++) {
			struct st_hashtable_value digest = st_hashtable_get(file->digests, checksums[i]);
			json_object_set_new(jchecksums, checksums[i], json_string(digest.value.string));
		}
		free(checksums);
	}
	json_object_set_new(jfile, "checksums", jchecksums);

	return jfile;
}

static json_t * st_io_json_media(struct st_media * media) {
	json_t * jmedia = json_object();

	json_object_set_new(jmedia, "uuid", json_string(media->uuid));
	json_object_set_new(jmedia, "label", json_string(media->label));
	json_object_set_new(jmedia, "medium serial number", json_string(media->medium_serial_number));
	json_object_set_new(jmedia, "name", json_string(media->name));

	json_object_set_new(jmedia, "block size", json_integer(media->block_size));

	return jmedia;
}

json_t * st_io_json_read_from(int fd, bool need_close) {
	ssize_t buffer_size = 4096, nb_total_read = 0, size;
	char * buffer = malloc(buffer_size + 1);
	buffer[0] = '\0';

	while (size = read(fd, buffer + nb_total_read, buffer_size - nb_total_read), size > 0) {
		nb_total_read += size;
		buffer[nb_total_read] = '\0';

		buffer_size += 4096;
		void * addr = realloc(buffer, buffer_size + 1);
		if (addr == NULL)
			break;

		buffer = addr;
	}

	if (need_close)
		close(fd);

	json_error_t error;
	json_t * value = json_loadb(buffer, nb_total_read, 0, &error);
	free(buffer);

	return value;
}

static json_t * st_io_json_volume(struct st_archive_volume * volume) {
	json_t * jvolume = json_object();

	json_object_set_new(jvolume, "host", json_string(st_io_json_uname.nodename));
	json_object_set_new(jvolume, "media", st_io_json_media(volume->media));
	json_object_set_new(jvolume, "sequence", json_integer(volume->sequence));
	json_object_set_new(jvolume, "size", json_integer(volume->size));

	char ctime[32];
	struct tm local_current;
	localtime_r(&volume->start_time, &local_current);
	strftime(ctime, 32, "%F %T", &local_current);
	json_object_set_new(jvolume, "created time", json_string(ctime));

	localtime_r(&volume->end_time, &local_current);
	strftime(ctime, 32, "%F %T", &local_current);
	json_object_set_new(jvolume, "finish time", json_string(ctime));

	json_t * jchecksums = json_object();
	unsigned int i;
	if (volume->digests != NULL) {
		unsigned int nb_digests;
		const void ** checksums = st_hashtable_keys(volume->digests, &nb_digests);
		for (i = 0; i < nb_digests; i++) {
			struct st_hashtable_value digest = st_hashtable_get(volume->digests, checksums[i]);
			json_object_set_new(jchecksums, checksums[i], json_string(digest.value.string));
		}
		json_object_set_new(jvolume, "checksums", jchecksums);
		free(checksums);
	}

	json_t * files = json_array();
	for (i = 0; i < volume->nb_files; i++) {
		struct st_archive_files * file = volume->files + i;

		json_t * jblock = json_object();
		json_object_set_new(jblock, "block position", json_integer(file->position));
		json_object_set_new(jblock, "file", st_io_json_file(file->file));

		json_array_append_new(files, jblock);
	}

	json_object_set_new(jvolume, "files", files);
	json_object_set_new(jvolume, "nb files", json_integer(volume->nb_files));

	return jvolume;
}

void st_io_json_write_to(int fd, json_t * data, bool need_close) {
	if (data == NULL)
		return;

	char * string = json_dumps(data, JSON_COMPACT);
	write(fd, string, strlen(string));
	free(string);

	if (need_close)
		close(fd);
}

ssize_t st_io_json_writer(struct st_stream_writer * writer, struct st_archive * archive) {
	json_t * stone = json_object();
	json_object_set_new(stone, "version", json_string(STONE_VERSION));
	json_object_set_new(stone, "build", json_string(__DATE__ " " __TIME__));

	json_t * root = json_object();
	json_object_set_new(root, "stone", stone);
	json_object_set_new(root, "archive", st_io_json_archive(archive));

	char * cjson = json_dumps(root, JSON_COMPACT | JSON_SORT_KEYS);
	ssize_t cjson_length = strlen(cjson);

	ssize_t nb_write = writer->ops->write(writer, cjson, cjson_length);
	writer->ops->close(writer);

	free(cjson);
	json_decref(root);

	return nb_write;
}

static void st_io_json_volume_init() {
	uname(&st_io_json_uname);
}

