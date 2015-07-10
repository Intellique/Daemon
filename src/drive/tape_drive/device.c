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
// glob, globfree
#include <glob.h>
// open
#include <fcntl.h>
// gettext
#include <libintl.h>
// bool
#include <stdbool.h>
// printf, sscanf, snprintf
#include <stdio.h>
// free
#include <stdlib.h>
// strrchr
#include <string.h>
// bzero
#include <strings.h>
// struct mtget
#include <sys/mtio.h>
// open
#include <sys/stat.h>
// open
#include <sys/types.h>
// open, size_t
#include <sys/types.h>
// time
#include <time.h>
// close, write
#include <unistd.h>

#include <libstoriqone/checksum.h>
#include <libstoriqone/database.h>
#include <libstoriqone/format.h>
#include <libstoriqone/log.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/time.h>
#include <libstoriqone/value.h>
#include <libstoriqone-drive/archive_format.h>
#include <libstoriqone-drive/drive.h>
#include <libstoriqone-drive/media.h>
#include <libstoriqone-drive/time.h>

#include "device.h"
#include "format/ltfs/ltfs.h"
#include "io.h"
#include "media.h"
#include "scsi.h"

static bool sodr_tape_drive_check_header(struct so_database_connection * db);
static bool sodr_tape_drive_check_support(struct so_media_format * format, bool for_writing, struct so_database_connection * db);
static unsigned int sodr_tape_drive_count_archives(const bool * const disconnected, struct so_database_connection * db);
static void sodr_tape_drive_create_media(struct so_database_connection * db);
static int sodr_tape_drive_erase_media(bool quick_mode, struct so_database_connection * db);
static ssize_t sodr_tape_drive_find_best_block_size(struct so_database_connection * db);
static int sodr_tape_drive_format_media(struct so_pool * pool, struct so_database_connection * db);
static struct so_stream_reader * sodr_tape_drive_get_raw_reader(int file_position, struct so_database_connection * db);
static struct so_stream_writer * sodr_tape_drive_get_raw_writer(struct so_database_connection * db);
static struct so_format_reader * sodr_tape_drive_get_reader(int file_position, struct so_value * checksums, struct so_database_connection * db);
static struct so_format_writer * sodr_tape_drive_get_writer(struct so_value * checksums, struct so_database_connection * db);
static int sodr_tape_drive_init(struct so_value * config, struct so_database_connection * db_connect);
static void sodr_tape_drive_on_failed(bool verbose, unsigned int sleep_time);
static struct so_archive * sodr_tape_drive_parse_archive(const bool * const disconnected, unsigned int archive_position, struct so_value * checksums, struct so_database_connection * db);
static int sodr_tape_drive_reset(struct so_database_connection * db);
static int sodr_tape_drive_set_file_position(int file_position, struct so_database_connection * db);
static int sodr_tape_drive_update_status(struct so_database_connection * db);

static char * scsi_device = NULL;
static char * so_device = NULL;
static int fd_nst = -1;
static struct mtget status;


static struct so_drive_ops sodr_tape_drive_ops = {
	.check_header         = sodr_tape_drive_check_header,
	.check_support        = sodr_tape_drive_check_support,
	.count_archives       = sodr_tape_drive_count_archives,
	.erase_media          = sodr_tape_drive_erase_media,
	.find_best_block_size = sodr_tape_drive_find_best_block_size,
	.format_media         = sodr_tape_drive_format_media,
	.get_raw_reader       = sodr_tape_drive_get_raw_reader,
	.get_raw_writer       = sodr_tape_drive_get_raw_writer,
	.get_reader           = sodr_tape_drive_get_reader,
	.get_writer           = sodr_tape_drive_get_writer,
	.init                 = sodr_tape_drive_init,
	.parse_archive        = sodr_tape_drive_parse_archive,
	.reset                = sodr_tape_drive_reset,
	.update_status        = sodr_tape_drive_update_status,
};

static struct so_drive sodr_tape_drive = {
	.status      = so_drive_status_unknown,
	.enable      = true,

	.density_code       = 0,
	.mode               = so_media_format_mode_linear,
	.operation_duration = 0,
	.is_empty           = true,

	.model         = NULL,
	.vendor        = NULL,
	.revision      = NULL,
	.serial_number = NULL,

	.changer = NULL,
	.index   = 0,
	.slot    = NULL,

	.ops = &sodr_tape_drive_ops,

	.data    = NULL,
	.db_data = NULL,
};


static bool sodr_tape_drive_check_header(struct so_database_connection * db) {
	size_t block_size = sodr_tape_drive_get_block_size();

	sodr_tape_drive.status = so_drive_status_rewinding;
	db->ops->sync_drive(db, &sodr_tape_drive, true, so_database_sync_default);

	static struct mtop rewind = { MTREW, 1 };
	sodr_time_start();
	int failed = ioctl(fd_nst, MTIOCTOP, &rewind);
	sodr_time_stop(&sodr_tape_drive);

	sodr_tape_drive.status = failed != 0 ? so_drive_status_error : so_drive_status_reading;
	db->ops->sync_drive(db, &sodr_tape_drive, true, so_database_sync_default);

	char * buffer = malloc(block_size);
	sodr_time_start();
	ssize_t nb_read = read(fd_nst, buffer, block_size);
	sodr_time_stop(&sodr_tape_drive);

	sodr_tape_drive.status = nb_read < 0 ? so_drive_status_error : so_drive_status_loaded_idle;
	db->ops->sync_drive(db, &sodr_tape_drive, true, so_database_sync_default);

	if (nb_read < 0) {
		free(buffer);
		return false;
	}

	struct so_media * media = sodr_tape_drive.slot->media;
	// check header
	bool ok = sodr_media_check_header(media, buffer, db);
	if (!ok)
		ok = sodr_tape_drive_media_check_header(media, buffer);

	if (ok && media->private_data == NULL) {
		enum sodr_tape_drive_media_format format = sodr_tape_drive_parse_label(buffer);

		if (format == sodr_tape_drive_media_unknown) {
			so_log_write(so_log_level_error,
				dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Failed to parse media header '%s'"),
				sodr_tape_drive.vendor, sodr_tape_drive.model, sodr_tape_drive.index, media->name);
		} else {
			media->private_data = sodr_tape_drive_media_new(format);
			media->free_private_data = sodr_tape_drive_media_free2;

			if (media->archive_format != NULL)
				so_archive_format_free(media->archive_format);

			switch (format) {
				case sodr_tape_drive_media_storiq_one:
					media->archive_format = db->ops->get_archive_format_by_name(db, "Storiq One");
					break;

				case sodr_tape_drive_media_ltfs:
					media->archive_format = db->ops->get_archive_format_by_name(db, "LTFS");
					break;

				default:
					media->archive_format = NULL;
					break;
			}
		}
	}

	free(buffer);
	return ok;
}

static bool sodr_tape_drive_check_support(struct so_media_format * format, bool for_writing, struct so_database_connection * db __attribute__((unused))) {
	return sodr_tape_drive_scsi_check_support(format, for_writing, scsi_device);
}

static unsigned int sodr_tape_drive_count_archives(const bool * const disconnected, struct so_database_connection * db) {
	struct so_media * media = sodr_tape_drive.slot->media;
	if (media == NULL)
		return 0;

	struct sodr_tape_drive_media * mp = media->private_data;
	switch (mp->format) {
		case sodr_tape_drive_media_storiq_one:
			return sodr_media_storiqone_count_files(&sodr_tape_drive, disconnected, db);

		case sodr_tape_drive_media_ltfs:
			return sodr_tape_drive_format_ltfs_count_archives(media);

		default:
			return 0;
	}
}

static void sodr_tape_drive_create_media(struct so_database_connection * db) {
	struct so_media * media = malloc(sizeof(struct so_media));
	bzero(media, sizeof(struct so_media));
	sodr_tape_drive.slot->media = media;

	if (sodr_tape_drive.slot->volume_name != NULL) {
		media->label = strdup(sodr_tape_drive.slot->volume_name);
		media->name = strdup(media->label);
	}

	unsigned int density_code = ((status.mt_dsreg & MT_ST_DENSITY_MASK) >> MT_ST_DENSITY_SHIFT) & 0xFF;
	media->media_format = db->ops->get_media_format(db, (unsigned char) density_code, so_media_format_mode_linear);

	media->status = so_media_status_foreign;
	media->first_used = time(NULL);
	media->use_before = media->first_used + media->media_format->life_span;
	media->append = false;

	media->load_count = 1;

	media->block_size = sodr_tape_drive_get_block_size();
	if (media->block_size < 1024)
		media->block_size = 1024;

	if (media->media_format != NULL && media->media_format->support_mam) {
		int fd = open(scsi_device, O_RDWR);
		int failed = sodr_tape_drive_scsi_read_mam(fd, media);
		close(fd);

		if (failed != 0)
			so_log_write(so_log_level_warning,
				dgettext("storiqone-drive-tape", "[%s | %s | #%u]: failed to read medium axilary memory"),
				sodr_tape_drive.vendor, sodr_tape_drive.model, sodr_tape_drive.index);
	}

	sodr_tape_drive_check_header(db);
}

static int sodr_tape_drive_erase_media(bool quick_mode, struct so_database_connection * db) {
	struct so_media * media = sodr_tape_drive.slot->media;
	if (media == NULL)
		return -1;

	int failed = sodr_tape_drive_set_file_position(0, db);
	if (failed != 0)
		return failed;

	so_log_write(so_log_level_info,
		dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Erasing media '%s' (mode: %s)"),
		sodr_tape_drive.vendor, sodr_tape_drive.model, sodr_tape_drive.index, media->name,
		quick_mode ? dgettext("storiqone-drive-tape", "quick") : dgettext("storiqone-drive-tape", "long"));
	failed = sodr_tape_drive_scsi_erase_media(scsi_device, quick_mode);

	if (failed != 0)
		so_log_write(so_log_level_error,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Failed to erase media '%s' (mode: %s)"),
			sodr_tape_drive.vendor, sodr_tape_drive.model, sodr_tape_drive.index, media->name,
			quick_mode ? dgettext("storiqone-drive-tape", "quick") : dgettext("storiqone-drive-tape", "long"));
	else {
		media->uuid[0] = '\0';
		media->status = so_media_status_new;
		media->last_write = time(NULL);

		media->free_block = media->total_block;

		media->nb_volumes = 0;
		media->append = true;

		if (media->archive_format != NULL)
			so_archive_format_free(media->archive_format);
		media->archive_format = NULL;

		if (media->pool != NULL)
			so_pool_free(media->pool);
		media->pool = NULL;

		so_log_write(so_log_level_error,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: media '%s' has been erased with success (mode: %s)"),
			sodr_tape_drive.vendor, sodr_tape_drive.model, sodr_tape_drive.index, media->name,
			quick_mode ? dgettext("storiqone-drive-tape", "quick") : dgettext("storiqone-drive-tape", "long"));
	}

	return failed;
}

static ssize_t sodr_tape_drive_find_best_block_size(struct so_database_connection * db) {
	struct so_media * media = sodr_tape_drive.slot->media;
	if (media == NULL)
		return -1;

	char * buffer = so_checksum_gen_salt(NULL, 1048576);

	double last_speed = 0;

	ssize_t current_block_size;
	for (current_block_size = media->media_format->block_size; current_block_size < 1048576; current_block_size <<= 1) {
		sodr_tape_drive.status = so_drive_status_rewinding;
		db->ops->sync_drive(db, &sodr_tape_drive, true, so_database_sync_default);

		static struct mtop rewind = { MTREW, 1 };
		sodr_time_start();
		int failed = ioctl(fd_nst, MTIOCTOP, &rewind);
		sodr_time_stop(&sodr_tape_drive);

		if (failed != 0) {
			sodr_tape_drive.status = so_drive_status_error;
			db->ops->sync_drive(db, &sodr_tape_drive, true, so_database_sync_default);

			current_block_size = -1;
			break;
		}

		sodr_tape_drive.status = so_drive_status_writing;
		db->ops->sync_drive(db, &sodr_tape_drive, true, so_database_sync_default);

		unsigned int i, nb_loop = 8589934592 / current_block_size;

		struct timespec start;
		clock_gettime(CLOCK_MONOTONIC, &start);

		for (i = 0; i < nb_loop; i++) {
			ssize_t nb_write = write(fd_nst, buffer, current_block_size);
			if (nb_write < 0) {
				failed = 1;
				break;
			}
		}

		struct timespec end;
		clock_gettime(CLOCK_MONOTONIC, &end);

		static struct mtop eof = { MTWEOF, 1 };
		sodr_time_start();
		failed = ioctl(fd_nst, MTIOCTOP, &eof);
		sodr_time_stop(&sodr_tape_drive);

		if (failed != 0) {
			sodr_tape_drive.status = so_drive_status_error;
			db->ops->sync_drive(db, &sodr_tape_drive, true, so_database_sync_default);

			current_block_size = -1;
			break;
		}

		double time_spent = so_time_diff(&end, &start) / 1000000000L;
		double speed = i * current_block_size;
		speed /= time_spent;

		if (last_speed > 0 && speed > 52428800 && last_speed * 1.1 > speed)
			break;
		else
			last_speed = speed;
	}
	free(buffer);

	sodr_tape_drive.status = so_drive_status_rewinding;
	db->ops->sync_drive(db, &sodr_tape_drive, true, so_database_sync_default);

	static struct mtop rewind = { MTREW, 1 };
	sodr_time_start();
	int failed = ioctl(fd_nst, MTIOCTOP, &rewind);
	sodr_time_stop(&sodr_tape_drive);

	sodr_tape_drive.status = failed != 0 ? so_drive_status_error : so_drive_status_loaded_idle;
	db->ops->sync_drive(db, &sodr_tape_drive, true, so_database_sync_default);

	return current_block_size;
}

static int sodr_tape_drive_format_media(struct so_pool * pool, struct so_database_connection * db) {
	size_t block_size = sodr_tape_drive_get_block_size();

	sodr_tape_drive.status = so_drive_status_rewinding;
	db->ops->sync_drive(db, &sodr_tape_drive, true, so_database_sync_default);

	static struct mtop rewind = { MTREW, 1 };
	sodr_time_start();
	int failed = ioctl(fd_nst, MTIOCTOP, &rewind);
	sodr_time_stop(&sodr_tape_drive);

	sodr_tape_drive.status = failed != 0 ? so_drive_status_error : so_drive_status_loaded_idle;
	db->ops->sync_drive(db, &sodr_tape_drive, true, so_database_sync_default);

	if (failed != 0)
		return failed;

	struct so_media * media = sodr_tape_drive.slot->media;
	char * header = malloc(block_size);
	if (!sodr_media_write_header(media, pool, header, block_size)) {
		free(header);
		return 1;
	}

	sodr_tape_drive.status = so_drive_status_writing;
	db->ops->sync_drive(db, &sodr_tape_drive, true, so_database_sync_default);

	sodr_time_start();
	ssize_t nb_write = write(fd_nst, header, block_size);
	sodr_time_stop(&sodr_tape_drive);

	if (nb_write < 0) {
		sodr_tape_drive.status = so_drive_status_error;
		db->ops->sync_drive(db, &sodr_tape_drive, true, so_database_sync_default);

		free(header);
		return 1;
	}

	static struct mtop eof = { MTWEOF, 1 };
	sodr_time_start();
	failed = ioctl(fd_nst, MTIOCTOP, &eof);
	sodr_time_stop(&sodr_tape_drive);

	sodr_tape_drive.status = failed != 0 ? so_drive_status_error : so_drive_status_loaded_idle;
	db->ops->sync_drive(db, &sodr_tape_drive, true, so_database_sync_default);

	if (failed == 0)
		media->archive_format = db->ops->get_archive_format_by_name(db, pool->archive_format->name);

	free(header);

	return failed;
}

ssize_t sodr_tape_drive_get_block_size() {
	ssize_t block_size = (status.mt_dsreg & MT_ST_BLKSIZE_MASK) >> MT_ST_BLKSIZE_SHIFT;
	if (block_size > 0)
		return block_size;

	struct so_media * media = sodr_tape_drive.slot->media;
	if (sodr_tape_drive.slot->media != NULL && media->block_size > 0)
		return media->block_size;

	sodr_tape_drive.status = so_drive_status_rewinding;

	static struct mtop rewind = { MTREW, 1 };
	sodr_time_start();
	int failed = ioctl(fd_nst, MTIOCTOP, &rewind);
	sodr_time_stop(&sodr_tape_drive);

	sodr_tape_drive.status = so_drive_status_loaded_idle;

	unsigned int i;
	ssize_t nb_read;
	block_size = 1 << 16;
	char * buffer = malloc(block_size);
	for (i = 0; i < 4 && buffer != NULL && failed == 0; i++) {
		sodr_tape_drive.status = so_drive_status_reading;

		sodr_time_start();
		nb_read = read(fd_nst, buffer, block_size);
		sodr_time_stop(&sodr_tape_drive);

		sodr_tape_drive.status = so_drive_status_loaded_idle;

		sodr_time_start();
		failed = ioctl(fd_nst, MTIOCGET, &status);
		sodr_time_stop(&sodr_tape_drive);

		if (!failed && status.mt_blkno < 1)
			break;

		if (media != NULL) {
			media->last_read = time(NULL);
			media->read_count++;
			media->nb_total_read++;
		}

		if (nb_read > 0) {
			so_log_write(so_log_level_notice,
				dgettext("storiqone-drive-tape", "[%s | %s | #%u]: found block size: %zd"),
				sodr_tape_drive.vendor, sodr_tape_drive.model, sodr_tape_drive.index, nb_read);

			free(buffer);
			return nb_read;
		}

		sodr_tape_drive.status = so_drive_status_rewinding;

		sodr_time_start();
		failed = ioctl(fd_nst, MTIOCTOP, &rewind);
		sodr_time_stop(&sodr_tape_drive);

		sodr_tape_drive.status = so_drive_status_loaded_idle;

		if (failed == 0) {
			block_size <<= 1;
			void * new_addr = realloc(buffer, block_size);
			if (new_addr == NULL)
				free(buffer);
			buffer = new_addr;
		} else
			break;
	}

	free(buffer);

	if (media != NULL && media->media_format != NULL)
		return media->media_format->block_size;

	return -1;
}

struct so_drive * sodr_tape_drive_get_device() {
	return &sodr_tape_drive;
}

static struct so_stream_reader * sodr_tape_drive_get_raw_reader(int file_position, struct so_database_connection * db) {
	if (sodr_tape_drive.slot->media == NULL)
		return NULL;

	if (sodr_tape_drive_set_file_position(file_position, db) != 0)
		return NULL;

	sodr_tape_drive.slot->media->read_count++;
	so_log_write(so_log_level_debug,
		dgettext("storiqone-drive-tape", "[%s | %s | #%u]: drive is open for reading"),
		sodr_tape_drive.vendor, sodr_tape_drive.model, sodr_tape_drive.index);

	return sodr_tape_drive_reader_get_raw_reader(&sodr_tape_drive, fd_nst, file_position);
}

static struct so_stream_writer * sodr_tape_drive_get_raw_writer(struct so_database_connection * db) {
	if (sodr_tape_drive.slot->media == NULL)
		return NULL;

	if (sodr_tape_drive_set_file_position(-1, db))
		return NULL;

	sodr_tape_drive.slot->media->write_count++;
	so_log_write(so_log_level_debug,
		dgettext("storiqone-drive-tape", "[%s | %s | #%u]: drive is open for writing"),
		sodr_tape_drive.vendor, sodr_tape_drive.model, sodr_tape_drive.index);

	return sodr_tape_drive_writer_get_raw_writer(&sodr_tape_drive, fd_nst, status.mt_fileno);
}

static struct so_format_reader * sodr_tape_drive_get_reader(int file_position, struct so_value * checksums, struct so_database_connection * db) {
	struct so_stream_reader * reader = sodr_tape_drive_get_raw_reader(file_position, db);
	if (reader == NULL)
		return NULL;

	struct so_media * media = sodr_tape_drive.slot->media;
	if (media == NULL)
		return NULL;

	struct sodr_tape_drive_media * mp = media->private_data;

	switch (mp->format) {
		case sodr_tape_drive_media_storiq_one:
			return so_format_tar_new_reader(reader, checksums);

		case sodr_tape_drive_media_ltfs: {
			int scsi_fd = open(scsi_device, O_RDWR);
			if (scsi_fd < 0)
				return NULL;

			return sodr_tape_drive_format_ltfs_new_reader(&sodr_tape_drive, fd_nst, scsi_fd);
		}

		default:
			return NULL;
	}
}

static struct so_format_writer * sodr_tape_drive_get_writer(struct so_value * checksums, struct so_database_connection * db) {
	struct so_stream_writer * writer = sodr_tape_drive_get_raw_writer(db);
	if (writer == NULL)
		return NULL;

	return so_format_tar_new_writer(writer, checksums);
}

static int sodr_tape_drive_init(struct so_value * config, struct so_database_connection * db_connect) {
	struct so_slot * sl = sodr_tape_drive.slot = malloc(sizeof(struct so_slot));
	bzero(sodr_tape_drive.slot, sizeof(struct so_slot));
	sl->drive = &sodr_tape_drive;

	so_value_unpack(config, "{sssssssbs{sisbsbss}}",
		"model", &sodr_tape_drive.model,
		"vendor", &sodr_tape_drive.vendor,
		"serial number", &sodr_tape_drive.serial_number,
		"enable", &sodr_tape_drive.enable,
		"slot",
			"index", &sl->index,
			"enable", &sl->enable,
			"ie port", &sl->is_ie_port,
			"volume name", &sl->volume_name
	);
	sodr_tape_drive.index = sl->index;

	glob_t gl;
	glob("/sys/class/scsi_device/*/device/scsi_tape", 0, NULL, &gl);

	unsigned int i;
	bool found = false;
	for (i = 0; i < gl.gl_pathc && !found; i++) {
		char * ptr = strrchr(gl.gl_pathv[i], '/');
		*ptr = '\0';

		char link[256];
		ssize_t length = readlink(gl.gl_pathv[i], link, 256);
		link[length] = '\0';

		ptr = strrchr(link, '/') + 1;

		char path[256];
		strcpy(path, gl.gl_pathv[i]);
		strcat(path, "/generic");
		length = readlink(path, link, 256);
		link[length] = '\0';

		char scsi_dev[12];
		ptr = strrchr(link, '/');
		strcpy(scsi_dev, "/dev");
		strcat(scsi_dev, ptr);

		strcpy(path, gl.gl_pathv[i]);
		strcat(path, "/tape");
		length = readlink(path, link, 256);
		link[length] = '\0';

		char device[12];
		ptr = strrchr(link, '/') + 1;
		strcpy(device, "/dev/n");
		strcat(device, ptr);

		found = sodr_tape_drive_scsi_check_drive(&sodr_tape_drive, scsi_dev);
		if (found) {
			so_device = strdup(device);
			scsi_device = strdup(scsi_dev);
		}
	}
	globfree(&gl);

	if (found) {
		sodr_tape_drive_scsi_read_density(&sodr_tape_drive, scsi_device);

		fd_nst = open(so_device, O_RDWR | O_NONBLOCK);
		sodr_time_start();
		int failed = ioctl(fd_nst, MTIOCGET, &status);
		sodr_time_stop(&sodr_tape_drive);

		if (failed != 0) {
			if (GMT_ONLINE(status.mt_gstat)) {
				sodr_tape_drive.status = so_drive_status_loaded_idle;
				sodr_tape_drive.is_empty = false;
			} else {
				sodr_tape_drive.status = so_drive_status_empty_idle;
				sodr_tape_drive.is_empty = true;
			}
		}

		static struct so_archive_format formats[] = {
			{ "LTFS", true, false, NULL },
		};
		sodr_archive_format_sync(formats, 1, db_connect);
	}

	return found ? 0 : 1;
}

static void sodr_tape_drive_on_failed(bool verbose, unsigned int sleep_time) {
	if (verbose)
		so_log_write(so_log_level_debug,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Try to recover an error"),
			sodr_tape_drive.vendor, sodr_tape_drive.model, sodr_tape_drive.index);

	close(fd_nst);
	sleep(sleep_time);
	fd_nst = open(so_device, O_RDWR | O_NONBLOCK);
}

static struct so_archive * sodr_tape_drive_parse_archive(const bool * const disconnected, unsigned int archive_position, struct so_value * checksums, struct so_database_connection * db) {
	struct so_media * media = sodr_tape_drive.slot->media;
	if (media == NULL)
		return NULL;

	struct sodr_tape_drive_media * mp = media->private_data;
	switch (mp->format) {
		case sodr_tape_drive_media_storiq_one:
			return sodr_media_storiqone_parse_archive(&sodr_tape_drive, disconnected, archive_position, db);

		case sodr_tape_drive_media_ltfs:
			if (archive_position > 0)
				return NULL;
			return sodr_tape_drive_format_ltfs_parse_archive(&sodr_tape_drive, disconnected, checksums, db);

		default:
			return NULL;
	}
}

static int sodr_tape_drive_reset(struct so_database_connection * db) {
	sodr_tape_drive_on_failed(false, 1);
	return sodr_tape_drive_update_status(db);
}

static int sodr_tape_drive_set_file_position(int file_position, struct so_database_connection * db) {
	int failed;
	if (file_position < 0) {
		so_log_write(so_log_level_info,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Fast forwarding to end of media '%s'"),
			sodr_tape_drive.vendor, sodr_tape_drive.model, sodr_tape_drive.index, sodr_tape_drive.slot->media->name);
		sodr_tape_drive.status = so_drive_status_positioning;
		db->ops->sync_drive(db, &sodr_tape_drive, true, so_database_sync_default);

		static struct mtop eod = { MTEOM, 1 };
		sodr_time_start();
		failed = ioctl(fd_nst, MTIOCTOP, &eod);
		sodr_time_stop(&sodr_tape_drive);

		if (failed != 0)
			so_log_write(so_log_level_error,
				dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Fast forwarding to end of media '%s' finished with error '%m'"),
				sodr_tape_drive.vendor, sodr_tape_drive.model, sodr_tape_drive.index, sodr_tape_drive.slot->media->name);
		else
			so_log_write(so_log_level_info,
				dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Fast forwarding to end of media '%s' finished with status = OK"),
				sodr_tape_drive.vendor, sodr_tape_drive.model, sodr_tape_drive.index, sodr_tape_drive.slot->media->name);

		sodr_tape_drive.status = failed ? so_drive_status_error : so_drive_status_positioning;
		db->ops->sync_drive(db, &sodr_tape_drive, true, so_database_sync_default);
	} else {
		so_log_write(so_log_level_info,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Positioning media '%s' to position #%d"),
			sodr_tape_drive.vendor, sodr_tape_drive.model, sodr_tape_drive.index, sodr_tape_drive.slot->media->name, file_position);
		sodr_tape_drive.status = so_drive_status_rewinding;
		db->ops->sync_drive(db, &sodr_tape_drive, true, so_database_sync_default);

		static struct mtop rewind = { MTREW, 1 };
		sodr_time_start();
		failed = ioctl(fd_nst, MTIOCTOP, &rewind);
		sodr_time_stop(&sodr_tape_drive);

		if (failed != 0) {
			sodr_tape_drive.status = so_drive_status_error;
			so_log_write(so_log_level_error,
				dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Positioning media '%s' to position #%d finished with error '%m'"),
				sodr_tape_drive.vendor, sodr_tape_drive.model, sodr_tape_drive.index, sodr_tape_drive.slot->media->name, file_position);
			db->ops->sync_drive(db, &sodr_tape_drive, true, so_database_sync_default);
			return failed;
		}

		if (file_position > 0) {
			sodr_tape_drive.status = so_drive_status_positioning;
			db->ops->sync_drive(db, &sodr_tape_drive, true, so_database_sync_default);

			struct mtop fsr = { MTFSF, file_position };
			sodr_time_start();
			failed = ioctl(fd_nst, MTIOCTOP, &fsr);
			sodr_time_stop(&sodr_tape_drive);

			if (failed != 0)
				so_log_write(so_log_level_error,
					dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Positioning media '%s' to position #%d finished with error '%m'"),
					sodr_tape_drive.vendor, sodr_tape_drive.model, sodr_tape_drive.index, sodr_tape_drive.slot->media->name, file_position);
			else
				so_log_write(so_log_level_info,
					dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Positioning media '%s' to position #%d finished with status = OK"),
					sodr_tape_drive.vendor, sodr_tape_drive.model, sodr_tape_drive.index, sodr_tape_drive.slot->media->name, file_position);

			sodr_tape_drive.status = failed != 0 ? so_drive_status_error : so_drive_status_positioning;
			db->ops->sync_drive(db, &sodr_tape_drive, true, so_database_sync_default);
		} else
			so_log_write(so_log_level_info,
				dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Positioning media '%s' to position #%d finished with status = OK"),
				sodr_tape_drive.vendor, sodr_tape_drive.model, sodr_tape_drive.index, sodr_tape_drive.slot->media->name, file_position);
	}

	if (failed != 0)
		failed = sodr_tape_drive_update_status(db);

	return failed;
}

static int sodr_tape_drive_update_status(struct so_database_connection * db) {
	sodr_time_start();
	int failed = ioctl(fd_nst, MTIOCGET, &status);
	sodr_time_stop(&sodr_tape_drive);

	unsigned int i;
	for (i = 0; i < 5 && failed != 0; i++) {
		sodr_tape_drive_on_failed(true, 20);

		sodr_time_start();
		failed = ioctl(fd_nst, MTIOCGET, &status);
		sodr_time_stop(&sodr_tape_drive);
	}

	if (failed != 0) {
		sodr_tape_drive.status = so_drive_status_error;

		static struct mtop reset = { MTRESET, 1 };
		sodr_time_start();
		failed = ioctl(fd_nst, MTIOCTOP, &reset);
		sodr_time_stop(&sodr_tape_drive);
	}

	/**
	 * Make sure that we have the correct status of tape
	 */
	while (!failed && GMT_ONLINE(status.mt_gstat)) {
		unsigned int density_code = ((status.mt_dsreg & MT_ST_DENSITY_MASK) >> MT_ST_DENSITY_SHIFT) & 0xFF;

		if (density_code != 0)
			break;

		// get the tape status again
		sodr_tape_drive_on_failed(true, 5);

		sodr_time_start();
		failed = ioctl(fd_nst, MTIOCGET, &status);
		sodr_time_stop(&sodr_tape_drive);
	}

	if (failed == 0) {
		bool is_empty = sodr_tape_drive.is_empty;

		if (GMT_ONLINE(status.mt_gstat)) {
			sodr_tape_drive.status = so_drive_status_loaded_idle;
			sodr_tape_drive.is_empty = false;
		} else {
			sodr_tape_drive.status = so_drive_status_empty_idle;
			sodr_tape_drive.is_empty = true;
		}

		if (sodr_tape_drive.slot->media == NULL && !sodr_tape_drive.is_empty) {
			char medium_serial_number[32];
			int fd = open(scsi_device, O_RDWR);
			failed = sodr_tape_drive_scsi_read_medium_serial_number(fd, medium_serial_number, 32);
			close(fd);

			if (failed == 0)
				sodr_tape_drive.slot->media = db->ops->get_media(db, medium_serial_number, NULL, NULL);

			if (sodr_tape_drive.slot->media == NULL) {
				sodr_tape_drive_create_media(db);

				struct so_media * media = sodr_tape_drive.slot->media;
				if (media != NULL && media->private_data != NULL) {
					struct sodr_tape_drive_media * mp = media->private_data;
					if (mp->format == sodr_tape_drive_media_ltfs) {
						int failed = sodr_tape_drive_media_parse_ltfs_label(&sodr_tape_drive, db);
						if (failed != 0)
							media->status = so_media_status_error;
						else {
							int fd = open(scsi_device, O_RDWR);
							failed = sodr_tape_drive_scsi_size_available(fd, media);
							close(fd);
						}
					}
				}
			}
		}

		if (sodr_tape_drive.slot->media != NULL) {
			struct so_media * media = sodr_tape_drive.slot->media;

			if (sodr_tape_drive.is_empty) {
				sodr_tape_drive_media_free(media->private_data);
				media->private_data = NULL;
			} else {
				int fd = open(scsi_device, O_RDWR);
				sodr_tape_drive_scsi_size_available(fd, media);
				if (media != NULL && media->media_format != NULL && media->media_format->support_mam)
					sodr_tape_drive_scsi_read_mam(fd, media);
				close(fd);

				media->write_lock = GMT_WR_PROT(status.mt_gstat);

				if (is_empty) {
					so_log_write(so_log_level_info,
						dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Checking media header '%s'"),
						sodr_tape_drive.vendor, sodr_tape_drive.model, sodr_tape_drive.index, media->name);

					// rewind tape
					int fd = open(scsi_device, O_RDWR);
					failed = sodr_tape_drive_scsi_rewind(fd);
					close(fd);

					if (failed == 0 && !sodr_tape_drive_check_header(db)) {
						media->status = so_media_status_error;
						so_log_write(so_log_level_error,
							dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Error while checking media header '%s'"),
							sodr_tape_drive.vendor, sodr_tape_drive.model, sodr_tape_drive.index, media->name);
						return 1;
					}
				}

				if (media->private_data != NULL) {
					struct sodr_tape_drive_media * mp = media->private_data;
					if (mp->format == sodr_tape_drive_media_ltfs) {
						if (mp->data.ltfs.files == NULL) {
							int failed = sodr_tape_drive_media_parse_ltfs_index(&sodr_tape_drive, db);
							if (failed != 0)
								media->status = so_media_status_error;
						}
					}
				}
			}
		}
	} else {
		sodr_tape_drive.status = so_drive_status_error;
	}

	return failed;
}

