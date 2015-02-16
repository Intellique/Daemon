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
*  Last modified: Wed, 11 Feb 2015 12:53:12 +0100                            *
\****************************************************************************/

// errno
#include <errno.h>
// json_*
#include <jansson.h>
// open
#include <fcntl.h>
// bool
#include <stdbool.h>
// free, malloc, realloc
#include <stdlib.h>
// memcpy, memmove, strdup
#include <string.h>
// bzero
#include <strings.h>
// ioctl
#include <sys/ioctl.h>
// struct mtget
#include <sys/mtio.h>
// open
#include <sys/stat.h>
// gettimeofday
#include <sys/time.h>
// open
#include <sys/types.h>
// time
#include <time.h>
// close, read, sleep
#include <unistd.h>

#include <libstone/database.h>
#include <libstone/format.h>
#include <libstone/io.h>
#include <libstone/library/archive.h>
#include <libstone/library/drive.h>
#include <libstone/library/media.h>
#include <libstone/library/ressource.h>
#include <libstone/log.h>

#include "common.h"
#include "scsi.h"


struct st_scsi_tape_drive_private {
	int fd_nst;
	bool used_by_io;
	bool allow_update_media;
	struct timeval last_start;
	struct mtget status;
};

struct st_scsi_tape_drive_io_reader {
	int fd;

	char * buffer;
	ssize_t block_size;
	char * buffer_pos;

	ssize_t position;
	bool end_of_file;

	int last_errno;

	struct st_drive * drive;
	struct st_media * media;
	struct st_scsi_tape_drive_private * drive_private;
};

struct st_scsi_tape_drive_io_writer {
	int fd;

	char * buffer;
	ssize_t buffer_used;
	ssize_t block_size;
	ssize_t position;

	bool eof_reached;

	int last_errno;

	struct st_drive * drive;
	struct st_media * media;
	struct st_scsi_tape_drive_private * drive_private;
};


static void st_scsi_tape_drive_create_media(struct st_drive * drive);
static int st_scsi_tape_drive_eject(struct st_drive * drive);
static int st_scsi_tape_drive_format(struct st_drive * drive, int file_position, bool quick_mode);
static void st_scsi_tape_drive_free(struct st_drive * drive);
static ssize_t st_scsi_tape_drive_get_block_size(struct st_drive * drive);
static int st_scsi_tape_drive_get_position(struct st_drive * drive);
static struct st_stream_reader * st_scsi_tape_drive_get_raw_reader(struct st_drive * drive, int file_position);
static struct st_stream_writer * st_scsi_tape_drive_get_raw_writer(struct st_drive * drive, bool append);
static struct st_format_reader * st_scsi_tape_drive_get_reader(struct st_drive * drive, int file_position, struct st_stream_reader * (*filter)(struct st_stream_reader * writer, void * param), void * param);
static struct st_format_writer * st_scsi_tape_drive_get_writer(struct st_drive * drive, bool append, struct st_stream_writer * (*filter)(struct st_stream_writer * writer, void * param), void * param);
static void st_scsi_tape_drive_on_failed(struct st_drive * drive, int verbose, int sleep_time);
static void st_scsi_tape_drive_operation_start(struct st_scsi_tape_drive_private * dr);
static void st_scsi_tape_drive_operation_stop(struct st_drive * dr);
static int st_scsi_tape_drive_rewind(struct st_drive * drive);
static int st_scsi_tape_drive_set_file_position(struct st_drive * drive, int file_position);
static int st_scsi_tape_drive_update_media_info(struct st_drive * drive);

static int st_scsi_tape_drive_io_reader_close(struct st_stream_reader * sr);
static bool st_scsi_tape_drive_io_reader_end_of_file(struct st_stream_reader * sr);
static off_t st_scsi_tape_drive_io_reader_forward(struct st_stream_reader * sr, off_t offset);
static void st_scsi_tape_drive_io_reader_free(struct st_stream_reader * sr);
static ssize_t st_scsi_tape_drive_io_reader_get_block_size(struct st_stream_reader * sr);
static int st_scsi_tape_drive_io_reader_last_errno(struct st_stream_reader * sr);
static struct st_stream_reader * st_scsi_tape_drive_io_reader_new(struct st_drive * drive);
static ssize_t st_scsi_tape_drive_io_reader_position(struct st_stream_reader * sr);
static ssize_t st_scsi_tape_drive_io_reader_read(struct st_stream_reader * sr, void * buffer, ssize_t length);

static ssize_t st_scsi_tape_drive_io_writer_before_close(struct st_stream_writer * sw, void * buffer, ssize_t length);
static int st_scsi_tape_drive_io_writer_close(struct st_stream_writer * sw);
static void st_scsi_tape_drive_io_writer_free(struct st_stream_writer * sw);
static ssize_t st_scsi_tape_drive_io_writer_get_available_size(struct st_stream_writer * sw);
static ssize_t st_scsi_tape_drive_io_writer_get_block_size(struct st_stream_writer * sw);
static int st_scsi_tape_drive_io_writer_last_errno(struct st_stream_writer * sw);
static struct st_stream_writer * st_scsi_tape_drive_io_writer_new(struct st_drive * drive);
static ssize_t st_scsi_tape_drive_io_writer_position(struct st_stream_writer * sw);
static struct st_stream_reader * st_scsi_tape_drive_io_writer_reopen(struct st_stream_writer * sw);
static ssize_t st_scsi_tape_drive_io_writer_write(struct st_stream_writer * sw, const void * buffer, ssize_t length);

static struct st_drive_ops st_scsi_tape_drive_ops = {
	.eject             = st_scsi_tape_drive_eject,
	.format            = st_scsi_tape_drive_format,
	.free              = st_scsi_tape_drive_free,
	.get_position      = st_scsi_tape_drive_get_position,
	.get_raw_reader    = st_scsi_tape_drive_get_raw_reader,
	.get_raw_writer    = st_scsi_tape_drive_get_raw_writer,
	.get_reader        = st_scsi_tape_drive_get_reader,
	.get_writer        = st_scsi_tape_drive_get_writer,
	.rewind            = st_scsi_tape_drive_rewind,
	.update_media_info = st_scsi_tape_drive_update_media_info,
};

static struct st_stream_reader_ops st_scsi_tape_drive_io_reader_ops = {
	.close          = st_scsi_tape_drive_io_reader_close,
	.end_of_file    = st_scsi_tape_drive_io_reader_end_of_file,
	.forward        = st_scsi_tape_drive_io_reader_forward,
	.free           = st_scsi_tape_drive_io_reader_free,
	.get_block_size = st_scsi_tape_drive_io_reader_get_block_size,
	.last_errno     = st_scsi_tape_drive_io_reader_last_errno,
	.position       = st_scsi_tape_drive_io_reader_position,
	.read           = st_scsi_tape_drive_io_reader_read,
};

static struct st_stream_writer_ops st_scsi_tape_drive_io_writer_ops = {
	.before_close       = st_scsi_tape_drive_io_writer_before_close,
	.close              = st_scsi_tape_drive_io_writer_close,
	.free               = st_scsi_tape_drive_io_writer_free,
	.get_available_size = st_scsi_tape_drive_io_writer_get_available_size,
	.get_block_size     = st_scsi_tape_drive_io_writer_get_block_size,
	.last_errno         = st_scsi_tape_drive_io_writer_last_errno,
	.position           = st_scsi_tape_drive_io_writer_position,
	.reopen             = st_scsi_tape_drive_io_writer_reopen,
	.write              = st_scsi_tape_drive_io_writer_write,
};


static void st_scsi_tape_drive_create_media(struct st_drive * drive) {
	struct st_scsi_tape_drive_private * self = drive->data;

	struct st_media * media = malloc(sizeof(struct st_media));
	bzero(media, sizeof(struct st_media));
	drive->slot->media = media;

	if (drive->slot->volume_name != NULL) {
		media->label = strdup(drive->slot->volume_name);
		media->name = strdup(media->label);
	}

	media->format = st_media_format_get_by_density_code((self->status.mt_dsreg & MT_ST_DENSITY_MASK) >> MT_ST_DENSITY_SHIFT, st_media_format_mode_linear);

	media->status = st_media_status_foreign;
	media->location = st_media_location_indrive;
	media->first_used = time(NULL);
	media->use_before = media->first_used + media->format->life_span;

	media->load_count = 1;

	media->block_size = st_scsi_tape_drive_get_block_size(drive);

	media->lock = st_ressource_new(true);
	media->locked = false;

	if (media->format != NULL && media->format->support_mam) {
		int fd = open(drive->scsi_device, O_RDWR);
		int failed = st_scsi_tape_read_mam(fd, media);
		close(fd);

		if (failed)
			st_log_write_all(st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: failed to read medium axilary memory", drive->vendor, drive->model, drive - drive->changer->drives);
	}

	st_media_read_header(drive);

	st_media_add(media);

	if (media->status == st_media_status_in_use) {
		ssize_t buffer_size = 10 * media->block_size;
		char * buffer = malloc(10 * media->block_size);

		struct st_database * db = st_database_get_default_driver();
		struct st_database_config * config = NULL;
		struct st_database_connection * connection = NULL;

		if (db != NULL)
			config = db->ops->get_default_config();
		if (config != NULL)
			connection = config->ops->connect(config);

		connection->ops->sync_media(connection, media);

		unsigned int i;
		for (i = 1;; i++) {
			struct st_stream_reader * reader = drive->ops->get_raw_reader(drive, i);
			if (reader == NULL)
				break;

			bool ok = false;
			ssize_t position = 0;
			json_t * meta = NULL;
			while (!ok) {
				ssize_t nb_read = reader->ops->read(reader, buffer + position, buffer_size - position);

				if (nb_read <= 0)
					break;

				if (nb_read > 0)
					position += nb_read;

				json_error_t error;
				meta = json_loads(buffer, 0, &error);

				if (meta != NULL) {
					ok = true;
				} else if (error.position > media->block_size / 2) {
					buffer_size <<= 1;
					void * addr = realloc(buffer, buffer_size);
					if (addr == NULL)
						break;
					buffer = addr;
				} else {
					break;
				}
			}

			reader->ops->close(reader);
			reader->ops->free(reader);

			if (ok) {
				json_t * jarchive = json_object_get(meta, "archive");
				struct st_archive * archive = st_archive_parse(jarchive, i - 1);
				if (archive != NULL) {
					connection->ops->sync_archive(connection, archive);
					st_archive_free(archive);
				}
			}
		}

		connection->ops->free(connection);

		free(buffer);
	}

	drive->slot->media = media;
}

static int st_scsi_tape_drive_eject(struct st_drive * drive) {
	if (drive == NULL || !drive->enabled)
		return 1;

	struct st_scsi_tape_drive_private * self = drive->data;

	st_log_write_all(st_log_level_info, st_log_type_drive, "[%s | %s | #%td]: rewind tape and put the drive offline", drive->vendor, drive->model, drive - drive->changer->drives);
	drive->status = st_drive_unloading;

	static struct mtop eject = { MTOFFL, 1 };
	st_scsi_tape_drive_operation_start(self);
	int failed = ioctl(self->fd_nst, MTIOCTOP, &eject);
	st_scsi_tape_drive_operation_stop(drive);

	if (!failed)
		drive->slot->media->operation_count++;

	drive->status = failed ? st_drive_error : st_drive_empty_idle;
	st_log_write_all(failed ? st_log_level_error : st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: rewind tape and put the drive offline, finish with code = %d", drive->vendor, drive->model, drive - drive->changer->drives, failed);

	st_scsi_tape_drive_update_media_info(drive);

	return failed;
}

static int st_scsi_tape_drive_format(struct st_drive * drive, int file_position, bool quick_mode) {
	if (drive == NULL || !drive->enabled)
		return 1;

	int failed = st_scsi_tape_drive_set_file_position(drive, file_position);

	struct st_scsi_tape_drive_private * self = drive->data;

	int fd = -1;
	if (!failed)
		fd = open(drive->scsi_device, O_RDWR);

	if (!failed && fd > -1) {
		drive->status = st_drive_erasing;
		st_scsi_tape_drive_operation_start(self);
		failed = st_scsi_tape_format(fd, quick_mode);
		st_scsi_tape_drive_operation_stop(drive);

		if (!failed)
			drive->slot->media->operation_count++;

		drive->status = st_drive_loaded_idle;

		close(fd);
	}

	return failed;
}

static void st_scsi_tape_drive_free(struct st_drive * dr) {
	struct st_scsi_tape_drive_private * self = dr->data;
	close(self->fd_nst);
	free(self);

	free(dr->device);
	free(dr->scsi_device);

	free(dr->model);
	free(dr->vendor);
	free(dr->revision);
	free(dr->serial_number);

	dr->lock->ops->free(dr->lock);

	dr->data = NULL;
	free(dr->db_data);
	dr->db_data = NULL;
}

static ssize_t st_scsi_tape_drive_get_block_size(struct st_drive * drive) {
	struct st_scsi_tape_drive_private * self = drive->data;

	ssize_t block_size = (self->status.mt_dsreg & MT_ST_BLKSIZE_MASK) >> MT_ST_BLKSIZE_SHIFT;
	if (block_size > 0)
		return block_size;

	struct st_media * media = drive->slot->media;
	if (drive->slot->media && media->block_size > 0)
		return media->block_size;

	drive->status = st_drive_rewinding;

	static struct mtop rewind = { MTREW, 1 };
	st_scsi_tape_drive_operation_start(self);
	int failed = ioctl(self->fd_nst, MTIOCTOP, &rewind);
	st_scsi_tape_drive_operation_stop(drive);

	drive->status = st_drive_loaded_idle;

	unsigned int i;
	ssize_t nb_read;
	block_size = 1 << 16;
	char * buffer = malloc(block_size);
	for (i = 0; i < 4 && buffer != NULL && !failed; i++) {
		drive->status = st_drive_reading;

		st_scsi_tape_drive_operation_start(self);
		nb_read = read(self->fd_nst, buffer, block_size);
		st_scsi_tape_drive_operation_stop(drive);

		drive->status = st_drive_loaded_idle;

		st_scsi_tape_drive_operation_start(self);
		failed = ioctl(self->fd_nst, MTIOCGET, &self->status);
		st_scsi_tape_drive_operation_stop(drive);

		if (!failed && self->status.mt_blkno < 1)
			break;

		if (media != NULL)
			media->read_count++;

		if (nb_read > 0) {
			st_log_write_all(st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: found block size: %zd", drive->vendor, drive->model, drive - drive->changer->drives, nb_read);

			free(buffer);
			return nb_read;
		}

		drive->status = st_drive_rewinding;

		st_scsi_tape_drive_operation_start(self);
		failed = ioctl(self->fd_nst, MTIOCTOP, &rewind);
		st_scsi_tape_drive_operation_stop(drive);

		drive->status = st_drive_loaded_idle;

		if (!failed) {
			block_size <<= 1;
			void * new_addr = realloc(buffer, block_size);
			if (new_addr == NULL)
				free(buffer);
			buffer = new_addr;
		} else
			break;
	}

	free(buffer);

	if (media != NULL && media->format != NULL)
		return media->format->block_size;

	return -1;
}

static int st_scsi_tape_drive_get_position(struct st_drive * drive) {
	struct st_scsi_tape_drive_private * self = drive->data;
	return self->status.mt_fileno;
}

static struct st_stream_reader * st_scsi_tape_drive_get_raw_reader(struct st_drive * drive, int file_position) {
	if (drive == NULL || !drive->enabled)
		return NULL;

	if (st_scsi_tape_drive_set_file_position(drive, file_position))
		return NULL;

	struct st_scsi_tape_drive_private * self = drive->data;
	if (drive->is_empty || self->used_by_io) {
		st_log_write_all(st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: drive is not free for reading on it", drive->vendor, drive->model, drive - drive->changer->drives);
		return NULL;
	}

	drive->status = st_drive_reading;
	drive->slot->media->read_count++;
	st_log_write_all(st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: drive is open for reading", drive->vendor, drive->model, drive - drive->changer->drives);

	return st_scsi_tape_drive_io_reader_new(drive);
}

static struct st_stream_writer * st_scsi_tape_drive_get_raw_writer(struct st_drive * drive, bool append) {
	if (drive == NULL || !drive->enabled)
		return NULL;

	if (append && st_scsi_tape_drive_set_file_position(drive, -1))
		return NULL;

	struct st_scsi_tape_drive_private * self = drive->data;
	if (drive->is_empty || self->used_by_io) {
		st_log_write_all(st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: drive is not free for writing on it", drive->vendor, drive->model, drive - drive->changer->drives);
		return NULL;
	}

	drive->status = st_drive_writing;
	drive->slot->media->write_count++;
	st_log_write_all(st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: drive is open for writing", drive->vendor, drive->model, drive - drive->changer->drives);

	return st_scsi_tape_drive_io_writer_new(drive);
}

static struct st_format_reader * st_scsi_tape_drive_get_reader(struct st_drive * drive, int file_position, struct st_stream_reader * (*filter)(struct st_stream_reader * writer, void * param), void * param) {
	struct st_stream_reader * reader = st_scsi_tape_drive_get_raw_reader(drive, file_position);
	if (reader != NULL) {
		if (filter != NULL) {
			struct st_stream_reader * tmp_io = filter(reader, param);
			if (tmp_io != NULL)
				reader = tmp_io;
			else {
				reader->ops->free(reader);
				return NULL;
			}
		}

		return st_format_get_reader(reader, drive->slot->media->format);
	}
	return NULL;
}

static struct st_format_writer * st_scsi_tape_drive_get_writer(struct st_drive * drive, bool append, struct st_stream_writer * (*filter)(struct st_stream_writer * writer, void * param), void * param) {
	struct st_stream_writer * writer = st_scsi_tape_drive_get_raw_writer(drive, append);
	if (writer != NULL) {
		if (filter != NULL) {
			struct st_stream_writer * tmp_io = filter(writer, param);
			if (tmp_io != NULL)
				writer = tmp_io;
			else {
				writer->ops->free(writer);
				return NULL;
			}
		}

		return st_format_get_writer(writer, drive->slot->media->format);
	}
	return NULL;
}

void st_scsi_tape_drive_setup(struct st_drive * drive) {
	int fd = open(drive->device, O_RDWR | O_NONBLOCK);

	struct st_scsi_tape_drive_private * self = malloc(sizeof(struct st_scsi_tape_drive_private));
	bzero(self, sizeof(struct st_scsi_tape_drive_private));
	self->fd_nst = fd;
	self->used_by_io = false;
	self->allow_update_media = true;

	drive->mode = st_media_format_mode_linear;
	drive->ops = &st_scsi_tape_drive_ops;
	drive->data = self;
	drive->operation_duration = 0;
	drive->last_clean = 0;

	drive->lock = st_ressource_new(false);

	struct mtget status;
	st_scsi_tape_drive_operation_start(self);
	int failed = ioctl(self->fd_nst, MTIOCGET, &status);
	st_scsi_tape_drive_operation_stop(drive);

	if (!failed) {
		if (GMT_ONLINE(status.mt_gstat)) {
			drive->status = st_drive_loaded_idle;
			drive->is_empty = false;
		} else {
			drive->status = st_drive_empty_idle;
			drive->is_empty = true;
		}
	}
}

static void st_scsi_tape_drive_operation_start(struct st_scsi_tape_drive_private * dr) {
	gettimeofday(&dr->last_start, NULL);
}

static void st_scsi_tape_drive_operation_stop(struct st_drive * drive) {
	struct st_scsi_tape_drive_private * self = drive->data;

	struct timeval finish;
	gettimeofday(&finish, NULL);

	drive->operation_duration += (finish.tv_sec - self->last_start.tv_sec) + ((double) (finish.tv_usec - self->last_start.tv_usec)) / 1000000;
}

static void st_scsi_tape_drive_on_failed(struct st_drive * drive, int verbose, int sleep_time) {
	struct st_scsi_tape_drive_private * self = drive->data;

	if (verbose)
		st_log_write_all(st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: Try to recover an error", drive->vendor, drive->model, drive - drive->changer->drives);
	close(self->fd_nst);
	sleep(sleep_time);
	self->fd_nst = open(drive->device, O_RDWR | O_NONBLOCK);
}

static int st_scsi_tape_drive_rewind(struct st_drive * drive) {
	return st_scsi_tape_drive_set_file_position(drive, 0);
}

static int st_scsi_tape_drive_set_file_position(struct st_drive * drive, int file_position) {
	struct st_scsi_tape_drive_private * self = drive->data;

	int failed;
	if (file_position < 0) {
		st_log_write_all(st_log_level_info, st_log_type_drive, "[%s | %s | #%td]: Fast forwarding to end of media", drive->vendor, drive->model, drive - drive->changer->drives);
		drive->status = st_drive_positioning;

		static struct mtop eod = { MTEOM, 1 };
		st_scsi_tape_drive_operation_start(self);
		failed = ioctl(self->fd_nst, MTIOCTOP, &eod);
		st_scsi_tape_drive_operation_stop(drive);
		if (failed)
			st_log_write_all(st_log_level_error, st_log_type_drive, "[%s | %s | #%td]: Fast forwarding to end of media finished with error (%m)", drive->vendor, drive->model, drive - drive->changer->drives);
		else
			st_log_write_all(st_log_level_info, st_log_type_drive, "[%s | %s | #%td]: Fast forwarding to end of media finished with status = OK", drive->vendor, drive->model, drive - drive->changer->drives);

		drive->status = failed ? st_drive_error : st_drive_positioning;
	} else {
		st_log_write_all(st_log_level_info, st_log_type_drive, "[%s | %s | #%td]: Positioning media to position #%d", drive->vendor, drive->model, drive - drive->changer->drives, file_position);
		drive->status = st_drive_rewinding;

		static struct mtop rewind = { MTREW, 1 };
		st_scsi_tape_drive_operation_start(self);
		failed = ioctl(self->fd_nst, MTIOCTOP, &rewind);
		st_scsi_tape_drive_operation_stop(drive);

		if (failed) {
			drive->status = st_drive_error;
			st_log_write_all(st_log_level_error, st_log_type_drive, "[%s | %s | #%td]: Positioning media to position #%d finished with error (%m)", drive->vendor, drive->model, drive - drive->changer->drives, file_position);
			return failed;
		}

		if (file_position > 0) {
			drive->status = st_drive_positioning;

			struct mtop fsr = { MTFSF, file_position };
			st_scsi_tape_drive_operation_start(self);
			failed = ioctl(self->fd_nst, MTIOCTOP, &fsr);
			st_scsi_tape_drive_operation_stop(drive);

			if (failed)
				st_log_write_all(st_log_level_error, st_log_type_drive, "[%s | %s | #%td]: Positioning media to position #%d finished with error (%m)", drive->vendor, drive->model, drive - drive->changer->drives, file_position);
			else
				st_log_write_all(st_log_level_info, st_log_type_drive, "[%s | %s | #%td]: Positioning media to position #%d finished with status = OK", drive->vendor, drive->model, drive - drive->changer->drives, file_position);

			drive->status = failed ? st_drive_error : st_drive_positioning;
		} else
			st_log_write_all(st_log_level_info, st_log_type_drive, "[%s | %s | #%td]: Positioning media to position #%d finished with status = OK", drive->vendor, drive->model, drive - drive->changer->drives, file_position);
	}

	if (!failed)
		st_scsi_tape_drive_update_media_info(drive);

	return failed;
}

static int st_scsi_tape_drive_update_media_info(struct st_drive * drive) {
	if (drive == NULL || !drive->enabled)
		return 1;

	struct st_scsi_tape_drive_private * self = drive->data;

	if (!self->allow_update_media)
		return 0;
	self->allow_update_media = false;

	st_scsi_tape_drive_on_failed(drive, 0, 1);

	st_scsi_tape_drive_operation_start(self);
	int failed = ioctl(self->fd_nst, MTIOCGET, &self->status);
	st_scsi_tape_drive_operation_stop(drive);

	unsigned int i;
	for (i = 0; i < 5 && failed; i++) {
		st_scsi_tape_drive_on_failed(drive, 1, 20);
		st_scsi_tape_drive_operation_start(self);
		failed = ioctl(self->fd_nst, MTIOCGET, &self->status);
		st_scsi_tape_drive_operation_stop(drive);
	}

	if (failed) {
		drive->status = st_drive_error;

		static struct mtop reset = { MTRESET, 1 };
		st_scsi_tape_drive_operation_start(self);
		failed = ioctl(self->fd_nst, MTIOCTOP, &reset);
		st_scsi_tape_drive_operation_stop(drive);

		if (!failed) {
			st_scsi_tape_drive_operation_start(self);
			failed = ioctl(self->fd_nst, MTIOCGET, &self->status);
			st_scsi_tape_drive_operation_stop(drive);
		}
	}

	/**
	 * Make sure that we have the correct status of tape
	 */
	while (!failed && GMT_ONLINE(self->status.mt_gstat)) {
		unsigned char density_code = (self->status.mt_dsreg & MT_ST_DENSITY_MASK) >> MT_ST_DENSITY_SHIFT;

		if (density_code != 0)
			break;

		// get the tape status again
		st_scsi_tape_drive_on_failed(drive, 1, 20);
		st_scsi_tape_drive_operation_start(self);
		failed = ioctl(self->fd_nst, MTIOCGET, &self->status);
		st_scsi_tape_drive_operation_stop(drive);
	}

	if (!failed) {
		bool is_empty = drive->is_empty;

		if (GMT_ONLINE(self->status.mt_gstat)) {
			drive->status = st_drive_loaded_idle;
			drive->is_empty = false;

			unsigned char density_code = (self->status.mt_dsreg & MT_ST_DENSITY_MASK) >> MT_ST_DENSITY_SHIFT;
			if (drive->density_code < density_code)
				drive->density_code = density_code;
		} else {
			drive->status = st_drive_empty_idle;
			drive->is_empty = true;
		}

		if (drive->slot->media == NULL && !drive->is_empty) {
			char medium_serial_number[32];
			int fd = open(drive->scsi_device, O_RDWR);

			failed = st_scsi_tape_read_medium_serial_number(fd, medium_serial_number, 32);

			close(fd);

			if (!failed)
				drive->slot->media = st_media_get_by_medium_serial_number(medium_serial_number);

			if (drive->slot->media == NULL)
				st_scsi_tape_drive_create_media(drive);
		}

		if (drive->slot->media != NULL && !drive->is_empty) {
			struct st_media * media = drive->slot->media;

			int fd = open(drive->scsi_device, O_RDWR);
			st_scsi_tape_size_available(fd, media);
			close(fd);

			media->type = GMT_WR_PROT(self->status.mt_gstat) ? st_media_type_readonly : st_media_type_rewritable;

			if (is_empty && media->status == st_media_status_in_use) {
				st_log_write_all(st_log_level_info, st_log_type_drive, "[%s | %s | #%td]: Checking media header", drive->vendor, drive->model, drive - drive->changer->drives);
				if (!st_media_check_header(drive)) {
					media->status = st_media_status_error;
					st_log_write_all(st_log_level_error, st_log_type_drive, "[%s | %s | #%td]: Error while checking media header", drive->vendor, drive->model, drive - drive->changer->drives);
					self->allow_update_media = true;
					return 1;
				}
			}
		}
	} else {
		drive->status = st_drive_error;
	}

	self->allow_update_media = true;
	return failed;
}


static int st_scsi_tape_drive_io_reader_close(struct st_stream_reader * sr) {
	if (sr == NULL || !sr->data)
		return -1;

	struct st_scsi_tape_drive_io_reader * self = sr->data;

	if (self->fd > -1) {
		self->fd = -1;
		self->buffer_pos = self->buffer + self->block_size;
		self->last_errno = 0;

		self->drive->status = st_drive_loaded_idle;
		self->media->last_read = time(NULL);

		self->drive_private->used_by_io = false;
		st_log_write_all(st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: drive is close", self->drive->vendor, self->drive->model, self->drive - self->drive->changer->drives);
	}

	return 0;
}

static bool st_scsi_tape_drive_io_reader_end_of_file(struct st_stream_reader * sr) {
	if (sr == NULL)
		return true;

	struct st_scsi_tape_drive_io_reader * self = sr->data;
	return self->end_of_file;
}

static off_t st_scsi_tape_drive_io_reader_forward(struct st_stream_reader * sr, off_t offset) {
	if (sr == NULL)
		return -1;

	struct st_scsi_tape_drive_io_reader * self = sr->data;
	self->last_errno = 0;

	if (self->fd < 0)
		return -1;

	ssize_t nb_total_read = self->block_size - (self->buffer_pos - self->buffer);
	if (nb_total_read > 0) {
		ssize_t will_move = nb_total_read > offset ? offset : nb_total_read;

		self->buffer_pos += will_move;
		self->position += will_move;

		if (will_move == offset)
			return self->position;
	}

	if (offset - nb_total_read >= self->block_size) {
		/**
		 * There is a limitation with scsi command 'space' used by linux driver st.
		 * With this command block_number is specified into 3 bytes so 8388607 is the
		 * maximum that we can forward in one time.
		 */
		long nb_records = (offset - nb_total_read) / self->block_size;
		while (nb_records > 0) {
			int next_forward = nb_records > 8388607 ? 8388607 : nb_records;

			st_log_write_all(st_log_level_info, st_log_type_drive, "[%s | %s | #%td]: forward (#record %ld)", self->drive->vendor, self->drive->model, self->drive - self->drive->changer->drives, nb_records);

			struct mtop forward = { MTFSR, next_forward };
			st_scsi_tape_drive_operation_start(self->drive_private);
			int failed = ioctl(self->fd, MTIOCTOP, &forward);
			st_scsi_tape_drive_operation_stop(self->drive);

			if (failed) {
				self->last_errno = errno;
				return failed;
			}

			nb_records -= next_forward;
			self->position += next_forward * self->block_size;
			nb_total_read += next_forward * self->block_size;
		}
	}

	if (nb_total_read == offset)
		return self->position;

	st_scsi_tape_drive_operation_start(self->drive_private);
	ssize_t nb_read = read(self->fd, self->buffer, self->block_size);
	st_scsi_tape_drive_operation_stop(self->drive);

	if (nb_read < 0) {
		self->last_errno = errno;
		self->media->nb_read_errors++;
		return nb_read;
	} else if (nb_read > 0) {
		ssize_t will_move = offset - nb_total_read;
		self->buffer_pos = self->buffer + will_move;
		self->position += will_move;
		self->media->nb_total_read++;
	}

	return self->position;
}

static void st_scsi_tape_drive_io_reader_free(struct st_stream_reader * sr) {
	if (sr == NULL)
		return;

	struct st_scsi_tape_drive_io_reader * self = sr->data;
	if (self) {
		self->fd = -1;
		free(self->buffer);
		free(self);
	}
	sr->data = NULL;
	sr->ops = NULL;

	free(sr);
}

static ssize_t st_scsi_tape_drive_io_reader_get_block_size(struct st_stream_reader * sr) {
	struct st_scsi_tape_drive_io_reader * self = sr->data;
	return self->block_size;
}

static int st_scsi_tape_drive_io_reader_last_errno(struct st_stream_reader * sr) {
	struct st_scsi_tape_drive_io_reader * self = sr->data;
	return self->last_errno;
}

static struct st_stream_reader * st_scsi_tape_drive_io_reader_new(struct st_drive * drive) {
	ssize_t block_size = st_scsi_tape_drive_get_block_size(drive);

	struct st_scsi_tape_drive_private * dr = drive->data;
	struct st_scsi_tape_drive_io_reader * self = malloc(sizeof(struct st_scsi_tape_drive_io_reader));

	self->fd = dr->fd_nst;

	self->buffer = malloc(block_size);
	self->block_size = block_size;
	self->buffer_pos = self->buffer + block_size;

	self->position = 0;
	self->last_errno = 0;
	self->end_of_file = false;

	self->drive = drive;
	self->media = drive->slot->media;
	self->drive_private = dr;

	dr->used_by_io = true;

	struct st_stream_reader * reader = malloc(sizeof(struct st_stream_reader));
	reader->ops = &st_scsi_tape_drive_io_reader_ops;
	reader->data = self;

	self->media->read_count++;
	self->media->last_read = time(NULL);

	return reader;
}

static ssize_t st_scsi_tape_drive_io_reader_position(struct st_stream_reader * sr) {
	if (sr == NULL || sr->data == NULL)
		return -1;

	struct st_scsi_tape_drive_io_reader * self = sr->data;
	return self->position;
}

static ssize_t st_scsi_tape_drive_io_reader_read(struct st_stream_reader * sr, void * buffer, ssize_t length) {
	if (sr == NULL || buffer == NULL || length < 0)
		return -1;

	struct st_scsi_tape_drive_io_reader * self = sr->data;
	self->last_errno = 0;

	if (self->fd < 0)
		return -1;

	if (self->end_of_file || length < 1)
		return 0;

	ssize_t nb_total_read = self->block_size - (self->buffer_pos - self->buffer);
	if (nb_total_read > 0) {
		ssize_t will_copy = nb_total_read > length ? length : nb_total_read;
		memcpy(buffer, self->buffer_pos, will_copy);

		self->buffer_pos += will_copy;
		self->position += will_copy;

		if (will_copy == length)
			return length;
	}

	char * c_buffer = buffer;
	while (length - nb_total_read >= self->block_size) {
		st_scsi_tape_drive_operation_start(self->drive_private);
		ssize_t nb_read = read(self->fd, c_buffer + nb_total_read, self->block_size);
		st_scsi_tape_drive_operation_stop(self->drive);

		if (nb_read < 0) {
			self->last_errno = errno;
			self->media->nb_read_errors++;
			return nb_read;
		} else if (nb_read > 0) {
			nb_total_read += nb_read;
			self->position += nb_read;
			self->media->nb_total_read++;
		} else {
			self->end_of_file = true;
			return nb_total_read;
		}
	}

	if (nb_total_read == length)
		return length;

	st_scsi_tape_drive_operation_start(self->drive_private);
	ssize_t nb_read = read(self->fd, self->buffer, self->block_size);
	st_scsi_tape_drive_operation_stop(self->drive);

	if (nb_read < 0) {
		self->last_errno = errno;
		self->media->nb_read_errors++;
		return nb_read;
	} else if (nb_read > 0) {
		ssize_t will_copy = length - nb_total_read;
		memcpy(c_buffer + nb_total_read, self->buffer, will_copy);
		self->buffer_pos = self->buffer + will_copy;
		self->position += will_copy;
		self->media->nb_total_read++;
		return length;
	} else {
		self->end_of_file = true;
		return nb_total_read;
	}
}


static ssize_t st_scsi_tape_drive_io_writer_before_close(struct st_stream_writer * sw, void * buffer, ssize_t length) {
	if (sw == NULL || buffer == NULL || length < 0)
		return -1;

	struct st_scsi_tape_drive_io_writer * self = sw->data;
	if (self->buffer_used > 0 && self->buffer_used < self->block_size) {
		ssize_t will_copy = self->block_size - self->buffer_used;
		if (will_copy > length)
			will_copy = length;

		bzero(buffer, will_copy);

		bzero(self->buffer + self->buffer_used, will_copy);
		self->buffer_used += will_copy;
		self->position += will_copy;

		if (self->buffer_used == self->block_size) {
			st_scsi_tape_drive_operation_start(self->drive_private);
			ssize_t nb_write = write(self->fd, self->buffer, self->block_size);
			st_scsi_tape_drive_operation_stop(self->drive);

			if (nb_write < 0) {
				switch (errno) {
					case ENOSPC:
						st_scsi_tape_drive_operation_start(self->drive_private);
						nb_write = write(self->fd, self->buffer, self->block_size);
						st_scsi_tape_drive_operation_stop(self->drive);

						if (nb_write == self->block_size)
							break;

					default:
						self->last_errno = errno;
						self->media->nb_write_errors++;
						return -1;
				}
			}

			self->buffer_used = 0;
			self->media->nb_total_write++;
			if (self->media->free_block > 0)
				self->media->free_block--;
		}

		return will_copy;
	}

	return 0;
}

static int st_scsi_tape_drive_io_writer_close(struct st_stream_writer * sw) {
	if (sw == NULL || sw->data == NULL)
		return -1;

	struct st_scsi_tape_drive_io_writer * self = sw->data;
	self->last_errno = 0;

	if (self->buffer_used > 0) {
		bzero(self->buffer + self->buffer_used, self->block_size - self->buffer_used);

		st_scsi_tape_drive_operation_start(self->drive_private);
		ssize_t nb_write = write(self->fd, self->buffer, self->block_size);
		st_scsi_tape_drive_operation_stop(self->drive);

		if (nb_write < 0) {
			switch (errno) {
				case ENOSPC:
					st_scsi_tape_drive_operation_start(self->drive_private);
					nb_write = write(self->fd, self->buffer, self->block_size);
					st_scsi_tape_drive_operation_stop(self->drive);

					if (nb_write == self->block_size)
						break;

				default:
					self->last_errno = errno;
					self->media->nb_write_errors++;
					return -1;
			}
		}

		self->position += nb_write;
		self->buffer_used = 0;
		self->media->nb_total_write++;
		if (self->media->free_block > 0)
			self->media->free_block--;
	}

	if (self->fd > -1) {
		static struct mtop eof = { MTWEOF, 1 };
		st_scsi_tape_drive_operation_start(self->drive_private);
		int failed = ioctl(self->fd, MTIOCTOP, &eof);
		st_scsi_tape_drive_operation_stop(self->drive);

		self->media->last_write = time(NULL);

		if (failed != 0) {
			self->last_errno = errno;
			self->media->nb_write_errors++;
			return -1;
		}

		self->drive->status = st_drive_loaded_idle;

		st_scsi_tape_drive_update_media_info(self->drive);
		self->drive->slot->media->nb_volumes = self->drive_private->status.mt_fileno;
		st_scsi_tape_drive_update_media_info(self->drive);

		self->fd = -1;
		self->drive_private->used_by_io = false;
		st_log_write_all(st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: drive is close", self->drive->vendor, self->drive->model, self->drive - self->drive->changer->drives);
	}

	return 0;
}

static void st_scsi_tape_drive_io_writer_free(struct st_stream_writer * sw) {
	if (sw == NULL)
		return;

	struct st_scsi_tape_drive_io_writer * self = sw->data;
	if (self) {
		self->fd = -1;
		free(self->buffer);
		free(self);
	}
	sw->data = NULL;
	sw->ops = NULL;

	free(sw);
}

static ssize_t st_scsi_tape_drive_io_writer_get_available_size(struct st_stream_writer * sw) {
	struct st_scsi_tape_drive_io_writer * self = sw->data;
	struct st_media * media = self->drive->slot->media;

	if (media == NULL)
		return 0;

	return media->free_block * media->block_size;
}

static ssize_t st_scsi_tape_drive_io_writer_get_block_size(struct st_stream_writer * sw) {
	struct st_scsi_tape_drive_io_writer * self = sw->data;
	return self->block_size;
}

static int st_scsi_tape_drive_io_writer_last_errno(struct st_stream_writer * sw) {
	struct st_scsi_tape_drive_io_writer * self = sw->data;
	return self->last_errno;
}

static struct st_stream_writer * st_scsi_tape_drive_io_writer_new(struct st_drive * drive) {
	ssize_t block_size = st_scsi_tape_drive_get_block_size(drive);

	struct st_scsi_tape_drive_private * dr = drive->data;
	struct st_scsi_tape_drive_io_writer * self = malloc(sizeof(struct st_scsi_tape_drive_io_reader));
	self->fd = dr->fd_nst;
	self->buffer = malloc(block_size);
	self->buffer_used = 0;
	self->block_size = block_size;
	self->position = 0;
	self->eof_reached = false;
	self->last_errno = 0;

	self->drive = drive;
	self->media = drive->slot->media;
	self->drive_private = dr;

	dr->used_by_io = true;

	struct st_stream_writer * writer = malloc(sizeof(struct st_stream_writer));
	writer->ops = &st_scsi_tape_drive_io_writer_ops;
	writer->data = self;

	self->media->write_count++;
	self->media->last_write = time(NULL);

	return writer;
}

static ssize_t st_scsi_tape_drive_io_writer_position(struct st_stream_writer * sw) {
	if (sw == NULL || sw->data == NULL)
		return -1;

	struct st_scsi_tape_drive_io_writer * self = sw->data;
	return self->position;
}

static struct st_stream_reader * st_scsi_tape_drive_io_writer_reopen(struct st_stream_writer * sw) {
	if (sw == NULL || sw->data == NULL)
		return NULL;

	struct st_scsi_tape_drive_io_writer * self = sw->data;
	st_scsi_tape_drive_update_media_info(self->drive);
	int position = self->drive_private->status.mt_fileno;

	st_scsi_tape_drive_io_writer_close(sw);

	return st_scsi_tape_drive_get_raw_reader(self->drive, position);
}

static ssize_t st_scsi_tape_drive_io_writer_write(struct st_stream_writer * sw, const void * buffer, ssize_t length) {
	if (sw == NULL || buffer == NULL || length < 0)
		return -1;

	struct st_scsi_tape_drive_io_writer * self = sw->data;
	self->last_errno = 0;

	ssize_t buffer_available = self->block_size - self->buffer_used;
	if (buffer_available > length) {
		memcpy(self->buffer + self->buffer_used, buffer, length);

		self->buffer_used += length;
		self->position += length;
		return length;
	}

	memcpy(self->buffer + self->buffer_used, buffer, buffer_available);

	st_scsi_tape_drive_operation_start(self->drive_private);
	ssize_t nb_write = write(self->fd, self->buffer, self->block_size);
	st_scsi_tape_drive_operation_stop(self->drive);

	if (nb_write < 0) {
		switch (errno) {
			case ENOSPC:
				self->media->free_block = self->eof_reached ? 0 : 1;
				self->eof_reached = true;

				st_scsi_tape_drive_operation_start(self->drive_private);
				nb_write = write(self->fd, self->buffer, self->block_size);
				st_scsi_tape_drive_operation_stop(self->drive);

				if (nb_write == self->block_size)
					break;

			default:
				self->last_errno = errno;
				self->media->nb_write_errors++;
				return -1;
		}
	}

	ssize_t nb_total_write = buffer_available;
	self->buffer_used = 0;
	self->position += buffer_available;
	self->media->nb_total_write++;
	if (self->media->free_block > 0)
		self->media->free_block--;

	const char * c_buffer = buffer;
	while (length - nb_total_write >= self->block_size) {
		st_scsi_tape_drive_operation_start(self->drive_private);
		nb_write = write(self->fd, c_buffer + nb_total_write, self->block_size);
		st_scsi_tape_drive_operation_stop(self->drive);

		if (nb_write < 0) {
			switch (errno) {
				case ENOSPC:
					self->drive->slot->media->free_block = self->eof_reached ? 0 : 1;
					self->eof_reached = true;

					st_scsi_tape_drive_operation_start(self->drive_private);
					nb_write = write(self->fd, self->buffer, self->block_size);
					st_scsi_tape_drive_operation_stop(self->drive);

					if (nb_write == self->block_size)
						break;

				default:
					self->last_errno = errno;
					self->media->nb_write_errors++;
					return -1;
			}
		}

		nb_total_write += nb_write;
		self->position += nb_write;
		self->media->nb_total_write++;
		if (self->media->free_block > 0)
			self->media->free_block--;
	}

	if (length == nb_total_write)
		return length;

	self->buffer_used = length - nb_total_write;
	self->position += self->buffer_used;
	memcpy(self->buffer, c_buffer + nb_total_write, self->buffer_used);

	return length;
}

