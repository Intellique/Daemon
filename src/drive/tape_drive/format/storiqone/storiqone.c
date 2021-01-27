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
*  Copyright (C) 2013-2021, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// dgettext
#include <libintl.h>
// free, malloc
#include <stdlib.h>
// struct mtget
#include <sys/mtio.h>
// clock_gettime
#include <time.h>
// close, write
#include <unistd.h>

#include <libstoriqone/archive.h>
#include <libstoriqone/checksum.h>
#include <libstoriqone/database.h>
#include <libstoriqone/drive.h>
#include <libstoriqone/file.h>
#include <libstoriqone/log.h>
#include <libstoriqone/media.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/time.h>
#include <libstoriqone/value.h>
#include <libstoriqone-drive/log.h>
#include <libstoriqone-drive/media.h>
#include <libstoriqone-drive/time.h>

#include "storiqone.h"

static ssize_t sodr_tape_drive_format_storiqone_find_best_block_size(struct so_drive * drive, int fd, struct so_database_connection * db);


static ssize_t sodr_tape_drive_format_storiqone_find_best_block_size(struct so_drive * drive, int fd, struct so_database_connection * db) {
	struct so_media * media = drive->slot->media;
	if (media == NULL)
		return -1;

	sodr_log_add_record(so_job_status_running, db, so_log_level_notice, so_job_record_notif_normal,
		dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Finding best block size"),
		drive->vendor, drive->model, drive->index);

	char * buffer = so_checksum_gen_salt(NULL, 1048576);

	double last_speed = 0;

	drive->status = so_drive_status_rewinding;
	db->ops->sync_drive(db, drive, true, so_database_sync_default);

	static struct mtop rewind = { MTREW, 1 };
	sodr_time_start();
	int failed = ioctl(fd, MTIOCTOP, &rewind);
	sodr_time_stop(drive);

	drive->status = so_drive_status_writing;
	db->ops->sync_drive(db, drive, true, so_database_sync_default);

	ssize_t current_block_size = 0;
	char buf_size[24] = "";

	for (current_block_size = media->media_format->block_size; current_block_size < 1048576; current_block_size <<= 1) {
		so_file_convert_size_to_string(current_block_size, buf_size, 24);

		sodr_log_add_record(so_job_status_running, db, so_log_level_debug, so_job_record_notif_normal,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Test (block size: %s)"),
			drive->vendor, drive->model, drive->index, buf_size);

		unsigned int i, nb_loop = 8589934592 / current_block_size;

		struct timespec start;
		clock_gettime(CLOCK_MONOTONIC, &start);

		for (i = 0; i < nb_loop; i++) {
			ssize_t nb_write = write(fd, buffer, current_block_size);
			media->nb_total_write++;

			if (nb_write < 0) {
				failed = 1;
				break;
			}
		}

		static struct mtop eof = { MTWEOF, 1 };
		sodr_time_start();
		failed = ioctl(fd, MTIOCTOP, &eof);
		sodr_time_stop(drive);

		struct timespec end;
		clock_gettime(CLOCK_MONOTONIC, &end);

		if (failed != 0) {
			drive->status = so_drive_status_error;
			db->ops->sync_drive(db, drive, true, so_database_sync_default);

			current_block_size = -1;
			break;
		}

		double time_spent = so_time_diff(&end, &start) / 1000000000L;
		double speed = i * current_block_size;
		speed /= time_spent;

		char buf_speed[24];
		so_file_convert_size_to_string((ssize_t) speed, buf_speed, 24);
		sodr_log_add_record(so_job_status_running, db, so_log_level_debug, so_job_record_notif_normal,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Test (block size: %s), throughput: %s/s"),
			drive->vendor, drive->model, drive->index, buf_size, buf_speed);

		media->last_write = time(NULL);
		media->write_count++;

		if (last_speed > 0 && speed > 52428800 && last_speed * 1.1 > speed) {
			if (last_speed > speed) {
				current_block_size >>= 1;
				so_file_convert_size_to_string(current_block_size, buf_size, 24);
			}
			break;
		} else
			last_speed = speed;
	}
	free(buffer);

	sodr_log_add_record(so_job_status_running, db, so_log_level_debug, so_job_record_notif_normal,
		dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Found best block size: %s"),
		drive->vendor, drive->model, drive->index, buf_size);

	drive->status = failed != 0 ? so_drive_status_error : so_drive_status_loaded_idle;
	db->ops->sync_drive(db, drive, true, so_database_sync_default);

	return current_block_size;
}

int sodr_tape_drive_format_storiqone_format_media(struct so_drive * drive, int fd, struct so_pool * pool, struct so_value * option, struct so_database_connection * db) {
	ssize_t block_size = 0;
	so_value_unpack(option, "{sz}", "block size", &block_size);

	if (block_size < 0)
		return 1;

	struct so_media * media = drive->slot->media;

	if (media->type == so_media_type_cleaning) {
		sodr_log_add_record(so_job_status_running, db, so_log_level_notice, so_job_record_notif_normal,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Trying to format a cleaning media '%s'"),
			drive->vendor, drive->model, drive->index, media->name);
		return -1;
	}

	if (block_size == 0) {
		char str_block_size[16];

		if (media->type == so_media_type_worm) {
			block_size = media->media_format->block_size;
			so_file_convert_size_to_string(block_size, str_block_size, 16);

			sodr_log_add_record(so_job_status_running, db, so_log_level_warning, so_job_record_notif_normal,
				dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Using default block size (%s) for WORM media '%s'"),
				drive->vendor, drive->model, drive->index, str_block_size, media->name);
		} else {
			block_size = db->ops->get_block_size_by_pool(db, pool);

			if (block_size <= 0) {
				block_size = sodr_tape_drive_format_storiqone_find_best_block_size(drive, fd, db);

				if (block_size < 0)
					sodr_log_add_record(so_job_status_running, db, so_log_level_error, so_job_record_notif_normal,
						dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Failed to find the best block size for media '%s'"),
						drive->vendor, drive->model, drive->index, media->name);
			} else {
				so_file_convert_size_to_string(block_size, str_block_size, 16);
				sodr_log_add_record(so_job_status_running, db, so_log_level_warning, so_job_record_notif_normal,
					dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Reusing block size (%s) of pool '%s' for media '%s'"),
					drive->vendor, drive->model, drive->index, str_block_size, pool->name, media->name);
			}
		}
	}

	if (block_size < 0)
		return 1;

	drive->status = so_drive_status_rewinding;
	db->ops->sync_drive(db, drive, true, so_database_sync_default);

	static struct mtop rewind = { MTREW, 1 };
	sodr_time_start();
	int failed = ioctl(fd, MTIOCTOP, &rewind);
	sodr_time_stop(drive);

	drive->status = failed != 0 ? so_drive_status_error : so_drive_status_loaded_idle;
	db->ops->sync_drive(db, drive, true, so_database_sync_default);

	if (failed != 0)
		return failed;

	media->block_size = block_size;

	char * header = malloc(block_size);
	if (!sodr_media_write_header(media, pool, header, block_size)) {
		free(header);
		return 1;
	}

	drive->status = so_drive_status_writing;
	db->ops->sync_drive(db, drive, true, so_database_sync_default);

	sodr_time_start();
	ssize_t nb_write = write(fd, header, block_size);
	sodr_time_stop(drive);

	if (nb_write < 0) {
		drive->status = so_drive_status_error;
		db->ops->sync_drive(db, drive, true, so_database_sync_default);

		free(header);
		return 1;
	}

	static struct mtop eof = { MTWEOF, 1 };
	sodr_time_start();
	failed = ioctl(fd, MTIOCTOP, &eof);
	sodr_time_stop(drive);

	if (failed == 0) {
		media->status = so_media_status_in_use;

		media->last_write = time(NULL);

		media->write_count++;
		media->operation_count++;

		media->nb_total_write++;

		media->block_size = block_size;
		media->total_block = media->media_format->capacity / block_size;
		media->free_block = media->total_block - 1;
		media->append = true;

		media->nb_volumes = 1;

		so_pool_free(media->pool);
		media->pool = pool;
	} else
		so_pool_free(pool);

	drive->status = failed != 0 ? so_drive_status_error : so_drive_status_loaded_idle;
	db->ops->sync_drive(db, drive, true, so_database_sync_default);

	if (failed == 0)
		media->archive_format = so_archive_format_dup(pool->archive_format);

	free(header);

	return failed;
}
