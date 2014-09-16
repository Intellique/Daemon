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
\****************************************************************************/

#define _GNU_SOURCE
// open
#include <fcntl.h>
// glob, globfree
#include <glob.h>
// asprintf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// strdup
#include <string.h>
// bzero
#include <strings.h>
// open, stat
#include <sys/stat.h>
// open, stat
#include <sys/types.h>
// access, close, stat, unlink
#include <unistd.h>
// time
#include <time.h>

#include <libstone/database.h>
#include <libstone/file.h>
#include <libstone/slot.h>
#include <libstone/value.h>
#include <libstone-drive/drive.h>
#include <libstone-drive/media.h>
#include <libstone-drive/time.h>

#include "device.h"
#include "io.h"

static bool vtl_drive_check_header(struct st_database_connection * db);
static bool vtl_drive_check_support(struct st_media_format * format, bool for_writing, struct st_database_connection * db);
static int vtl_drive_format_media(struct st_pool * pool, struct st_database_connection * db);
static ssize_t vtl_drive_find_best_block_size(struct st_database_connection * db);
static struct st_stream_reader * vtl_drive_get_raw_reader(int file_position, struct st_database_connection * db);
static struct st_stream_writer * vtl_drive_get_raw_writer(struct st_database_connection * db);
static int vtl_drive_init(struct st_value * config);
static int vtl_drive_reset(struct st_database_connection * db);
static int vtl_drive_update_status(struct st_database_connection * db);

static char * vtl_media_dir = NULL;
static struct st_media_format * vtl_media_format = NULL;

static struct st_drive_ops vtl_drive_ops = {
	.check_header         = vtl_drive_check_header,
	.check_support        = vtl_drive_check_support,
	.find_best_block_size = vtl_drive_find_best_block_size,
	.format_media         = vtl_drive_format_media,
	.get_raw_reader       = vtl_drive_get_raw_reader,
	.get_raw_writer       = vtl_drive_get_raw_writer,
	.init                 = vtl_drive_init,
	.reset                = vtl_drive_reset,
	.update_status        = vtl_drive_update_status,
};

static struct st_drive vtl_drive = {
	.status      = st_drive_status_empty_idle,
	.enable      = true,

	.density_code       = 0,
	.mode               = st_media_format_mode_linear,
	.operation_duration = 0,
	.is_empty           = true,

	.model         = "Stone vtl drive",
	.vendor        = "Intellique",
	.revision      = "A01",
	.serial_number = NULL,

	.changer = NULL,
	.index   = 0,
	.slot    = NULL,

	.ops = &vtl_drive_ops,

	.data    = NULL,
	.db_data = NULL,
};


static bool vtl_drive_check_header(struct st_database_connection * db) {
	vtl_drive.status = st_drive_status_reading;
	db->ops->sync_drive(db, &vtl_drive, st_database_sync_default);

	char * file;
	asprintf(&file, "%s/file_0", vtl_media_dir);

	char * header = st_file_read_all_from(file);
	free(file);

	struct st_media * media = vtl_drive.slot->media;
	media->last_read = time(NULL);
	media->read_count++;
	media->operation_count++;
	media->nb_total_read++;

	if (header == NULL)
		return true;

	bool ok = stdr_media_check_header(vtl_drive.slot->media, header, db);
	free(header);

	vtl_drive.status = st_drive_status_loaded_idle;
	db->ops->sync_drive(db, &vtl_drive, st_database_sync_default);

	return ok;
}

static bool vtl_drive_check_support(struct st_media_format * format, bool for_writing __attribute__((unused)), struct st_database_connection * db __attribute__((unused))) {
	return st_media_format_cmp(format, vtl_media_format) == 0;
}

static int vtl_drive_format_media(struct st_pool * pool, struct st_database_connection * db) {
	char * files;
	asprintf(&files, "%s/file_*", vtl_media_dir);

	vtl_drive.status = st_drive_status_writing;
	db->ops->sync_drive(db, &vtl_drive, st_database_sync_default);

	glob_t gl;
	int ret = glob(files, 0, NULL, &gl);
	if (ret != 0 && ret != GLOB_NOMATCH) {
		globfree(&gl);
		free(files);
		return 1;
	}

	unsigned int i;
	for (i = 0; i < gl.gl_pathc; i++)
		unlink(gl.gl_pathv[i]);

	globfree(&gl);
	free(files);

	ssize_t block_size = vtl_drive_find_best_block_size(db);
	char * header = malloc(block_size);

	struct st_media * media = vtl_drive.slot->media;
	media->status = st_media_status_new;
	media->operation_count++;
	media->block_size = block_size;
	media->total_block = media->format->capacity / block_size;
	media->free_block = media->total_block;
	media->nb_volumes = 0;
	media->append = true;

	if (!stdr_media_write_header(media, pool, header, block_size)) {
		free(header);
		return 1;
	}

	char * file;
	asprintf(&file, "%s/file_0", vtl_media_dir);

	stdr_time_start();
	int fd = open(file, O_CREAT | O_WRONLY, 0600);
	ssize_t nb_write = write(fd, header, block_size);
	close(fd);
	stdr_time_stop(&vtl_drive);

	media->write_count++;
	media->operation_count++;

	if (nb_write == block_size) {
		media->status = st_media_status_in_use;
		media->last_write = time(NULL);
		media->nb_total_write++;
	}

	vtl_drive.status = nb_write != block_size ? st_drive_status_error : st_drive_status_loaded_idle;
	db->ops->sync_drive(db, &vtl_drive, st_database_sync_default);

	free(file);

	return nb_write != block_size;
}

static ssize_t vtl_drive_find_best_block_size(struct st_database_connection * db __attribute__((unused))) {
	struct stat st;
	int failed = stat(vtl_media_dir, &st);
	if (failed != 0)
		return -1;

	return st.st_blksize;
}

struct st_drive * vtl_drive_get_device() {
	return &vtl_drive;
}

static struct st_stream_reader * vtl_drive_get_raw_reader(int file_position, struct st_database_connection * db) {}

static struct st_stream_writer * vtl_drive_get_raw_writer(struct st_database_connection * db) {
	char * files;
	asprintf(&files, "%s/file_*", vtl_media_dir);

	glob_t gl;
	int ret = glob(files, 0, NULL, &gl);
	if (ret != 0)
		return NULL;

	int nb_files = gl.gl_pathc;
	globfree(&gl);
	free(files);

	asprintf(&files, "%s/file_%d", vtl_media_dir, nb_files);

	struct st_stream_writer * writer = vtl_drive_writer_get_raw_writer(&vtl_drive, files);

	vtl_drive.status = st_drive_status_writing;
	db->ops->sync_drive(db, &vtl_drive, st_database_sync_default);

	free(files);
	return writer;
}

static int vtl_drive_init(struct st_value * config) {
	struct st_slot * sl = vtl_drive.slot = malloc(sizeof(struct st_slot));
	bzero(vtl_drive.slot, sizeof(struct st_slot));
	sl->drive = &vtl_drive;

	char * root_dir = NULL;
	struct st_value * format = NULL;
	st_value_unpack(config, "{ssssso}", "serial number", &vtl_drive.serial_number, "device", &root_dir, "format", &format);

	if (access(root_dir, R_OK | W_OK | X_OK) != 0)
		return 1;

	asprintf(&vtl_media_dir, "%s/media", root_dir);
	free(root_dir);

	vtl_media_format = malloc(sizeof(struct st_media_format));
	bzero(vtl_media_format, sizeof(struct st_media_format));
	st_media_format_sync(vtl_media_format, format);

	return 0;
}

static int vtl_drive_reset(struct st_database_connection * db) {
	struct st_slot * slot = vtl_drive.slot;
	struct st_media * media = slot->media;
	vtl_drive.slot->media = NULL;
	st_media_free(media);

	if (access(vtl_media_dir, F_OK) == 0) {
		vtl_drive.status = st_drive_status_loaded_idle;
		vtl_drive.is_empty = false;

		char * media_uuid = 0;
		asprintf(&media_uuid, "%s/serial_number", vtl_media_dir);

		char * uuid = st_file_read_all_from(media_uuid);
		free(media_uuid);

		media = vtl_drive.slot->media = db->ops->get_media(db, uuid, NULL, NULL);
		free(uuid);

		if (media == NULL)
			return 1;

		slot->volume_name = strdup(media->label);
		slot->full = true;

		bool ok = vtl_drive_check_header(db);
		if (!ok)
			return -1;
	} else {
		vtl_drive.status = st_drive_status_empty_idle;
		vtl_drive.is_empty = true;
		free(slot->volume_name);

		slot->volume_name = NULL;
		slot->full = false;
	}

	return 0;
}

static int vtl_drive_update_status(struct st_database_connection * db __attribute__((unused))) {
	return 0;
}

