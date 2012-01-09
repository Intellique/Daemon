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
*  Copyright (C) 2011, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Mon, 09 Jan 2012 21:28:09 +0100                         *
\*************************************************************************/

#include <jansson.h>
// malloc
#include <stdlib.h>
// strlen
#include <string.h>
// strptime
#include <time.h>

#include <stone/io/json.h>
#include <stone/job.h>
#include <stone/library/archive.h>
#include <stone/library/tape.h>
#include <stone/user.h>

#include "config.h"

struct st_io_json {
	json_t * root;

	json_t * archive;
	json_t * archive_volumes;

	json_t * current_volume;
	json_t * current_volume_files;

	json_t * current_file;
};


void st_io_json_add_file(struct st_io_json * js, struct st_archive_file * file) {
	if (!js || !file)
		return;

	js->current_file = json_object();
	json_object_set_new(js->current_file, "name", json_string(file->name));

	char perm[4];
	snprintf(perm, 4, "%3o", file->perm & 0x777);
	json_object_set_new(js->current_file, "perm", json_string_nocheck(perm));
	json_object_set_new(js->current_file, "type", json_string_nocheck(st_archive_file_type_to_string(file->type)));
	json_object_set_new(js->current_file, "owner id", json_integer(file->ownerid));
	json_object_set_new(js->current_file, "owner", json_string(file->owner));
	json_object_set_new(js->current_file, "group id", json_integer(file->groupid));
	json_object_set_new(js->current_file, "group", json_string(file->group));

	char ctime[32], mtime[32];
	struct tm local_current;
	localtime_r(&file->ctime, &local_current);
	strftime(ctime, 32, "%F %T", &local_current);
	localtime_r(&file->mtime, &local_current);
	strftime(mtime, 32, "%F %T", &local_current);
	json_object_set_new(js->current_file, "created time", json_string(ctime));
	json_object_set_new(js->current_file, "modified time", json_string(mtime));

	json_object_set_new(js->current_file, "size", json_integer(file->size));

	json_t * checksums = json_array();
	// checksum
	unsigned int i;
	for (i = 0; i < file->nb_checksums; i++) {
		json_t * checksum = json_object();
		json_object_set_new(checksum, "name", json_string(file->archive->job->checksums[i]));
		json_object_set_new(checksum, "value", json_string(file->digests[i]));
		json_array_append_new(checksums, checksum);
	}
	json_object_set_new(js->current_file, "checksums", checksums);

	json_array_append(js->current_volume_files, js->current_file);
}

void st_io_json_add_volume(struct st_io_json * js, struct st_archive_volume * volume) {
	if (!js || !volume)
		return;

	js->current_volume = json_object();
	json_array_append_new(js->archive_volumes, js->current_volume);

	json_object_set_new(js->current_volume, "sequence", json_integer(volume->sequence));
	json_object_set_new(js->current_volume, "tape id", json_string(volume->tape->uuid));
	json_object_set_new(js->current_volume, "tape position", json_integer(volume->tape_position));

	char ctime[32];
	struct tm local_current;
	localtime_r(&volume->ctime, &local_current);
	strftime(ctime, 32, "%F %T", &local_current);
	json_object_set_new(js->current_volume, "create time", json_string(ctime));

	js->current_volume_files = json_array();

	if (js->current_file)
		json_array_append(js->current_volume, js->current_file);
}

void st_io_json_free(struct st_io_json * js) {
	if (!js)
		return;

	json_decref(js->current_file);
	json_decref(js->root);

	free(js);
}

struct st_io_json * st_io_json_new(struct st_archive * archive) {
	struct st_io_json * js = malloc(sizeof(struct st_io_json));
	js->root = json_object();

	json_t * stone = json_object();
	json_object_set_new(stone, "version", json_string(STONE_VERSION));
	json_object_set_new(stone, "build", json_string(__DATE__ " " __TIME__));
	json_object_set_new(js->root, "STone", stone);

	js->archive = json_object();
	json_object_set_new(js->root, "archive", js->archive);

	js->archive_volumes = json_array();

	json_t * name = json_string(archive->name);
	json_object_set_new(js->archive, "name", name);
	json_object_set_new(js->archive, "user", json_string(archive->user->login));

	char ctime[32];
	struct tm local_current;
	localtime_r(&archive->ctime, &local_current);
	strftime(ctime, 32, "%F %T", &local_current);
	json_object_set_new(js->archive, "create time", json_string(ctime));

	js->current_volume = 0;
	js->current_file = 0;

	return js;
}

void st_io_json_update_archive(struct st_io_json * js, struct st_archive * archive) {
	if (!js || !archive)
		return;

	char endtime[32];
	struct tm local_current;
	localtime_r(&archive->endtime, &local_current);
	strftime(endtime, 32, "%F %T", &local_current);
	json_object_set_new(js->archive, "finish time", json_string(endtime));
	json_object_set_new(js->archive, "volumes", js->archive_volumes);
}

void st_io_json_update_volume(struct st_io_json * js, struct st_archive_volume * volume) {
	if (!js || !volume)
		return;

	char endtime[32];
	struct tm local_current;
	localtime_r(&volume->endtime, &local_current);
	strftime(endtime, 32, "%F %T", &local_current);
	json_object_set_new(js->current_volume, "finish time", json_string(endtime));
	json_object_set_new(js->current_volume, "size", json_integer(volume->size));

	json_t * checksums = json_array();
	// checksum
	unsigned int i;
	for (i = 0; i < volume->nb_checksums; i++) {
		json_t * checksum = json_object();
		json_object_set_new(checksum, "name", json_string(volume->archive->job->checksums[i]));
		json_object_set_new(checksum, "value", json_string(volume->digests[i]));
		json_array_append_new(checksums, checksum);
	}
	json_object_set_new(js->current_volume, "checksums", checksums);

	json_object_set_new(js->current_volume, "files", js->current_volume_files);
}

ssize_t st_io_json_write_to(struct st_io_json * js, struct st_stream_writer * writer) {
	if (!js || !writer)
		return -1;

	char * buffer = json_dumps(js->root, JSON_INDENT(2));
	ssize_t nb_write = writer->ops->write(writer, buffer, strlen(buffer));

	free(buffer);

	return nb_write;
}

