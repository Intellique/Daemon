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

#include <libstoriqone/database.h>
#include <libstoriqone/file.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/value.h>
#include <libstoriqone-drive/drive.h>
#include <libstoriqone-drive/media.h>
#include <libstoriqone-drive/time.h>

#include "device.h"
#include "io.h"

static bool sodr_vtl_drive_check_header(struct so_database_connection * db);
static bool sodr_vtl_drive_check_support(struct so_media_format * format, bool for_writing, struct so_database_connection * db);
static int sodr_vtl_drive_format_media(struct so_pool * pool, struct so_database_connection * db);
static ssize_t sodr_vtl_drive_find_best_block_size(struct so_database_connection * db);
static struct so_stream_reader * sodr_vtl_drive_get_raw_reader(int file_position, struct so_database_connection * db);
static struct so_stream_writer * sodr_vtl_drive_get_raw_writer(struct so_database_connection * db);
static int sodr_vtl_drive_init(struct so_value * config);
static int sodr_vtl_drive_reset(struct so_database_connection * db);
static int sodr_vtl_drive_update_status(struct so_database_connection * db);

static char * sodr_vtl_media_dir = NULL;
static struct so_media_format * sodr_vtl_media_format = NULL;

static struct so_drive_ops sodr_vtl_drive_ops = {
	.check_header         = sodr_vtl_drive_check_header,
	.check_support        = sodr_vtl_drive_check_support,
	.find_best_block_size = sodr_vtl_drive_find_best_block_size,
	.format_media         = sodr_vtl_drive_format_media,
	.get_raw_reader       = sodr_vtl_drive_get_raw_reader,
	.get_raw_writer       = sodr_vtl_drive_get_raw_writer,
	.init                 = sodr_vtl_drive_init,
	.reset                = sodr_vtl_drive_reset,
	.update_status        = sodr_vtl_drive_update_status,
};

static struct so_drive sodr_vtl_drive = {
	.status      = so_drive_status_empty_idle,
	.enable      = true,

	.density_code       = 0,
	.mode               = so_media_format_mode_linear,
	.operation_duration = 0,
	.is_empty           = true,

	.model         = "Stone vtl drive",
	.vendor        = "Intellique",
	.revision      = "A01",
	.serial_number = NULL,

	.changer = NULL,
	.index   = 0,
	.slot    = NULL,

	.ops = &sodr_vtl_drive_ops,

	.data    = NULL,
	.db_data = NULL,
};


static bool sodr_vtl_drive_check_header(struct so_database_connection * db) {
	sodr_vtl_drive.status = so_drive_status_reading;
	db->ops->sync_drive(db, &sodr_vtl_drive, true, so_database_sync_default);

	char * file;
	asprintf(&file, "%s/file_0", sodr_vtl_media_dir);

	char * header = so_file_read_all_from(file);
	free(file);

	struct so_media * media = sodr_vtl_drive.slot->media;
	media->last_read = time(NULL);
	media->read_count++;
	media->operation_count++;
	media->nb_total_read++;

	if (header == NULL)
		return true;

	bool ok = sodr_media_check_header(sodr_vtl_drive.slot->media, header, db);
	free(header);

	sodr_vtl_drive.status = so_drive_status_loaded_idle;
	db->ops->sync_drive(db, &sodr_vtl_drive, true, so_database_sync_default);

	return ok;
}

static bool sodr_vtl_drive_check_support(struct so_media_format * format, bool for_writing __attribute__((unused)), struct so_database_connection * db __attribute__((unused))) {
	return so_media_format_cmp(format, sodr_vtl_media_format) == 0;
}

static int sodr_vtl_drive_format_media(struct so_pool * pool, struct so_database_connection * db) {
	char * files;
	asprintf(&files, "%s/file_*", sodr_vtl_media_dir);

	sodr_vtl_drive.status = so_drive_status_writing;
	db->ops->sync_drive(db, &sodr_vtl_drive, true, so_database_sync_default);

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

	ssize_t block_size = sodr_vtl_drive_find_best_block_size(db);
	char * header = malloc(block_size);

	struct so_media * media = sodr_vtl_drive.slot->media;
	media->status = so_media_status_new;
	media->operation_count++;
	media->block_size = block_size;
	media->total_block = media->format->capacity / block_size;
	media->free_block = media->total_block;
	media->nb_volumes = 0;
	media->append = true;

	if (!sodr_media_write_header(media, pool, header, block_size)) {
		free(header);
		return 1;
	}

	char * file;
	asprintf(&file, "%s/file_0", sodr_vtl_media_dir);

	sodr_time_start();
	int fd = open(file, O_CREAT | O_WRONLY, 0600);
	ssize_t nb_write = write(fd, header, block_size);
	close(fd);
	sodr_time_stop(&sodr_vtl_drive);

	media->write_count++;
	media->operation_count++;

	if (nb_write == block_size) {
		media->status = so_media_status_in_use;
		media->last_write = time(NULL);
		media->nb_total_write++;
		media->nb_volumes = 1;
		media->free_block--;
	}

	sodr_vtl_drive.status = nb_write != block_size ? so_drive_status_error : so_drive_status_loaded_idle;
	db->ops->sync_drive(db, &sodr_vtl_drive, true, so_database_sync_default);

	free(file);

	return nb_write != block_size;
}

static ssize_t sodr_vtl_drive_find_best_block_size(struct so_database_connection * db __attribute__((unused))) {
	struct stat st;
	int failed = stat(sodr_vtl_media_dir, &st);
	if (failed != 0)
		return -1;

	return st.st_blksize;
}

struct so_drive * sodr_vtl_drive_get_device() {
	return &sodr_vtl_drive;
}

static struct so_stream_reader * sodr_vtl_drive_get_raw_reader(int file_position, struct so_database_connection * db) {}

static struct so_stream_writer * sodr_vtl_drive_get_raw_writer(struct so_database_connection * db) {
	char * files;
	asprintf(&files, "%s/file_*", sodr_vtl_media_dir);

	glob_t gl;
	int ret = glob(files, 0, NULL, &gl);
	if (ret != 0)
		return NULL;

	int nb_files = gl.gl_pathc;
	globfree(&gl);
	free(files);

	asprintf(&files, "%s/file_%d", sodr_vtl_media_dir, nb_files);

	struct so_stream_writer * writer = sodr_vtl_drive_writer_get_raw_writer(&sodr_vtl_drive, files);

	sodr_vtl_drive.status = so_drive_status_writing;
	db->ops->sync_drive(db, &sodr_vtl_drive, true, so_database_sync_default);

	free(files);
	return writer;
}

static int sodr_vtl_drive_init(struct so_value * config) {
	struct so_slot * sl = sodr_vtl_drive.slot = malloc(sizeof(struct so_slot));
	bzero(sodr_vtl_drive.slot, sizeof(struct so_slot));
	sl->drive = &sodr_vtl_drive;

	char * root_dir = NULL;
	struct so_value * format = NULL;
	so_value_unpack(config, "{ssssso}", "serial number", &sodr_vtl_drive.serial_number, "device", &root_dir, "format", &format);

	if (access(root_dir, R_OK | W_OK | X_OK) != 0)
		return 1;

	asprintf(&sodr_vtl_media_dir, "%s/media", root_dir);
	free(root_dir);

	sodr_vtl_media_format = malloc(sizeof(struct so_media_format));
	bzero(sodr_vtl_media_format, sizeof(struct so_media_format));
	so_media_format_sync(sodr_vtl_media_format, format);

	return 0;
}

static int sodr_vtl_drive_reset(struct so_database_connection * db) {
	struct so_slot * slot = sodr_vtl_drive.slot;
	struct so_media * media = slot->media;
	sodr_vtl_drive.slot->media = NULL;
	so_media_free(media);

	if (access(sodr_vtl_media_dir, F_OK) == 0) {
		sodr_vtl_drive.status = so_drive_status_loaded_idle;
		sodr_vtl_drive.is_empty = false;

		char * media_uuid = 0;
		asprintf(&media_uuid, "%s/serial_number", sodr_vtl_media_dir);

		char * uuid = so_file_read_all_from(media_uuid);
		free(media_uuid);

		media = sodr_vtl_drive.slot->media = db->ops->get_media(db, uuid, NULL, NULL);
		free(uuid);

		if (media == NULL)
			return 1;

		slot->volume_name = strdup(media->label);
		slot->full = true;

		bool ok = sodr_vtl_drive_check_header(db);
		if (!ok)
			return -1;
	} else {
		sodr_vtl_drive.status = so_drive_status_empty_idle;
		sodr_vtl_drive.is_empty = true;
		free(slot->volume_name);

		slot->volume_name = NULL;
		slot->full = false;
	}

	return 0;
}

static int sodr_vtl_drive_update_status(struct so_database_connection * db __attribute__((unused))) {
	return 0;
}

