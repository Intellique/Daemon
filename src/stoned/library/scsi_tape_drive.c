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
*  Copyright (C) 2012, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Sat, 18 Aug 2012 17:04:09 +0200                         *
\*************************************************************************/

// errno
#include <errno.h>
// open
#include <fcntl.h>
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

#include <libstone/io.h>
#include <libstone/library/drive.h>
#include <libstone/library/media.h>
#include <libstone/library/ressource.h>
#include <libstone/log.h>

#include "common.h"
#include "scsi.h"


struct st_scsi_tape_drive_private {
	int fd_nst;
	int used_by_io;
	struct timeval last_start;
	struct mtget status;
};

struct st_scsi_tape_drive_io_reader {
	int fd;

	char * buffer;
	unsigned int block_size;
	char * buffer_pos;

	ssize_t position;
	short end_of_file;

	int last_errno;

	struct st_drive * drive;
	struct st_scsi_tape_drive_private * drive_private;
};

struct st_scsi_tape_drive_io_writer {
	int fd;

	char * buffer;
	ssize_t buffer_used;
	ssize_t block_size;
	ssize_t position;

	int last_errno;

	struct st_drive * drive;
	struct st_scsi_tape_drive_private * drive_private;
};


static void st_scsi_tape_drive_create_media(struct st_drive * drive);
static int st_scsi_tape_drive_eject(struct st_drive * drive);
static int st_scsi_tape_drive_format(struct st_drive * drive, int quick_mode);
static ssize_t st_scsi_tape_drive_get_block_size(struct st_drive * drive);
static struct st_stream_reader * st_scsi_tape_drive_get_reader(struct st_drive * drive, int file_position);
static struct st_stream_writer * st_scsi_tape_drive_get_writer(struct st_drive * drive, int file_position);
static void st_scsi_tape_drive_on_failed(struct st_drive * drive, int verbose, int sleep_time);
static void st_scsi_tape_drive_operation_start(struct st_scsi_tape_drive_private * dr);
static void st_scsi_tape_drive_operation_stop(struct st_drive * dr);
static int st_scsi_tape_drive_rewind(struct st_drive * dr);
static int st_scsi_tape_drive_set_file_position(struct st_drive * drive, int file_position);
static int st_scsi_tape_drive_update_media_info(struct st_drive * drive);

static int st_scsi_tape_drive_io_reader_close(struct st_stream_reader * io);
static int st_scsi_tape_drive_io_reader_end_of_file(struct st_stream_reader * io);
static off_t st_scsi_tape_drive_io_reader_forward(struct st_stream_reader * io, off_t offset);
static void st_scsi_tape_drive_io_reader_free(struct st_stream_reader * io);
static ssize_t st_scsi_tape_drive_io_reader_get_block_size(struct st_stream_reader * io);
static int st_scsi_tape_drive_io_reader_last_errno(struct st_stream_reader * io);
static struct st_stream_reader * st_scsi_tape_drive_io_reader_new(struct st_drive * drive);
static ssize_t st_scsi_tape_drive_io_reader_position(struct st_stream_reader * io);
static ssize_t st_scsi_tape_drive_io_reader_read(struct st_stream_reader * io, void * buffer, ssize_t length);

static int st_scsi_tape_drive_io_writer_close(struct st_stream_writer * io);
static void st_scsi_tape_drive_io_writer_free(struct st_stream_writer * io);
static ssize_t st_scsi_tape_drive_io_writer_get_available_size(struct st_stream_writer * io);
static ssize_t st_scsi_tape_drive_io_writer_get_block_size(struct st_stream_writer * io);
static int st_scsi_tape_drive_io_writer_last_errno(struct st_stream_writer * io);
static struct st_stream_writer * st_scsi_tape_drive_io_writer_new(struct st_drive * drive);
static ssize_t st_scsi_tape_drive_io_writer_position(struct st_stream_writer * io);
static ssize_t st_scsi_tape_drive_io_writer_write(struct st_stream_writer * io, const void * buffer, ssize_t length);

static struct st_drive_ops st_scsi_tape_drive_ops = {
	.eject             = st_scsi_tape_drive_eject,
	.format            = st_scsi_tape_drive_format,
	.get_reader        = st_scsi_tape_drive_get_reader,
	.get_writer        = st_scsi_tape_drive_get_writer,
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
	.close              = st_scsi_tape_drive_io_writer_close,
	.free               = st_scsi_tape_drive_io_writer_free,
	.get_available_size = st_scsi_tape_drive_io_writer_get_available_size,
	.get_block_size     = st_scsi_tape_drive_io_writer_get_block_size,
	.last_errno         = st_scsi_tape_drive_io_writer_last_errno,
	.position           = st_scsi_tape_drive_io_writer_position,
	.write              = st_scsi_tape_drive_io_writer_write,
};


static void st_scsi_tape_drive_create_media(struct st_drive * drive) {
	struct st_scsi_tape_drive_private * self = drive->data;

	struct st_media * media = malloc(sizeof(struct st_media));
	bzero(media, sizeof(struct st_media));
	drive->slot->media = media;

	if (drive->slot->volume_name)
		media->label = strdup(drive->slot->volume_name);

	media->format = st_media_format_get_by_density_code((self->status.mt_dsreg & MT_ST_DENSITY_MASK) >> MT_ST_DENSITY_SHIFT, st_media_format_mode_linear);

	media->status = st_media_status_foreign;
	media->location = st_media_location_indrive;
	media->first_used = time(NULL);
	media->use_before = media->first_used + media->format->life_span;

	media->load_count = 1;

	media->block_size = st_scsi_tape_drive_get_block_size(drive);

	if (media->format != NULL && media->format->support_mam) {
		int fd = open(drive->scsi_device, O_RDWR);
		int failed = st_scsi_tape_read_mam(fd, media);
		close(fd);

		if (failed)
			st_log_write_all(st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: failed to read medium axilary memory", drive->vendor, drive->model, drive - drive->changer->drives);
	}

	st_media_read_header(drive);
}

static int st_scsi_tape_drive_eject(struct st_drive * drive) {
	if (drive == NULL || !drive->enabled)
		return 1;

	struct st_scsi_tape_drive_private * self = drive->data;

	st_log_write_all(st_log_level_info, st_log_type_drive, "[%s | %s | #%td]: rewind tape and put the drive offline", drive->vendor, drive->model, drive - drive->changer->drives);

	static struct mtop eject = { MTOFFL, 1 };
	st_scsi_tape_drive_operation_start(self);
	int failed = ioctl(self->fd_nst, MTIOCTOP, &eject);
	st_scsi_tape_drive_operation_stop(drive);

	if (!failed)
		drive->slot->media->operation_count++;

	st_log_write_all(failed ? st_log_level_error : st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: rewind tape and put the drive offline, finish with code = %d", drive->vendor, drive->model, drive - drive->changer->drives, failed);

	st_scsi_tape_drive_update_media_info(drive);

	return failed;
}

static int st_scsi_tape_drive_format(struct st_drive * drive, int quick_mode) {
	if (drive == NULL || !drive->enabled)
		return 1;

	struct st_scsi_tape_drive_private * self = drive->data;

	st_scsi_tape_drive_operation_start(self);
	int failed = ioctl(self->fd_nst, MTIOCGET, &self->status);
	st_scsi_tape_drive_operation_stop(drive);

	/*
	 * There is a limitation with scsi command 'space' used by driver st of linux.
	 * In this command block_number is specified into 3 bytes so 8388607 is the
	 * maximum that we can forward or backward in one time.
	 */
	while (self->status.mt_blkno > 0 && !failed) {
		struct mtop backward = {
			.mt_op    = MTBSR,
			.mt_count = self->status.mt_blkno > 8388607 ? 8388607 : self->status.mt_blkno,
		};

		st_scsi_tape_drive_operation_start(self);
		failed = ioctl(self->fd_nst, MTIOCTOP, &backward);
		st_scsi_tape_drive_operation_stop(drive);

		if (!failed)
			drive->slot->media->operation_count++;

		st_scsi_tape_drive_operation_start(self);
		failed = ioctl(self->fd_nst, MTIOCGET, &self->status);
		st_scsi_tape_drive_operation_stop(drive);
	}

	int fd = -1;
	if (!failed)
		fd = open(drive->scsi_device, O_RDWR);

	if (!failed && fd > -1) {
		st_scsi_tape_drive_operation_start(self);
		failed = st_scsi_tape_format(fd, quick_mode);
		st_scsi_tape_drive_operation_stop(drive);

		if (!failed)
			drive->slot->media->operation_count++;

		close(fd);
	}

	return failed;
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

static struct st_stream_reader * st_scsi_tape_drive_get_reader(struct st_drive * drive, int file_position) {
	if (drive == NULL || !drive->enabled)
		return NULL;

	if (st_scsi_tape_drive_set_file_position(drive, file_position))
		return NULL;

	struct st_scsi_tape_drive_private * self = drive->data;
	if (drive->is_empty || self->used_by_io) {
		st_log_write_all(st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: drive is not free for reading on it", drive->vendor, drive->model, drive - drive->changer->drives);
		return NULL;
	}

	drive->slot->media->read_count++;
	st_log_write_all(st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: drive is open for reading", drive->vendor, drive->model, drive - drive->changer->drives);

	return st_scsi_tape_drive_io_reader_new(drive);
}

static struct st_stream_writer * st_scsi_tape_drive_get_writer(struct st_drive * drive, int file_position) {
	if (drive == NULL || !drive->enabled)
		return NULL;

	st_scsi_tape_drive_set_file_position(drive, file_position);

	struct st_scsi_tape_drive_private * self = drive->data;
	if (drive->is_empty || self->used_by_io) {
		st_log_write_all(st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: drive is not free for writing on it", drive->vendor, drive->model, drive - drive->changer->drives);
		return NULL;
	}

	drive->slot->media->write_count++;
	st_log_write_all(st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: drive is open for writing", drive->vendor, drive->model, drive - drive->changer->drives);

	return st_scsi_tape_drive_io_writer_new(drive);
}

void st_scsi_tape_drive_setup(struct st_drive * drive) {
	int fd = open(drive->device, O_RDWR | O_NONBLOCK);

	struct st_scsi_tape_drive_private * self = malloc(sizeof(struct st_scsi_tape_drive_private));
	bzero(self, sizeof(struct st_scsi_tape_drive_private));
	self->fd_nst = fd;
	self->used_by_io = 0;

	drive->mode = st_media_format_mode_linear;
	drive->ops = &st_scsi_tape_drive_ops;
	drive->data = self;
	drive->operation_duration = 0;
	drive->last_clean = 0;

	drive->lock = st_ressource_new();

	struct mtget status;
	st_scsi_tape_drive_operation_start(self);
	int failed = ioctl(self->fd_nst, MTIOCGET, &status);
	st_scsi_tape_drive_operation_stop(drive);

	if (!failed) {
		if (GMT_DR_OPEN(status.mt_gstat)) {
			drive->status = st_drive_empty_idle;
			drive->is_empty = 1;
		} else {
			drive->status = st_drive_loaded_idle;
			drive->is_empty = 0;
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
	struct st_scsi_tape_drive_private * self = drive->data;

	int failed = st_scsi_tape_drive_update_media_info(drive);
	if (failed)
		return failed;

	if (self->status.mt_fileno == 0 && self->status.mt_blkno == 0)
		return 0;

	st_log_write_all(st_log_level_info, st_log_type_drive, "[%s | %s | #%td]: rewind tape and put the drive offline", drive->vendor, drive->model, drive - drive->changer->drives);

	drive->status = st_drive_rewinding;

	static struct mtop rewind = { MTBSFM, 1 };
	st_scsi_tape_drive_operation_start(self);
	failed = ioctl(self->fd_nst, MTIOCTOP, &rewind);
	st_scsi_tape_drive_operation_stop(drive);

	drive->status = st_drive_loaded_idle;

	if (!failed)
		drive->slot->media->operation_count++;

	st_log_write_all(failed ? st_log_level_error : st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: rewind file, finish with code = %d", drive->vendor, drive->model, drive - drive->changer->drives, failed);

	st_scsi_tape_drive_update_media_info(drive);

	return failed;
}

static int st_scsi_tape_drive_set_file_position(struct st_drive * drive, int file_position) {
	struct st_scsi_tape_drive_private * self = drive->data;

	int failed;
	if (file_position < 0) {
		static struct mtop eod = { MTEOM, 1 };
		st_scsi_tape_drive_operation_start(self);
		failed = ioctl(self->fd_nst, MTIOCTOP, &eod);
		st_scsi_tape_drive_operation_stop(drive);
	} else {
		static struct mtop rewind = { MTBSFM, 1 };
		st_scsi_tape_drive_operation_start(self);
		failed = ioctl(self->fd_nst, MTIOCTOP, &rewind);
		st_scsi_tape_drive_operation_stop(drive);

		if (failed)
			return failed;

		struct mtop fsr = { MTFSF, file_position };
		st_scsi_tape_drive_operation_start(self);
		failed = ioctl(self->fd_nst, MTIOCTOP, &fsr);
		st_scsi_tape_drive_operation_stop(drive);
	}

	if (!failed)
		st_scsi_tape_drive_update_media_info(drive);

	return failed;
}

static int st_scsi_tape_drive_update_media_info(struct st_drive * drive) {
	if (drive == NULL || !drive->enabled)
		return 1;

	struct st_scsi_tape_drive_private * self = drive->data;

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

	if (!failed) {
		if (GMT_DR_OPEN(self->status.mt_gstat)) {
			drive->status = st_drive_empty_idle;
			drive->is_empty = 1;
		} else {
			drive->status = st_drive_loaded_idle;
			drive->is_empty = 0;
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
	} else {
		drive->status = st_drive_error;
	}

	return failed;
}


static int st_scsi_tape_drive_io_reader_close(struct st_stream_reader * io) {
	if (io == NULL || !io->data)
		return -1;

	struct st_scsi_tape_drive_io_reader * self = io->data;

	if (self->fd > -1) {
		self->fd = -1;
		self->buffer_pos = self->buffer + self->block_size;
		self->last_errno = 0;

		self->drive_private->used_by_io = 0;
		st_log_write_all(st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: drive is close", self->drive->vendor, self->drive->model, self->drive - self->drive->changer->drives);
	}

	return 0;
}

static int st_scsi_tape_drive_io_reader_end_of_file(struct st_stream_reader * io) {
	if (io == NULL)
		return 1;

	struct st_scsi_tape_drive_io_reader * self = io->data;
	return self->end_of_file;
}

static off_t st_scsi_tape_drive_io_reader_forward(struct st_stream_reader * io, off_t offset) {
	if (io == NULL)
		return -1;

	struct st_scsi_tape_drive_io_reader * self = io->data;
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

			struct mtop forward = { MTFSR, next_forward };
			st_scsi_tape_drive_operation_start(self->drive_private);
			int failed = ioctl(self->fd, MTIOCTOP, &forward);
			st_scsi_tape_drive_operation_stop(self->drive);

			if (failed) {
				self->last_errno = errno;
				return failed;
			} else
				nb_records -= next_forward;
		}

		self->position += nb_records * self->block_size;
		nb_total_read += nb_records * self->block_size;
	}

	if (nb_total_read == offset)
		return self->position;

	st_scsi_tape_drive_operation_start(self->drive_private);
	ssize_t nb_read = read(self->fd, self->buffer, self->block_size);
	st_scsi_tape_drive_operation_stop(self->drive);

	if (nb_read < 0) {
		self->last_errno = errno;
		return nb_read;
	} else if (nb_read > 0) {
		ssize_t will_move = offset - nb_total_read;
		self->buffer_pos = self->buffer + will_move;
		self->position += will_move;
	}

	return self->position;
}

static void st_scsi_tape_drive_io_reader_free(struct st_stream_reader * io) {
	if (io == NULL)
		return;

	struct st_scsi_tape_drive_io_reader * self = io->data;
	if (self) {
		self->fd = -1;
		free(self->buffer);
		free(self);
	}
	io->data = NULL;
	io->ops = NULL;
}

static ssize_t st_scsi_tape_drive_io_reader_get_block_size(struct st_stream_reader * io) {
	struct st_scsi_tape_drive_io_reader * self = io->data;
	return self->block_size;
}

static int st_scsi_tape_drive_io_reader_last_errno(struct st_stream_reader * io) {
	struct st_scsi_tape_drive_io_reader * self = io->data;
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
	self->end_of_file = 0;

	self->drive = drive;
	self->drive_private = dr;

	dr->used_by_io = 1;

	struct st_stream_reader * reader = malloc(sizeof(struct st_stream_reader));
	reader->ops = &st_scsi_tape_drive_io_reader_ops;
	reader->data = self;

	return reader;
}

static ssize_t st_scsi_tape_drive_io_reader_position(struct st_stream_reader * io) {
	if (io == NULL || io->data == NULL)
		return -1;

	struct st_scsi_tape_drive_io_reader * self = io->data;
	return self->position;
}

static ssize_t st_scsi_tape_drive_io_reader_read(struct st_stream_reader * io, void * buffer, ssize_t length) {
	if (io == NULL || buffer == NULL || length < 0)
		return -1;

	struct st_scsi_tape_drive_io_reader * self = io->data;
	self->last_errno = 0;

	if (self->fd < 0)
		return -1;

	if (length < 1)
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
			return nb_read;
		} else if (nb_read > 0) {
			nb_total_read += nb_read;
			self->position += nb_read;
		} else {
			self->end_of_file = 1;
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
		return nb_read;
	} else if (nb_read > 0) {
		ssize_t will_copy = length - nb_total_read;
		memcpy(c_buffer + nb_total_read, self->buffer, will_copy);
		self->buffer_pos = self->buffer + will_copy;
		self->position += will_copy;
		return length;
	} else {
		self->end_of_file = 1;
		return nb_total_read;
	}
}


static int st_scsi_tape_drive_io_writer_close(struct st_stream_writer * io) {
	if (io == NULL || io->data == NULL)
		return -1;

	struct st_scsi_tape_drive_io_writer * self = io->data;
	self->last_errno = 0;

	if (self->buffer_used) {
		bzero(self->buffer + self->buffer_used, self->block_size - self->buffer_used);

		st_scsi_tape_drive_operation_start(self->drive_private);
		ssize_t nb_write = write(self->fd, self->buffer, self->block_size);
		st_scsi_tape_drive_operation_stop(self->drive);

		if (nb_write < 0) {
			self->last_errno = errno;
			return -1;
		}

		self->position += nb_write;
		self->buffer_used = 0;
	}

	if (self->fd > -1) {
		static struct mtop eof = { MTWEOF, 1 };
		st_scsi_tape_drive_operation_start(self->drive_private);
		int failed = ioctl(self->fd, MTIOCTOP, &eof);
		st_scsi_tape_drive_operation_stop(self->drive);

		if (failed) {
			self->last_errno = errno;
			return -1;
		}

		st_scsi_tape_drive_update_media_info(self->drive);
		self->drive->slot->media->nb_volumes = self->drive_private->status.mt_fileno;
		//st_drive_generic_update_position(self->drive);

		self->fd = -1;
		self->drive_private->used_by_io = 0;
		st_log_write_all(st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: drive is close", self->drive->vendor, self->drive->model, self->drive - self->drive->changer->drives);
	}

	return 0;
}

static void st_scsi_tape_drive_io_writer_free(struct st_stream_writer * io) {
	if (io == NULL)
		return;

	struct st_scsi_tape_drive_io_writer * self = io->data;
	if (self) {
		self->fd = -1;
		free(self->buffer);
		free(self);
	}
	io->data = NULL;
	io->ops = NULL;
}

static ssize_t st_scsi_tape_drive_io_writer_get_available_size(struct st_stream_writer * io) {
	struct st_scsi_tape_drive_io_writer * self = io->data;
	struct st_media * media = self->drive->slot->media;

	if (!media)
		return 0;

	// only for test purpose
	// should be a multiple of blocksize
	// return 59999977472 - self->position;

	// we reserve 16 blocks at the end of tape
	// if (tape->available_block <= 16)
	// 	return 0;

	return (media->available_block - 16) * media->block_size - self->buffer_used;
}

static ssize_t st_scsi_tape_drive_io_writer_get_block_size(struct st_stream_writer * io) {
	struct st_scsi_tape_drive_io_writer * self = io->data;
	return self->block_size;
}

static int st_scsi_tape_drive_io_writer_last_errno(struct st_stream_writer * io) {
	struct st_scsi_tape_drive_io_writer * self = io->data;
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
	self->last_errno = 0;
	self->drive = drive;
	self->drive_private = dr;

	dr->used_by_io = 1;

	struct st_stream_writer * writer = malloc(sizeof(struct st_stream_writer));
	writer->ops = &st_scsi_tape_drive_io_writer_ops;
	writer->data = self;

	return writer;
}

static ssize_t st_scsi_tape_drive_io_writer_position(struct st_stream_writer * io) {
	if (io == NULL || io->data == NULL)
		return -1;

	struct st_scsi_tape_drive_io_writer * self = io->data;
	return self->position;
}

static ssize_t st_scsi_tape_drive_io_writer_write(struct st_stream_writer * io, const void * buffer, ssize_t length) {
	if (io == NULL || buffer == NULL || length < 0)
		return -1;

	struct st_scsi_tape_drive_io_writer * self = io->data;
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
		self->last_errno = errno;
		return -1;
	}

	ssize_t nb_total_write = buffer_available;
	self->buffer_used = 0;
	self->position += buffer_available;
	self->drive->slot->media->available_block--;

	const char * c_buffer = buffer;
	while (length - nb_total_write >= self->block_size) {
		st_scsi_tape_drive_operation_start(self->drive_private);
		nb_write = write(self->fd, c_buffer + nb_total_write, self->block_size);
		st_scsi_tape_drive_operation_stop(self->drive);

		if (nb_write < 0) {
			self->last_errno = errno;
			return -1;
		}

		nb_total_write += nb_write;
		self->position += nb_write;
		self->drive->slot->media->available_block--;
	}

	if (length == nb_total_write)
		return length;

	self->buffer_used = length - nb_total_write;
	self->position += self->buffer_used;
	memcpy(self->buffer, c_buffer + nb_total_write, self->buffer_used);

	return length;
}

