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
*  Last modified: Mon, 05 Nov 2012 18:21:35 +0100                         *
\*************************************************************************/

// errno
#include <errno.h>
// open
#include <fcntl.h>
// bool
#include <stdbool.h>
// free, malloc, realloc
#include <stdlib.h>
// bzero, memcpy, memmove
#include <string.h>
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
// close, read, sleep
#include <unistd.h>

#include <stone/io.h>
#include <stone/library/drive.h>
#include <stone/library/ressource.h>
#include <stone/library/tape.h>
#include <stone/log.h>

#include "common.h"
#include "scsi.h"

struct st_drive_generic {
	int fd_nst;
	struct st_database_connection * db_con;

	int used_by_io;

	struct timeval last_start;
	long double total_spent_time;
};

struct st_drive_io_reader {
	int fd;

	char * buffer;
	unsigned int block_size;
	char * buffer_pos;

	ssize_t position;
	short end_of_file;

	int last_errno;

	struct st_drive * drive;
	struct st_drive_generic * drive_private;
};

struct st_drive_io_writer {
	int fd;
	char * buffer;
	ssize_t buffer_used;
	ssize_t block_size;
	ssize_t position;
	int last_errno;
	bool eod_reached;

	struct st_drive * drive;
	struct st_drive_generic * drive_private;
};

static int st_drive_generic_eject(struct st_drive * drive);
static int st_drive_generic_eod(struct st_drive * drive);
static ssize_t st_drive_generic_get_block_size(struct st_drive * drive);
static struct st_stream_reader * st_drive_generic_get_reader(struct st_drive * drive);
static struct st_stream_writer * st_drive_generic_get_writer(struct st_drive * drive);
static void st_drive_generic_on_failed(struct st_drive * drive, int verbose);
static void st_drive_generic_operation_start(struct st_drive_generic * dr);
static void st_drive_generic_operation_stop(struct st_drive * dr);
static int st_drive_generic_read_mam(struct st_drive * drive);
static void st_drive_generic_reset(struct st_drive * drive, bool quick_mode, bool force);
static int st_drive_generic_rewind_file(struct st_drive * drive);
static int st_drive_generic_rewind_tape(struct st_drive * drive);
static int st_drive_generic_set_file_position(struct st_drive * drive, int file_position);
static void st_drive_generic_update_position(struct st_drive * drive);
static void st_drive_generic_update_status(struct st_drive * drive);

static int st_drive_io_reader_close(struct st_stream_reader * io);
static int st_drive_io_reader_end_of_file(struct st_stream_reader * io);
static off_t st_drive_io_reader_forward(struct st_stream_reader * io, off_t offset);
static void st_drive_io_reader_free(struct st_stream_reader * io);
static ssize_t st_drive_io_reader_get_block_size(struct st_stream_reader * io);
static int st_drive_io_reader_last_errno(struct st_stream_reader * io);
static struct st_stream_reader * st_drive_io_reader_new(struct st_drive * drive);
static ssize_t st_drive_io_reader_position(struct st_stream_reader * io);
static ssize_t st_drive_io_reader_read(struct st_stream_reader * io, void * buffer, ssize_t length);
static off_t st_drive_io_reader_set_position(struct st_stream_reader * io, off_t position);

static int st_drive_io_writer_close(struct st_stream_writer * io);
static void st_drive_io_writer_free(struct st_stream_writer * io);
static ssize_t st_drive_io_writer_get_available_size(struct st_stream_writer * io);
static ssize_t st_drive_io_writer_get_block_size(struct st_stream_writer * io);
static int st_drive_io_writer_last_errno(struct st_stream_writer * io);
static struct st_stream_writer * st_drive_io_writer_new(struct st_drive * drive);
static ssize_t st_drive_io_writer_position(struct st_stream_writer * io);
static ssize_t st_drive_io_writer_write(struct st_stream_writer * io, const void * buffer, ssize_t length);

static struct st_drive_ops st_drive_generic_ops = {
	.eject             = st_drive_generic_eject,
	.eod               = st_drive_generic_eod,
	.get_block_size    = st_drive_generic_get_block_size,
	.get_reader        = st_drive_generic_get_reader,
	.get_writer        = st_drive_generic_get_writer,
	.read_mam          = st_drive_generic_read_mam,
	.reset             = st_drive_generic_reset,
	.rewind_file       = st_drive_generic_rewind_file,
	.rewind_tape       = st_drive_generic_rewind_tape,
	.set_file_position = st_drive_generic_set_file_position,
};

static struct st_stream_reader_ops st_drive_io_reader_ops = {
	.close          = st_drive_io_reader_close,
	.end_of_file    = st_drive_io_reader_end_of_file,
	.forward        = st_drive_io_reader_forward,
	.free           = st_drive_io_reader_free,
	.get_block_size = st_drive_io_reader_get_block_size,
	.last_errno     = st_drive_io_reader_last_errno,
	.position       = st_drive_io_reader_position,
	.read           = st_drive_io_reader_read,
	.set_position   = st_drive_io_reader_set_position,
};

static struct st_stream_writer_ops st_drive_io_writer_ops = {
	.close              = st_drive_io_writer_close,
	.free               = st_drive_io_writer_free,
	.get_available_size = st_drive_io_writer_get_available_size,
	.get_block_size     = st_drive_io_writer_get_block_size,
	.last_errno         = st_drive_io_writer_last_errno,
	.position           = st_drive_io_writer_position,
	.write              = st_drive_io_writer_write,
};


int st_drive_generic_eject(struct st_drive * drive) {
	struct st_drive_generic * self = drive->data;

	st_log_write_all(st_log_level_info, st_log_type_drive, "[%s | %s | #%td]: rewind tape and put the drive offline", drive->vendor, drive->model, drive - drive->changer->drives);

	drive->status = ST_DRIVE_UNLOADING;

	static struct mtop eject = { MTOFFL, 1 };
	st_drive_generic_operation_start(self);
	int failed = ioctl(self->fd_nst, MTIOCTOP, &eject);
	st_drive_generic_operation_stop(drive);

	st_log_write_all(failed ? st_log_level_error : st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: rewind tape and put the drive offline, finish with code = %d", drive->vendor, drive->model, drive - drive->changer->drives, failed);

	st_drive_generic_update_status(drive);

	return failed;
}

int st_drive_generic_eod(struct st_drive * drive) {
	struct st_drive_generic * self = drive->data;

	st_log_write_all(st_log_level_info, st_log_type_drive, "[%s | %s | #%td]: goto end of tape", drive->vendor, drive->model, drive - drive->changer->drives);

	drive->status = ST_DRIVE_POSITIONING;

	static struct mtop eod = { MTEOM, 1 };
	st_drive_generic_operation_start(self);
	int failed = ioctl(self->fd_nst, MTIOCTOP, &eod);
	st_drive_generic_operation_stop(drive);

	if (failed) {
		st_drive_generic_on_failed(drive, 1);
		st_drive_generic_update_status(drive);
		st_drive_generic_operation_start(self);
		failed = ioctl(self->fd_nst, MTIOCTOP, &eod);
		st_drive_generic_operation_stop(drive);
	}

	st_log_write_all(failed ? st_log_level_error : st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: goto end of tape, finish with code = %d", drive->vendor, drive->model, drive - drive->changer->drives, failed);

	st_drive_generic_update_position(drive);
	st_drive_generic_update_status(drive);

	return failed;
}

ssize_t st_drive_generic_get_block_size(struct st_drive * drive) {
	if (drive->block_size < 1) {
		struct st_tape * tape = drive->slot->tape;

		if (tape->block_size > 0)
			return tape->block_size;

		st_drive_generic_update_status(drive);

		if (!drive->is_bottom_of_tape) {
			if (drive->file_position > 0)
				st_drive_generic_rewind_file(drive);
			else
				st_drive_generic_rewind_tape(drive);
		}

		st_log_write_all(st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: try to find block size previously used", drive->vendor, drive->model, drive - drive->changer->drives);

		struct st_drive_generic * self = drive->data;

		unsigned int i;
		ssize_t nb_read;
		ssize_t block_size = 1 << 16;
		char * buffer = malloc(block_size);

		for (i = 0; i < 4; i++) {
			st_drive_generic_operation_start(self);
			nb_read = read(self->fd_nst, buffer, block_size);
			st_drive_generic_operation_stop(drive);

			tape->read_count++;

			st_drive_generic_update_status(drive);

			if (drive->block_number < 0)
				break;

			if (drive->file_position > 0)
				st_drive_generic_rewind_file(drive);
			else
				st_drive_generic_rewind_tape(drive);

			if (nb_read > 0) {
				st_log_write_all(st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: found block size: %zd", drive->vendor, drive->model, drive - drive->changer->drives, nb_read);

				free(buffer);
				return tape->block_size = nb_read;
			}

			block_size <<= 1;
			buffer = realloc(buffer, block_size);
		}

		free(buffer);

		if (tape->format) {
			st_log_write_all(st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: failed to find block size, using the default block size: %zd", drive->vendor, drive->model, drive - drive->changer->drives, tape->format->block_size);
			return tape->block_size = tape->format->block_size;
		}

		st_log_write_all(st_log_level_error, st_log_type_drive, "[%s | %s | #%td]: failed to detect block size", drive->vendor, drive->model, drive - drive->changer->drives);
	}

	return drive->block_size;
}

struct st_stream_reader * st_drive_generic_get_reader(struct st_drive * drive) {
	st_drive_generic_update_status(drive);

	struct st_drive_generic * self = drive->data;
	if (drive->is_door_opened || self->used_by_io) {
		st_log_write_all(st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: drive is not free for reading on it", drive->vendor, drive->model, drive - drive->changer->drives);
		return 0;
	}

	drive->slot->tape->read_count++;
	drive->status = ST_DRIVE_READING;
	st_log_write_all(st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: drive is open for reading", drive->vendor, drive->model, drive - drive->changer->drives);

	return st_drive_io_reader_new(drive);
}

struct st_stream_writer * st_drive_generic_get_writer(struct st_drive * drive) {
	st_drive_generic_update_status(drive);

	struct st_drive_generic * self = drive->data;
	if (drive->is_door_opened || self->used_by_io) {
		st_log_write_all(st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: drive is not free for writing on it", drive->vendor, drive->model, drive - drive->changer->drives);
		return 0;
	}

	if (!drive->is_writable) {
		st_log_write_all(st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: tape is not writable (Write protec enabled)", drive->vendor, drive->model, drive - drive->changer->drives);
		return 0;
	}

	drive->slot->tape->write_count++;
	drive->status = ST_DRIVE_WRITING;
	st_log_write_all(st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: drive is open for writing", drive->vendor, drive->model, drive - drive->changer->drives);

	return st_drive_io_writer_new(drive);
}

void st_drive_generic_on_failed(struct st_drive * drive, int verbose) {
	struct st_drive_generic * self = drive->data;

	if (verbose)
		st_log_write_all(st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: Try to recover an error", drive->vendor, drive->model, drive - drive->changer->drives);
	close(self->fd_nst);
	sleep(20);
	self->fd_nst = open(drive->device, O_RDWR | O_NONBLOCK);
}

void st_drive_generic_operation_start(struct st_drive_generic * dr) {
	gettimeofday(&dr->last_start, 0);
}

void st_drive_generic_operation_stop(struct st_drive * drive) {
	struct st_drive_generic * self = drive->data;

	struct timeval finish;
	gettimeofday(&finish, 0);

	drive->operation_duration += (finish.tv_sec - self->last_start.tv_sec) + ((double) (finish.tv_usec - self->last_start.tv_usec)) / 1000000;
}

int st_drive_generic_read_mam(struct st_drive * drive) {
	struct st_tape * tape = drive->slot->tape;
	if (!tape)
		return -1;

	st_log_write_all(st_log_level_info, st_log_type_drive, "[%s | %s | #%td]: Try to read medium axiliary memory", drive->vendor, drive->model, drive - drive->changer->drives);

	int fd = open(drive->scsi_device, O_RDWR);
	int failed = st_scsi_tape_read_mam(fd, tape);
	close(fd);

	st_log_write_all(failed ? st_log_level_error : st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: Finish to read mam, finish with code = %d", drive->vendor, drive->model, drive - drive->changer->drives, failed);

	return failed;
}

void st_drive_generic_reset(struct st_drive * drive, bool quick_mode, bool force) {
	struct st_drive_generic * self = drive->data;

	if (force) {
		if (self->fd_nst > -1)
			close(self->fd_nst);

		sleep(1);

		self->fd_nst = open(drive->device, O_RDWR | O_NONBLOCK);
	}

	static struct mtop nop = { MTNOP, 1 };

	int i, failed = 1;
	for (i = 0; i < 600 && failed && !force; i++) {
		if (self->fd_nst > -1) {
			st_drive_generic_operation_start(self);
			failed = ioctl(self->fd_nst, MTIOCTOP, &nop);
			st_drive_generic_operation_stop(drive);
		}

		if (failed) {
			if (i == 1)
				st_log_write_all(st_log_level_info, st_log_type_drive, "[%s | %s | #%td]: tape drive is not ready, waiting for", drive->vendor, drive->model, drive - drive->changer->drives);

			if (self->fd_nst > -1)
				close(self->fd_nst);

			sleep(1);

			self->fd_nst = open(drive->device, O_RDWR | O_NONBLOCK);
		}

		if (quick_mode && failed)
			return;
	}

	drive->nb_files = 0;
	st_drive_generic_update_status(drive);

	if (i > 1)
		st_log_write_all(st_log_level_info, st_log_type_drive, "[%s | %s | #%td]: tape drive is now ready", drive->vendor, drive->model, drive - drive->changer->drives);
}

int st_drive_generic_rewind_file(struct st_drive * drive) {
	struct st_drive_generic * self = drive->data;

	st_log_write_all(st_log_level_info, st_log_type_drive, "[%s | %s | #%td]: rewind file", drive->vendor, drive->model, drive - drive->changer->drives);

	drive->status = ST_DRIVE_REWINDING;

	static struct mtop rewind = { MTBSFM, 1 };
	st_drive_generic_operation_start(self);
	int failed = ioctl(self->fd_nst, MTIOCTOP, &rewind);
	st_drive_generic_operation_stop(drive);

	unsigned int i;
	for (i = 0; i < 3 && failed; i++) {
		st_drive_generic_on_failed(drive, 1);
		st_drive_generic_update_status(drive);
		st_drive_generic_operation_start(self);
		failed = ioctl(self->fd_nst, MTIOCTOP, &rewind);
		st_drive_generic_operation_stop(drive);
	}

	st_log_write_all(failed ? st_log_level_error : st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: rewind file, finish with code = %d", drive->vendor, drive->model, drive - drive->changer->drives, failed);

	st_drive_generic_update_status(drive);

	return failed;
}

int st_drive_generic_rewind_tape(struct st_drive * drive) {
	struct st_drive_generic * self = drive->data;

	st_log_write_all(st_log_level_info, st_log_type_drive, "[%s | %s | #%td]: rewind tape", drive->vendor, drive->model, drive - drive->changer->drives);

	drive->status = ST_DRIVE_REWINDING;

	static struct mtop rewind = { MTREW, 1 };
	st_drive_generic_operation_start(self);
	int failed = ioctl(self->fd_nst, MTIOCTOP, &rewind);
	st_drive_generic_operation_stop(drive);

	unsigned int i;
	for (i = 0; i < 3 && failed; i++) {
		st_drive_generic_on_failed(drive, 1);
		st_drive_generic_update_status(drive);
		st_drive_generic_operation_start(self);
		failed = ioctl(self->fd_nst, MTIOCTOP, &rewind);
		st_drive_generic_operation_stop(drive);
	}

	st_log_write_all(failed ? st_log_level_error : st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: rewind tape, finish with code = %d", drive->vendor, drive->model, drive - drive->changer->drives, failed);

	st_drive_generic_update_status(drive);

	return failed;
}

int st_drive_generic_set_file_position(struct st_drive * drive, int file_position) {
	st_log_write_all(st_log_level_info, st_log_type_drive, "[%s | %s | #%td]: positioning tape to position = %d", drive->vendor, drive->model, drive - drive->changer->drives, file_position);

	int failed = st_drive_generic_rewind_tape(drive);

	if (failed)
		return failed;

	drive->status = ST_DRIVE_POSITIONING;

	struct st_drive_generic * self = drive->data;
	struct mtop fsr = { MTFSF, file_position };
	st_drive_generic_operation_start(self);
	failed = ioctl(self->fd_nst, MTIOCTOP, &fsr);
	st_drive_generic_operation_stop(drive);

	st_log_write_all(failed ? st_log_level_error : st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: positioning tape to position = %d, finish with code = %d", drive->vendor, drive->model, drive - drive->changer->drives, file_position, failed);

	return failed;
}

void st_drive_generic_update_position(struct st_drive * drive) {
	struct st_tape * tape = drive->slot->tape;
	if (!tape)
		return;

	struct st_drive_generic * self = drive->data;
	struct mtpos pos = { 0 };

	int fd = open(drive->scsi_device, O_RDWR);

	int failed = ioctl(self->fd_nst, MTIOCPOS, &pos);
	if (failed)
		failed = st_scsi_tape_position(fd, tape);
	else
		tape->end_position = pos.mt_blkno;

	st_log_write_all(failed ? st_log_level_error : st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: update tape position: %zd, finish with code = %d", drive->vendor, drive->model, drive - drive->changer->drives, tape->end_position, failed);

	failed = st_scsi_tape_size_available(fd, tape);
	close(fd);

	if (!failed)
		st_log_write_all(st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: there is %zd block%s of %zd bytes available (%zd * %zd = %zd)", drive->vendor, drive->model, drive - drive->changer->drives, tape->available_block, tape->available_block != 1 ? "s" : "", tape->block_size, tape->available_block, tape->block_size, tape->available_block * tape->block_size);
	else
		st_log_write_all(st_log_level_warning, st_log_type_drive, "[%s | %s | #%td]: failed to get available size on tape (%s)", drive->vendor, drive->model, drive - drive->changer->drives, tape->label);
}

void st_drive_generic_update_status(struct st_drive * drive) {
	struct st_drive_generic * self = drive->data;

	struct mtget status;
	st_drive_generic_operation_start(self);
	int failed = ioctl(self->fd_nst, MTIOCGET, &status);
	st_drive_generic_operation_stop(drive);

	unsigned int i;
	for (i = 0; i < 5 && failed; i++) {
		st_drive_generic_on_failed(drive, 1);
		st_drive_generic_operation_start(self);
		failed = ioctl(self->fd_nst, MTIOCGET, &status);
		st_drive_generic_operation_stop(drive);
	}

	if (failed) {
		drive->status = ST_DRIVE_ERROR;

		static struct mtop reset = { MTRESET, 1 };
		st_drive_generic_operation_start(self);
		failed = ioctl(self->fd_nst, MTIOCTOP, &reset);
		st_drive_generic_operation_stop(drive);

		if (!failed) {
			st_drive_generic_operation_start(self);
			failed = ioctl(self->fd_nst, MTIOCGET, &status);
			st_drive_generic_operation_stop(drive);
		}
	}

	if (!failed) {
		drive->file_position = status.mt_fileno;
		drive->block_number  = status.mt_blkno;

		if (drive->file_position > drive->nb_files)
			drive->nb_files = drive->file_position;

		drive->is_bottom_of_tape = GMT_BOT(status.mt_gstat) ? 1 : 0;
		drive->is_end_of_file    = GMT_EOF(status.mt_gstat) ? 1 : 0;
		drive->is_end_of_tape    = GMT_EOT(status.mt_gstat) ? 1 : 0;
		drive->is_writable       = GMT_WR_PROT(status.mt_gstat) ? 0 : 1;
		drive->is_online         = GMT_ONLINE(status.mt_gstat) ? 1 : 0;
		drive->is_door_opened    = GMT_DR_OPEN(status.mt_gstat) ? 1 : 0;

		drive->block_size   = (status.mt_dsreg & MT_ST_BLKSIZE_MASK) >> MT_ST_BLKSIZE_SHIFT;
		drive->density_code = (status.mt_dsreg & MT_ST_DENSITY_MASK) >> MT_ST_DENSITY_SHIFT;

		if (drive->density_code > drive->best_density_code)
			drive->best_density_code = drive->density_code;

		drive->status = drive->is_door_opened ? ST_DRIVE_EMPTY_IDLE : ST_DRIVE_LOADED_IDLE;

		struct st_tape * tape = drive->slot->tape;
		if (tape && !tape->mam_ok && tape->block_size > 0 && tape->format && tape->format->support_mam)
			st_drive_generic_read_mam(drive);
	} else {
		drive->status = ST_DRIVE_ERROR;
	}
}

void st_drive_setup(struct st_drive * drive) {
	int fd = open(drive->device, O_RDWR | O_NONBLOCK);

	struct st_drive_generic * self = malloc(sizeof(struct st_drive_generic));
	self->fd_nst = fd;
	self->db_con = 0;
	self->used_by_io = 0;
	self->total_spent_time = 0;

	drive->ops = &st_drive_generic_ops;
	drive->data = self;
	drive->best_density_code = 0;
	drive->density_code = 0;

	drive->lock = st_ressource_new();

	st_drive_generic_update_status(drive);
}


int st_drive_io_reader_close(struct st_stream_reader * io) {
	if (!io || !io->data)
		return -1;

	struct st_drive_io_reader * self = io->data;

	if (self->fd > -1) {
		self->fd = -1;
		self->buffer_pos = self->buffer + self->block_size;
		self->last_errno = 0;

		self->drive->status = ST_DRIVE_LOADED_IDLE;
		self->drive_private->used_by_io = 0;
		st_log_write_all(st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: drive is close", self->drive->vendor, self->drive->model, self->drive - self->drive->changer->drives);
	}

	return 0;
}

int st_drive_io_reader_end_of_file(struct st_stream_reader * io) {
	struct st_drive_io_reader * self = io->data;
	return self->end_of_file;
}

off_t st_drive_io_reader_forward(struct st_stream_reader * io, off_t offset) {
	if (!io)
		return -1;

	struct st_drive_io_reader * self = io->data;
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
		int nb_records = (offset - nb_total_read) / self->block_size;

		st_log_write_all(st_log_level_info, st_log_type_drive, "[%s | %s | #%td]: forward (#record %d)", self->drive->vendor, self->drive->model, self->drive - self->drive->changer->drives, nb_records);

		/**
		 * There is a limitation with scsi command 'space' used by driver st of linux.
		 * In this command block_number is specified into 3 bytes so 8388607 is the
		 * maximum that we can forward in one time.
		 */
		int failed = 0;
		while (nb_records > 0 && !failed) {
			int move_nb_records = nb_records > 8388607 ? 8388607 : nb_records;

			struct mtop forward = { MTFSR, move_nb_records };
			st_drive_generic_operation_start(self->drive_private);
			failed = ioctl(self->fd, MTIOCTOP, &forward);
			st_drive_generic_operation_stop(self->drive);

			if (failed) {
				self->last_errno = errno;
				return failed;
			}

			nb_records -= move_nb_records;

			self->position += move_nb_records * self->block_size;
			nb_total_read += move_nb_records * self->block_size;
		}
	}

	if (nb_total_read == offset)
		return self->position;

	st_drive_generic_operation_start(self->drive_private);
	ssize_t nb_read = read(self->fd, self->buffer, self->block_size);
	st_drive_generic_operation_stop(self->drive);
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

void st_drive_io_reader_free(struct st_stream_reader * io) {
	if (!io)
		return;

	struct st_drive_io_reader * self = io->data;
	if (self) {
		self->fd = -1;
		free(self->buffer);
		free(self);
	}
	io->data = 0;
	io->ops = 0;
}

ssize_t st_drive_io_reader_get_block_size(struct st_stream_reader * io) {
	struct st_drive_io_reader * self = io->data;
	return self->block_size;
}

int st_drive_io_reader_last_errno(struct st_stream_reader * io) {
	struct st_drive_io_reader * self = io->data;
	return self->last_errno;
}

struct st_stream_reader * st_drive_io_reader_new(struct st_drive * drive) {
	ssize_t block_size = drive->ops->get_block_size(drive);

	struct st_drive_generic * dr = drive->data;
	struct st_drive_io_reader * self = malloc(sizeof(struct st_drive_io_reader));

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
	reader->ops = &st_drive_io_reader_ops;
	reader->data = self;

	return reader;
}

ssize_t st_drive_io_reader_position(struct st_stream_reader * io) {
	if (!io || !io->data)
		return -1;

	struct st_drive_io_reader * self = io->data;
	return self->position;
}

ssize_t st_drive_io_reader_read(struct st_stream_reader * io, void * buffer, ssize_t length) {
	if (!io || !buffer || length < 0)
		return -1;

	struct st_drive_io_reader * self = io->data;
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
		st_drive_generic_operation_start(self->drive_private);
		ssize_t nb_read = read(self->fd, c_buffer + nb_total_read, self->block_size);
		st_drive_generic_operation_stop(self->drive);

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

	st_drive_generic_operation_start(self->drive_private);
	ssize_t nb_read = read(self->fd, self->buffer, self->block_size);
	st_drive_generic_operation_stop(self->drive);

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

off_t st_drive_io_reader_set_position(struct st_stream_reader * io, off_t position) {
	if (!io || !io->data)
		return -1;

	struct st_drive_io_reader * self = io->data;
	self->last_errno = 0;

	st_log_write_all(st_log_level_info, st_log_type_drive, "[%s | %s | #%td]: positioning tape to block = %zd", self->drive->vendor, self->drive->model, self->drive - self->drive->changer->drives, position);

	struct mtget pos;
	int failed = ioctl(self->fd, MTIOCGET, &pos);
	if (failed) {
		self->last_errno = errno;
		return -1;
	}

	/**
	 * There is a limitation with scsi command 'space' used by driver st of linux.
	 * In this command block_number is specified into 3 bytes so 8388607 is the
	 * maximum that we can forward in one time.
	 */
	if (pos.mt_blkno < position) {
		unsigned int nb_blocks, nb_blocks_remains;
		for (nb_blocks_remains = position - pos.mt_blkno; nb_blocks_remains > 0 && !failed; nb_blocks_remains -= nb_blocks ) {
			nb_blocks = nb_blocks_remains > 8388607 ? 8388607 : nb_blocks_remains;

			struct mtop forward = { MTFSR, nb_blocks };
			failed = ioctl(self->fd, MTIOCTOP, &forward);
		}
	} else if (pos.mt_blkno > position) {
		unsigned int nb_blocks, nb_blocks_remains;
		for (nb_blocks_remains = pos.mt_blkno - position; nb_blocks_remains > 0 && !failed; nb_blocks_remains -= nb_blocks ) {
			nb_blocks = nb_blocks_remains > 8388607 ? 8388607 : nb_blocks_remains;

			struct mtop backward = { MTFSR, nb_blocks };
			failed = ioctl(self->fd, MTIOCTOP, &backward);
		}
	}

	if (failed) {
		self->last_errno = errno;
		return -1;
	}

	failed = ioctl(self->fd, MTIOCGET, &pos);
	if (failed) {
		self->last_errno = errno;
		return -1;
	} else {
		self->position = (ssize_t) pos.mt_blkno * self->block_size;
	}

	st_log_write_all(failed ? st_log_level_error : st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: positioning tape to block = %zd, finish with code = %d", self->drive->vendor, self->drive->model, self->drive - self->drive->changer->drives, position, failed);

	self->buffer_pos = self->buffer + self->block_size;
	return self->position;
}


int st_drive_io_writer_close(struct st_stream_writer * io) {
	if (!io || !io->data)
		return -1;

	struct st_drive_io_writer * self = io->data;
	self->last_errno = 0;

	if (self->buffer_used) {
		bzero(self->buffer + self->buffer_used, self->block_size - self->buffer_used);

		st_drive_generic_operation_start(self->drive_private);
		ssize_t nb_write = write(self->fd, self->buffer, self->block_size);
		st_drive_generic_operation_stop(self->drive);

		if (nb_write < 0) {
			self->last_errno = errno;
			return -1;
		}

		self->position += nb_write;
		self->buffer_used = 0;
	}

	if (self->fd > -1) {
		static struct mtop eof = { MTWEOF, 1 };
		st_drive_generic_operation_start(self->drive_private);
		int failed = ioctl(self->fd, MTIOCTOP, &eof);
		st_drive_generic_operation_stop(self->drive);

		if (failed) {
			self->last_errno = errno;
			return -1;
		}

		self->drive->slot->tape->nb_files = self->drive->file_position + 1;
		st_drive_generic_update_position(self->drive);
		st_drive_generic_update_status(self->drive);

		self->fd = -1;
		self->drive->status = ST_DRIVE_LOADED_IDLE;
		self->drive_private->used_by_io = 0;
		st_log_write_all(st_log_level_debug, st_log_type_drive, "[%s | %s | #%td]: drive is close", self->drive->vendor, self->drive->model, self->drive - self->drive->changer->drives);
	}

	return 0;
}

void st_drive_io_writer_free(struct st_stream_writer * io) {
	if (!io)
		return;

	struct st_drive_io_writer * self = io->data;
	if (self) {
		self->fd = -1;
		free(self->buffer);
		free(self);
	}
	io->data = 0;
	io->ops = 0;
}

ssize_t st_drive_io_writer_get_available_size(struct st_stream_writer * io) {
	struct st_drive_io_writer * self = io->data;
	struct st_tape * tape = self->drive->slot->tape;

	if (!tape)
		return 0;

	if (self->eod_reached)
		return self->block_size - self->buffer_used;

	// only for test purpose
	// should be a multiple of blocksize
	// return 8589934592 - self->position;

	return tape->format->capacity - tape->end_position * tape->block_size - self->position;
}

ssize_t st_drive_io_writer_get_block_size(struct st_stream_writer * io) {
	struct st_drive_io_writer * self = io->data;
	return self->block_size;
}

int st_drive_io_writer_last_errno(struct st_stream_writer * io) {
	struct st_drive_io_writer * self = io->data;
	return self->last_errno;
}

struct st_stream_writer * st_drive_io_writer_new(struct st_drive * drive) {
	ssize_t block_size = drive->ops->get_block_size(drive);

	struct st_drive_generic * dr = drive->data;
	struct st_drive_io_writer * self = malloc(sizeof(struct st_drive_io_reader));
	self->fd = dr->fd_nst;
	self->buffer = malloc(block_size);
	self->buffer_used = 0;
	self->block_size = block_size;
	self->position = 0;
	self->last_errno = 0;
	self->eod_reached = false;
	self->drive = drive;
	self->drive_private = dr;

	dr->used_by_io = 1;

	struct st_stream_writer * writer = malloc(sizeof(struct st_stream_writer));
	writer->ops = &st_drive_io_writer_ops;
	writer->data = self;

	return writer;
}

ssize_t st_drive_io_writer_position(struct st_stream_writer * io) {
	if (!io || !io->data)
		return -1;

	struct st_drive_io_writer * self = io->data;
	return self->position;
}

ssize_t st_drive_io_writer_write(struct st_stream_writer * io, const void * buffer, ssize_t length) {
	if (!io || !buffer || length < 0)
		return -1;

	struct st_drive_io_writer * self = io->data;
	self->last_errno = 0;

	ssize_t buffer_available = self->block_size - self->buffer_used;
	if (buffer_available > length) {
		memcpy(self->buffer + self->buffer_used, buffer, length);

		self->buffer_used += length;
		self->position += length;
		return length;
	}

	memcpy(self->buffer + self->buffer_used, buffer, buffer_available);

	st_drive_generic_operation_start(self->drive_private);
	ssize_t nb_write = write(self->fd, self->buffer, self->block_size);
	st_drive_generic_operation_stop(self->drive);

	if (nb_write < 0) {
		switch (errno) {
			case ENOSPC:
				self->eod_reached = true;

				st_drive_generic_operation_start(self->drive_private);
				nb_write = write(self->fd, self->buffer, self->block_size);
				st_drive_generic_operation_stop(self->drive);

				if (nb_write == self->block_size)
					break;

			default:
				self->last_errno = errno;
				return -1;
		}
	}

	ssize_t nb_total_write = buffer_available;
	self->buffer_used = 0;
	self->position += buffer_available;
	self->drive->slot->tape->available_block--;

	const char * c_buffer = buffer;
	while (length - nb_total_write >= self->block_size) {
		st_drive_generic_operation_start(self->drive_private);
		nb_write = write(self->fd, c_buffer + nb_total_write, self->block_size);
		st_drive_generic_operation_stop(self->drive);

		if (nb_write < 0) {
			switch (errno) {
				case ENOSPC:
					self->eod_reached = true;

					st_drive_generic_operation_start(self->drive_private);
					nb_write = write(self->fd, c_buffer + nb_total_write, self->block_size);
					st_drive_generic_operation_stop(self->drive);

					if (nb_write == self->block_size)
						break;

				default:
					self->last_errno = errno;
					return -1;
			}
		}

		nb_total_write += nb_write;
		self->position += nb_write;
		self->drive->slot->tape->available_block--;
	}

	if (length == nb_total_write)
		return length;

	self->buffer_used = length - nb_total_write;
	self->position += self->buffer_used;
	memcpy(self->buffer, c_buffer + nb_total_write, self->buffer_used);

	return length;
}

