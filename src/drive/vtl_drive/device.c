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
// open
#include <fcntl.h>
// glob, globfree
#include <glob.h>
// dgettext
#include <libintl.h>
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
#include <libstoriqone/format.h>
#include <libstoriqone/log.h>
#include <libstoriqone/process.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/value.h>
#include <libstoriqone-drive/archive_format.h>
#include <libstoriqone-drive/drive.h>
#include <libstoriqone-drive/log.h>
#include <libstoriqone-drive/media.h>
#include <libstoriqone-drive/time.h>

#include "device.h"
#include "io.h"

static bool sodr_vtl_drive_check_header(struct sodr_peer * peer, struct so_database_connection * db);
static bool sodr_vtl_drive_check_support(struct so_media_format * format, bool for_writing, struct so_database_connection * db);
static unsigned int sodr_vtl_drive_count_archives(struct sodr_peer * peer, const bool * const disconnected, struct so_database_connection * db);
static bool sodr_vtl_drive_erase_file(const char * path);
static int sodr_vtl_drive_erase_media(struct sodr_peer * peer, bool quick_mode, struct so_database_connection * db);
static int sodr_vtl_drive_format_media(struct sodr_peer * peer, struct so_pool * pool, struct so_value * option, struct so_database_connection * db);
static struct so_stream_reader * sodr_vtl_drive_get_raw_reader(struct sodr_peer * peer, int file_position, struct so_database_connection * db);
static struct so_stream_writer * sodr_vtl_drive_get_raw_writer(struct sodr_peer * peer, struct so_database_connection * db);
static struct so_format_reader * sodr_vtl_drive_get_reader(struct sodr_peer * peer, int file_position, struct so_value * checksums, struct so_database_connection * db);
static struct so_format_writer * sodr_vtl_drive_get_writer(struct sodr_peer * peer, struct so_value * checksums, struct so_database_connection * db);
static int sodr_vtl_drive_init(struct so_value * config, struct so_database_connection * db_connect);
static struct so_archive * sodr_vtl_drive_parse_archive(struct sodr_peer * peer, const bool * const disconnected, unsigned int archive_position, struct so_value * checksums, struct so_database_connection * db);
static int sodr_vtl_drive_reset(struct so_database_connection * db);
static int sodr_vtl_drive_update_status(struct so_database_connection * db);

static char * sodr_vtl_media_dir = NULL;
static struct so_media_format * sodr_vtl_media_format = NULL;

static struct so_drive_ops sodr_vtl_drive_ops = {
	.check_header   = sodr_vtl_drive_check_header,
	.check_support  = sodr_vtl_drive_check_support,
	.count_archives = sodr_vtl_drive_count_archives,
	.erase_media    = sodr_vtl_drive_erase_media,
	.format_media   = sodr_vtl_drive_format_media,
	.get_raw_reader = sodr_vtl_drive_get_raw_reader,
	.get_raw_writer = sodr_vtl_drive_get_raw_writer,
	.get_reader     = sodr_vtl_drive_get_reader,
	.get_writer     = sodr_vtl_drive_get_writer,
	.init           = sodr_vtl_drive_init,
	.parse_archive  = sodr_vtl_drive_parse_archive,
	.reset          = sodr_vtl_drive_reset,
	.update_status  = sodr_vtl_drive_update_status,
};

static struct so_drive sodr_vtl_drive = {
	.status      = so_drive_status_empty_idle,
	.enable      = true,

	.density_code       = 0,
	.mode               = so_media_format_mode_linear,
	.operation_duration = 0,
	.is_empty           = true,

	.model         = "Storiq one vtl drive",
	.vendor        = "Intellique",
	.revision      = "B01",
	.serial_number = NULL,

	.changer = NULL,
	.index   = 0,
	.slot    = NULL,

	.ops = &sodr_vtl_drive_ops,

	.data    = NULL,
	.db_data = NULL,
};


static bool sodr_vtl_drive_check_header(struct sodr_peer * peer, struct so_database_connection * db) {
	sodr_vtl_drive.status = so_drive_status_reading;
	db->ops->sync_drive(db, &sodr_vtl_drive, true, so_database_sync_default);

	char * file;
	int size = asprintf(&file, "%s/file_0", sodr_vtl_media_dir);
	if (size < 0) {
		sodr_vtl_drive.status = so_drive_status_error;
		db->ops->sync_drive(db, &sodr_vtl_drive, true, so_database_sync_default);

		return false;
	}

	char * header = so_file_read_all_from(file);
	free(file);

	struct so_media * media = sodr_vtl_drive.slot->media;
	media->last_read = time(NULL);
	media->read_count++;
	media->operation_count++;
	media->nb_total_read++;

	bool ok = true;
	if (header != NULL) {
		ok = sodr_media_check_header(peer, sodr_vtl_drive.slot->media, header, db);
		free(header);
	}

	sodr_vtl_drive.status = so_drive_status_loaded_idle;
	db->ops->sync_drive(db, &sodr_vtl_drive, true, so_database_sync_default);

	return ok;
}

static bool sodr_vtl_drive_check_support(struct so_media_format * format, bool for_writing __attribute__((unused)), struct so_database_connection * db __attribute__((unused))) {
	return so_media_format_cmp(format, sodr_vtl_media_format) == 0;
}

static unsigned int sodr_vtl_drive_count_archives(struct sodr_peer * peer, const bool * const disconnected, struct so_database_connection * db) {
	struct so_media * media = sodr_vtl_drive.slot->media;
	if (media == NULL)
		return 0;

	return sodr_media_storiqone_count_files(peer, &sodr_vtl_drive, disconnected, db);
}

static bool sodr_vtl_drive_erase_file(const char * path) {
	int failed = unlink(path);
	if (failed != 0)
		so_log_write(so_log_level_error,
			dgettext("storiqone-drive-vtl", "[%s %s %d]: Failed to erase file '%s' because %m"),
			sodr_vtl_drive.vendor, sodr_vtl_drive.model, sodr_vtl_drive.index, path);

	return failed == 0;
}

static int sodr_vtl_drive_erase_media(struct sodr_peer * peer, bool quick_mode, struct so_database_connection * db) {
	char * files;
	int size = asprintf(&files, "%s/file_*", sodr_vtl_media_dir);
	if (size < 0)
		return 1;

	sodr_vtl_drive.status = so_drive_status_erasing;
	db->ops->sync_drive(db, &sodr_vtl_drive, true, so_database_sync_default);

	glob_t gl;
	int ret = glob(files, 0, NULL, &gl);
	if (ret != 0 && ret != GLOB_NOMATCH) {
		struct so_media * media = sodr_vtl_drive.slot->media;
		sodr_log_add_record(peer, so_job_status_running, db, so_log_level_error, so_job_record_notif_important,
				dgettext("storiqone-drive-vtl", "[%s %s %d]: Failed to list files from media '%s'"),
				sodr_vtl_drive.vendor, sodr_vtl_drive.model, sodr_vtl_drive.index,
				media != NULL ? media->name : dgettext("storiqone-drive-vtl", "NULL"));

		globfree(&gl);
		free(files);

		sodr_vtl_drive.status = so_drive_status_loaded_idle;
		db->ops->sync_drive(db, &sodr_vtl_drive, true, so_database_sync_default);

		return 1;
	}

	unsigned int i;
	bool ok = true;
	if (quick_mode)
		for (i = 0; i < gl.gl_pathc && ok; i++)
			ok = sodr_vtl_drive_erase_file(gl.gl_pathv[i]);
	else
		for (i = 0; i < gl.gl_pathc; i++) {
			const char * params[] = { "-uz", gl.gl_pathv[i] };
			struct so_process command;
			so_process_new(&command, "shred", params, 2);
			so_process_start(&command, 1);

			if (command.pid > 0) {
				so_process_wait(&command, 1);

				if (command.exited_code != 0) {
					ok = false;
					sodr_log_add_record(peer, so_job_status_running, db, so_log_level_error, so_job_record_notif_important,
						dgettext("storiqone-drive-vtl", "[%s %s %d]: Command 'shred' ended with code '%d' while removing file '%s'"),
						sodr_vtl_drive.vendor, sodr_vtl_drive.model, sodr_vtl_drive.index, command.exited_code, gl.gl_pathv[i]);
				}
			} else {
				ok = false;
				sodr_log_add_record(peer, so_job_status_running, db, so_log_level_error, so_job_record_notif_important,
					dgettext("storiqone-drive-vtl", "[%s %s %d]: Failed to start 'shred' command to remove file '%s'"),
					sodr_vtl_drive.vendor, sodr_vtl_drive.model, sodr_vtl_drive.index, gl.gl_pathv[i]);
			}

			so_process_free(&command, 1);
		}

	globfree(&gl);
	free(files);

	sodr_vtl_drive.status = so_drive_status_loaded_idle;
	db->ops->sync_drive(db, &sodr_vtl_drive, true, so_database_sync_default);

	struct so_media * media = sodr_vtl_drive.slot->media;
	media->write_count++;
	media->operation_count++;
	media->last_write = time(NULL);
	media->nb_volumes = 0;
	media->free_block = media->total_block;
	media->status = ok ? so_media_status_new : so_media_status_error;
	media->append = true;
	media->uuid[0] = '\0';

	so_archive_format_free(media->archive_format);
	media->archive_format = NULL;

	so_pool_free(media->pool);
	media->pool = NULL;

	return ok ? 0 : 1;
}

static int sodr_vtl_drive_format_media(struct sodr_peer * peer, struct so_pool * pool, struct so_value * option, struct so_database_connection * db) {
	ssize_t block_size = 0;
	so_value_unpack(option, "{sz}", "block size", &block_size);

	if (block_size < 0)
		return 1;

	if (pool->archive_format == NULL || pool->archive_format->name == NULL || strcmp(pool->archive_format->name, "Storiq One") != 0) {
		sodr_log_add_record(peer, so_job_status_running, db, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-drive-vtl", "[%s %s %d]: VTL does only support Storiq One format"),
			sodr_vtl_drive.vendor, sodr_vtl_drive.model, sodr_vtl_drive.index);
		return 1;
	}

	char * files;
	int size = asprintf(&files, "%s/file_*", sodr_vtl_media_dir);
	if (size < 0)
		return 1;

	sodr_vtl_drive.status = so_drive_status_writing;
	db->ops->sync_drive(db, &sodr_vtl_drive, true, so_database_sync_default);

	glob_t gl;
	int ret = glob(files, 0, NULL, &gl);
	if (ret != 0 && ret != GLOB_NOMATCH) {
		struct so_media * media = sodr_vtl_drive.slot->media;
		sodr_log_add_record(peer, so_job_status_running, db, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-drive-vtl", "[%s %s %d]: Failed to list files from media '%s'"),
			sodr_vtl_drive.vendor, sodr_vtl_drive.model, sodr_vtl_drive.index,
			media != NULL ? media->name : dgettext("storiqone-drive-vtl", "NULL"));

		globfree(&gl);
		free(files);
		return 1;
	}

	unsigned int i;
	bool ok = true;
	for (i = 0; i < gl.gl_pathc && ok; i++)
		ok = sodr_vtl_drive_erase_file(gl.gl_pathv[i]);

	globfree(&gl);
	free(files);

	if (block_size == 0) {
		struct stat st;
		int failed = stat(sodr_vtl_media_dir, &st);
		if (failed != 0) {
			struct so_media * media = sodr_vtl_drive.slot->media;
			sodr_log_add_record(peer, so_job_status_running, db, so_log_level_error, so_job_record_notif_important,
				dgettext("storiqone-drive-vtl", "[%s %s %d]: Failed to find optimal block size for media '%s'"),
				sodr_vtl_drive.vendor, sodr_vtl_drive.model, sodr_vtl_drive.index,
				media != NULL ? media->name : dgettext("storiqone-drive-vtl", "NULL"));
			return 1;
		}

		block_size = st.st_blksize;
	}

	char * header = malloc(block_size);

	struct so_media * media = sodr_vtl_drive.slot->media;
	media->status = so_media_status_new;
	media->operation_count++;
	media->block_size = block_size;
	media->total_block = media->media_format->capacity / block_size;
	media->free_block = media->total_block;
	media->nb_volumes = 0;
	media->append = true;

	if (!sodr_media_write_header(media, pool, header, block_size)) {
		sodr_log_add_record(peer, so_job_status_running, db, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-drive-vtl", "[%s %s %d]: Failed to write media header '%s'"),
			sodr_vtl_drive.vendor, sodr_vtl_drive.model, sodr_vtl_drive.index, sodr_vtl_drive.slot->media->name);

		free(header);
		return 1;
	}

	char * file;
	size = asprintf(&file, "%s/file_0", sodr_vtl_media_dir);
	if (size < 0)
		return 1;

	sodr_time_start();
	int fd = open(file, O_CREAT | O_WRONLY, 0640);
	ssize_t nb_write = write(fd, header, block_size);
	close(fd);
	sodr_time_stop(&sodr_vtl_drive);

	free(header);

	media->write_count++;
	media->operation_count++;

	if (nb_write == block_size) {
		media->status = so_media_status_in_use;
		media->last_write = time(NULL);
		media->nb_total_write++;
		media->nb_volumes = 1;
		media->free_block--;

		so_pool_free(media->pool);
		media->archive_format = db->ops->get_archive_format_by_name(db, "Storiq One");
		media->pool = pool;
	} else
		so_pool_free(pool);

	sodr_vtl_drive.status = nb_write != block_size ? so_drive_status_error : so_drive_status_loaded_idle;
	db->ops->sync_drive(db, &sodr_vtl_drive, true, so_database_sync_default);

	free(file);

	return nb_write != block_size;
}

struct so_drive * sodr_vtl_drive_get_device() {
	return &sodr_vtl_drive;
}

static struct so_stream_reader * sodr_vtl_drive_get_raw_reader(struct sodr_peer * peer, int file_position, struct so_database_connection * db) {
	char * filename;
	int size = asprintf(&filename, "%s/file_%d", sodr_vtl_media_dir, file_position);
	if (size < 0)
		return NULL;

	int fd = open(filename, O_RDONLY);

	struct so_stream_reader * reader = NULL;
	if (fd >= 0) {
		sodr_vtl_drive.status = so_drive_status_reading;
		db->ops->sync_drive(db, &sodr_vtl_drive, true, so_database_sync_default);

		reader = sodr_vtl_drive_reader_get_raw_reader(fd, file_position);
	} else
		sodr_log_add_record(peer, so_job_status_running, db, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-drive-vtl", "[%s %s %d]: Failed to open file from media '%s' at position '%d' because %m"),
			sodr_vtl_drive.vendor, sodr_vtl_drive.model, sodr_vtl_drive.index, sodr_vtl_drive.slot->media->name, file_position);

	free(filename);

	return reader;
}

static struct so_stream_writer * sodr_vtl_drive_get_raw_writer(struct sodr_peer * peer, struct so_database_connection * db) {
	char * files;
	int size = asprintf(&files, "%s/file_*", sodr_vtl_media_dir);
	if (size < 0)
		return NULL;

	glob_t gl;
	int ret = glob(files, 0, NULL, &gl);
	if (ret != 0) {
		struct so_media * media = sodr_vtl_drive.slot->media;
		sodr_log_add_record(peer, so_job_status_running, db, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-drive-vtl", "[%s %s %d]: Failed to list files from media '%s'"),
			sodr_vtl_drive.vendor, sodr_vtl_drive.model, sodr_vtl_drive.index,
			media != NULL ? media->name : dgettext("storiqone-drive-vtl", "NULL"));

		return NULL;
	}

	int nb_files = gl.gl_pathc;
	globfree(&gl);
	free(files);

	size = asprintf(&files, "%s/file_%d", sodr_vtl_media_dir, nb_files);
	if (size < 0)
		return NULL;

	struct so_stream_writer * writer = sodr_vtl_drive_writer_get_raw_writer(files, nb_files);
	if (writer != NULL) {
		sodr_vtl_drive.status = so_drive_status_writing;
		db->ops->sync_drive(db, &sodr_vtl_drive, true, so_database_sync_default);
	}

	free(files);
	return writer;
}

static struct so_format_reader * sodr_vtl_drive_get_reader(struct sodr_peer * peer, int file_position, struct so_value * checksums, struct so_database_connection * db) {
	struct so_stream_reader * raw_reader = sodr_vtl_drive_get_raw_reader(peer, file_position, db);
	if (raw_reader == NULL)
		return NULL;

	return so_format_tar_new_reader(raw_reader, checksums);
}

static struct so_format_writer * sodr_vtl_drive_get_writer(struct sodr_peer * peer, struct so_value * checksums, struct so_database_connection * db) {
	struct so_stream_writer * raw_writer = sodr_vtl_drive_get_raw_writer(peer, db);
	if (raw_writer == NULL)
		return NULL;

	return so_format_tar_new_writer(raw_writer, checksums);
}

static int sodr_vtl_drive_init(struct so_value * config, struct so_database_connection * db_connect) {
	struct so_slot * sl = sodr_vtl_drive.slot = malloc(sizeof(struct so_slot));
	bzero(sodr_vtl_drive.slot, sizeof(struct so_slot));
	sl->drive = &sodr_vtl_drive;

	char * root_dir = NULL;
	struct so_value * format = NULL;
	so_value_unpack(config, "{sssssosu}",
		"serial number", &sodr_vtl_drive.serial_number,
		"device", &root_dir,
		"format", &format,
		"index", &sodr_vtl_drive.index
	);

	sodr_vtl_drive.slot->index = sodr_vtl_drive.index;

	if (access(root_dir, R_OK | W_OK | X_OK) != 0)
		return 1;

	int size = asprintf(&sodr_vtl_media_dir, "%s/media", root_dir);
	if (size < 0)
		return 1;

	free(root_dir);


	sodr_vtl_media_format = malloc(sizeof(struct so_media_format));
	bzero(sodr_vtl_media_format, sizeof(struct so_media_format));
	so_media_format_sync(sodr_vtl_media_format, format);

	sodr_archive_format_sync(NULL, 0, db_connect);

	return 0;
}

static struct so_archive * sodr_vtl_drive_parse_archive(struct sodr_peer * peer, const bool * const disconnected, unsigned int archive_position, struct so_value * checksums __attribute__((unused)), struct so_database_connection * db) {
	struct so_media * media = sodr_vtl_drive.slot->media;
	if (media == NULL)
		return NULL;

	return sodr_media_storiqone_parse_archive(peer, &sodr_vtl_drive, disconnected, archive_position, db);
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
		int size = asprintf(&media_uuid, "%s/serial_number", sodr_vtl_media_dir);
		if (size < 0)
			return 1;

		char * uuid = so_file_read_all_from(media_uuid);
		free(media_uuid);

		media = sodr_vtl_drive.slot->media = db->ops->get_media(db, uuid, NULL, NULL);
		if (media == NULL)
			so_log_write(so_log_level_error,
				dgettext("storiqone-drive-vtl", "[%s %s %d]: Failed to find media with UUID '%s' from database"),
				sodr_vtl_drive.vendor, sodr_vtl_drive.model, sodr_vtl_drive.index, uuid);
		free(uuid);

		if (media == NULL)
			return 1;

		slot->volume_name = strdup(media->label);
		slot->full = true;

		bool ok = sodr_vtl_drive_check_header(NULL, db);
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

