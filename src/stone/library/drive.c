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
*  Last modified: Sat, 17 Dec 2011 17:32:16 +0100                         *
\*************************************************************************/

// open
#include <fcntl.h>
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

#include <stone/database.h>
#include <stone/io.h>
#include <stone/library/drive.h>
#include <stone/library/tape.h>
#include <stone/log.h>

#include "common.h"
#include "scsi.h"

struct sa_drive_generic {
	int fd_nst;
	struct sa_database_connection * db_con;

	int used_by_io;

	struct timeval last_start;
	long double total_spent_time;
};

struct sa_drive_io_reader {
	int fd;
	char * buffer;
	ssize_t buffer_used;
	ssize_t block_size;
	ssize_t position;

	struct sa_drive * drive;
	struct sa_drive_generic * drive_private;
};

struct sa_drive_io_writer {
	int fd;
	char * buffer;
	ssize_t buffer_used;
	ssize_t block_size;
	ssize_t position;

	struct sa_drive * drive;
	struct sa_drive_generic * drive_private;
};

static int sa_drive_generic_eject(struct sa_drive * drive);
static int sa_drive_generic_eod(struct sa_drive * drive);
static ssize_t sa_drive_generic_get_block_size(struct sa_drive * drive);
static struct sa_stream_reader * sa_drive_generic_get_reader(struct sa_drive * drive);
static struct sa_stream_writer * sa_drive_generic_get_writer(struct sa_drive * drive);
static void sa_drive_generic_on_failed(struct sa_drive * drive, int verbose);
static void sa_drive_generic_operation_start(struct sa_drive_generic * dr);
static void sa_drive_generic_operation_stop(struct sa_drive * dr);
static void sa_drive_generic_reset(struct sa_drive * drive);
static int sa_drive_generic_rewind_file(struct sa_drive * drive);
static int sa_drive_generic_rewind_tape(struct sa_drive * drive);
static int sa_drive_generic_set_file_position(struct sa_drive * drive, int file_position);
static void sa_drive_generic_update_status(struct sa_drive * drive);
static void sa_drive_generic_update_status2(struct sa_drive * drive, enum sa_drive_status status);

static int sa_drive_io_reader_close(struct sa_stream_reader * io);
static void sa_drive_io_reader_free(struct sa_stream_reader * io);
static ssize_t sa_drive_io_reader_get_block_size(struct sa_stream_reader * io);
static struct sa_stream_reader * sa_drive_io_reader_new(struct sa_drive * drive);
static ssize_t sa_drive_io_reader_position(struct sa_stream_reader * io);
static ssize_t sa_drive_io_reader_read(struct sa_stream_reader * io, void * buffer, ssize_t length);

static int sa_drive_io_writer_close(struct sa_stream_writer * io);
static void sa_drive_io_writer_free(struct sa_stream_writer * io);
static ssize_t sa_drive_io_writer_get_block_size(struct sa_stream_writer * io);
static struct sa_stream_writer * sa_drive_io_writer_new(struct sa_drive * drive);
static ssize_t sa_drive_io_writer_position(struct sa_stream_writer * io);
static ssize_t sa_drive_io_writer_write(struct sa_stream_writer * io, void * buffer, ssize_t length);

static struct sa_drive_ops sa_drive_generic_ops = {
	.eject             = sa_drive_generic_eject,
	.eod               = sa_drive_generic_eod,
	.get_block_size    = sa_drive_generic_get_block_size,
	.get_reader        = sa_drive_generic_get_reader,
	.get_writer        = sa_drive_generic_get_writer,
	.reset             = sa_drive_generic_reset,
	.rewind_file       = sa_drive_generic_rewind_file,
	.rewind_tape       = sa_drive_generic_rewind_tape,
	.set_file_position = sa_drive_generic_set_file_position,
};

static struct sa_stream_reader_ops sa_drive_io_reader_ops = {
	.close          = sa_drive_io_reader_close,
	.free           = sa_drive_io_reader_free,
	.get_block_size = sa_drive_io_reader_get_block_size,
	.position       = sa_drive_io_reader_position,
	.read           = sa_drive_io_reader_read,
};

static struct sa_stream_writer_ops sa_drive_io_writer_ops = {
	.close          = sa_drive_io_writer_close,
	.free           = sa_drive_io_writer_free,
	.get_block_size = sa_drive_io_writer_get_block_size,
	.position       = sa_drive_io_writer_position,
	.write          = sa_drive_io_writer_write,
};


int sa_drive_generic_eject(struct sa_drive * drive) {
	struct sa_drive_generic * self = drive->data;

	sa_log_write_all(sa_log_level_info, "Drive (%s | %s | #%td): rewind tape and put the drive offline", drive->vendor, drive->model, drive - drive->changer->drives);

	sa_drive_generic_update_status2(drive, SA_DRIVE_UNLOADING);

	static struct mtop eject = { MTOFFL, 1 };
	sa_drive_generic_operation_start(self);
	int failed = ioctl(self->fd_nst, MTIOCTOP, &eject);
	sa_drive_generic_operation_stop(drive);

	sa_log_write_all(failed ? sa_log_level_error : sa_log_level_debug, "Drive (%s | %s | #%td): rewind tape and put the drive offline, finish with code = %d", drive->vendor, drive->model, drive - drive->changer->drives, failed);

	sa_drive_generic_update_status(drive);

	return failed;
}

int sa_drive_generic_eod(struct sa_drive * drive) {
	struct sa_drive_generic * self = drive->data;

	sa_log_write_all(sa_log_level_info, "Drive (%s | %s | #%td): goto end of tape", drive->vendor, drive->model, drive - drive->changer->drives);

	sa_drive_generic_update_status2(drive, SA_DRIVE_POSITIONING);

	static struct mtop eod = { MTEOM, 1 };
	sa_drive_generic_operation_start(self);
	int failed = ioctl(self->fd_nst, MTIOCTOP, &eod);
	sa_drive_generic_operation_stop(drive);

	if (failed) {
		sa_drive_generic_on_failed(drive, 1);
		sa_drive_generic_update_status(drive);
		sa_drive_generic_operation_start(self);
		failed = ioctl(self->fd_nst, MTIOCTOP, &eod);
		sa_drive_generic_operation_stop(drive);
	}

	sa_log_write_all(failed ? sa_log_level_error : sa_log_level_debug, "Drive (%s | %s | #%td): goto end of tape, finish with code = %d", drive->vendor, drive->model, drive - drive->changer->drives, failed);

	sa_drive_generic_update_status(drive);

	return failed;
}

ssize_t sa_drive_generic_get_block_size(struct sa_drive * drive) {
	if (drive->block_size < 1) {
		struct sa_tape * tape = drive->slot->tape;

		if (tape->block_size > 0)
			return tape->block_size;

		sa_drive_generic_update_status(drive);

		if (!drive->is_bottom_of_tape) {
			if (drive->file_position > 0)
				sa_drive_generic_rewind_file(drive);
			else
				sa_drive_generic_rewind_tape(drive);
		}

		struct sa_drive_generic * self = drive->data;

		unsigned int i;
		ssize_t nb_read;
		ssize_t block_size = 1 << 16;
		char * buffer = malloc(block_size);

		for (i = 0; i < 4; i++) {
			sa_drive_generic_operation_start(self);
			nb_read = read(self->fd_nst, buffer, block_size);
			sa_drive_generic_operation_stop(drive);

			tape->read_count++;

			sa_drive_generic_update_status(drive);

			if (drive->block_number < 0)
				break;

			if (drive->file_position > 0)
				sa_drive_generic_rewind_file(drive);
			else
				sa_drive_generic_rewind_tape(drive);

			if (nb_read > 0) {
				free(buffer);
				return tape->block_size = nb_read;
			}

			block_size <<= 1;
			buffer = realloc(buffer, block_size);
		}

		free(buffer);

		if (tape->format)
			return tape->block_size = tape->format->block_size;

		sa_log_write_all(sa_log_level_error, "Drive (%s | %s | #%td): failed to detect block size", drive->vendor, drive->model, drive - drive->changer->drives);
	}

	return drive->block_size;
}

struct sa_stream_reader * sa_drive_generic_get_reader(struct sa_drive * drive) {
	sa_drive_generic_update_status(drive);

	struct sa_drive_generic * self = drive->data;
	if (drive->is_door_opened || self->used_by_io) {
		sa_log_write_all(sa_log_level_debug, "Drive (%s | %s | #%td): drive is not free for reading on it", drive->vendor, drive->model, drive - drive->changer->drives);
		return 0;
	}

	drive->slot->tape->read_count++;
	sa_drive_generic_update_status2(drive, SA_DRIVE_READING);

	return sa_drive_io_reader_new(drive);
}

struct sa_stream_writer * sa_drive_generic_get_writer(struct sa_drive * drive) {
	sa_drive_generic_update_status(drive);

	struct sa_drive_generic * self = drive->data;
	if (drive->is_door_opened || self->used_by_io) {
		sa_log_write_all(sa_log_level_debug, "Drive (%s | %s | #%td): drive is not free for writing on it", drive->vendor, drive->model, drive - drive->changer->drives);
		return 0;
	}

	if (!drive->is_writable) {
		sa_log_write_all(sa_log_level_debug, "Drive (%s | %s | #%td): tape is not writable (Write protec enabled)", drive->vendor, drive->model, drive - drive->changer->drives);
		return 0;
	}

	drive->slot->tape->write_count++;
	sa_drive_generic_update_status2(drive, SA_DRIVE_WRITING);

	return sa_drive_io_writer_new(drive);
}

void sa_drive_generic_on_failed(struct sa_drive * drive, int verbose) {
	struct sa_drive_generic * self = drive->data;

	if (verbose)
		sa_log_write_all(sa_log_level_debug, "Drive (%s | %s | #%td): Try to recover an error", drive->vendor, drive->model, drive - drive->changer->drives);
	close(self->fd_nst);
	sleep(20);
	self->fd_nst = open(drive->device, O_RDWR | O_NONBLOCK);
}

void sa_drive_generic_operation_start(struct sa_drive_generic * dr) {
	gettimeofday(&dr->last_start, 0);
}

void sa_drive_generic_operation_stop(struct sa_drive * drive) {
	struct sa_drive_generic * self = drive->data;

	struct timeval finish;
	gettimeofday(&finish, 0);

	drive->operation_duration += (finish.tv_sec - self->last_start.tv_sec) + ((double) (finish.tv_usec - self->last_start.tv_usec)) / 1000000;
}

void sa_drive_generic_reset(struct sa_drive * drive) {
	struct sa_drive_generic * self = drive->data;

	close(self->fd_nst);
	sleep(1);
	self->fd_nst = open(drive->device, O_RDWR | O_NONBLOCK);

	static struct mtop nop = { MTNOP, 1 };

	int i, failed = 1;
	for (i = 0; i < 120 && failed; i++) {
		if (self->fd_nst > -1) {
			sa_drive_generic_operation_start(self);
			failed = ioctl(self->fd_nst, MTIOCTOP, &nop);
			sa_drive_generic_operation_stop(drive);
		}

		if (failed) {
			if (self->fd_nst > -1)
				close(self->fd_nst);

			sleep(1);

			self->fd_nst = open(drive->device, O_RDWR | O_NONBLOCK);
		}
	}

	drive->nb_files = 0;
	sa_drive_generic_update_status(drive);
}

int sa_drive_generic_rewind_file(struct sa_drive * drive) {
	struct sa_drive_generic * self = drive->data;

	sa_log_write_all(sa_log_level_info, "Drive (%s | %s | #%td): rewind file", drive->vendor, drive->model, drive - drive->changer->drives);

	sa_drive_generic_update_status2(drive, SA_DRIVE_REWINDING);

	static struct mtop rewind = { MTBSFM, 1 };
	sa_drive_generic_operation_start(self);
	int failed = ioctl(self->fd_nst, MTIOCTOP, &rewind);
	sa_drive_generic_operation_stop(drive);

	unsigned int i;
	for (i = 0; i < 3 && failed; i++) {
		sa_drive_generic_on_failed(drive, 1);
		sa_drive_generic_update_status(drive);
		sa_drive_generic_operation_start(self);
		failed = ioctl(self->fd_nst, MTIOCTOP, &rewind);
		sa_drive_generic_operation_stop(drive);
	}

	sa_log_write_all(failed ? sa_log_level_error : sa_log_level_debug, "Drive (%s | %s | #%td): rewind file, finish with code = %d", drive->vendor, drive->model, drive - drive->changer->drives, failed);

	sa_drive_generic_update_status(drive);

	return failed;
}

int sa_drive_generic_rewind_tape(struct sa_drive * drive) {
	struct sa_drive_generic * self = drive->data;

	sa_log_write_all(sa_log_level_info, "Drive (%s | %s | #%td): rewind tape", drive->vendor, drive->model, drive - drive->changer->drives);

	sa_drive_generic_update_status2(drive, SA_DRIVE_REWINDING);

	static struct mtop rewind = { MTREW, 1 };
	sa_drive_generic_operation_start(self);
	int failed = ioctl(self->fd_nst, MTIOCTOP, &rewind);
	sa_drive_generic_operation_stop(drive);

	unsigned int i;
	for (i = 0; i < 3 && failed; i++) {
		sa_drive_generic_on_failed(drive, 1);
		sa_drive_generic_update_status(drive);
		sa_drive_generic_operation_start(self);
		failed = ioctl(self->fd_nst, MTIOCTOP, &rewind);
		sa_drive_generic_operation_stop(drive);
	}

	sa_log_write_all(failed ? sa_log_level_error : sa_log_level_debug, "Drive (%s | %s | #%td): rewind tape, finish with code = %d", drive->vendor, drive->model, drive - drive->changer->drives, failed);

	sa_drive_generic_update_status(drive);

	return failed;
}

int sa_drive_generic_set_file_position(struct sa_drive * drive, int file_position) {
	sa_log_write_all(sa_log_level_info, "Drive (%s | %s | #%td): positioning tape to position = %d", drive->vendor, drive->model, drive - drive->changer->drives, file_position);

	int failed = sa_drive_generic_rewind_tape(drive);

	if (failed)
		return failed;

	sa_drive_generic_update_status2(drive, SA_DRIVE_POSITIONING);

	struct sa_drive_generic * self = drive->data;
	struct mtop fsr = { MTFSF, file_position };
	sa_drive_generic_operation_start(self);
	failed = ioctl(self->fd_nst, MTIOCTOP, &fsr);
	sa_drive_generic_operation_stop(drive);

	sa_log_write_all(failed ? sa_log_level_error : sa_log_level_debug, "Drive (%s | %s | #%td): positioning tape to position = %d, finish with code = %d", drive->vendor, drive->model, drive - drive->changer->drives, file_position, failed);

	return failed;
}

void sa_drive_generic_update_status(struct sa_drive * drive) {
	struct sa_drive_generic * self = drive->data;

	struct mtget status;
	sa_drive_generic_operation_start(self);
	int failed = ioctl(self->fd_nst, MTIOCGET, &status);
	sa_drive_generic_operation_stop(drive);

	unsigned int i;
	for (i = 0; i < 5 && failed; i++) {
		sa_drive_generic_on_failed(drive, 1);
		sa_drive_generic_operation_start(self);
		failed = ioctl(self->fd_nst, MTIOCGET, &status);
		sa_drive_generic_operation_stop(drive);
	}

	if (failed) {
		drive->status = SA_DRIVE_ERROR;

		static struct mtop reset = { MTRESET, 1 };
		sa_drive_generic_operation_start(self);
		failed = ioctl(self->fd_nst, MTIOCTOP, &reset);
		sa_drive_generic_operation_stop(drive);

		if (!failed) {
			sa_drive_generic_operation_start(self);
			failed = ioctl(self->fd_nst, MTIOCGET, &status);
			sa_drive_generic_operation_stop(drive);
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
		unsigned char density_code = (status.mt_dsreg & MT_ST_DENSITY_MASK) >> MT_ST_DENSITY_SHIFT;
		if (density_code > 0)
			drive->density_code = density_code;

		sa_drive_generic_update_status2(drive, drive->is_door_opened ? SA_DRIVE_EMPTY_IDLE : SA_DRIVE_LOADED_IDLE);
	} else {
		sa_drive_generic_update_status2(drive, SA_DRIVE_ERROR);
	}
}

void sa_drive_generic_update_status2(struct sa_drive * drive, enum sa_drive_status status) {
	drive->status = status;

	struct sa_drive_generic * self = drive->data;
	if (drive->id > -1)
		self->db_con->ops->sync_drive(self->db_con, drive);
}

void sa_drive_setup(struct sa_drive * drive) {
	int fd = open(drive->device, O_RDWR | O_NONBLOCK);

	struct sa_drive_generic * self = malloc(sizeof(struct sa_drive_generic));
	self->fd_nst = fd;
	self->used_by_io = 0;
	self->total_spent_time = 0;

	int fd_device = open(drive->scsi_device, O_RDWR | O_NONBLOCK);
	sa_scsi_tapeinfo(fd_device, drive);
	close(fd_device);

	drive->ops = &sa_drive_generic_ops;
	drive->data = self;
	drive->density_code = 0;

	sa_drive_generic_update_status(drive);

	struct sa_database * db = sa_db_get_default_db();
	if (db) {
		self->db_con = db->ops->connect(db, 0);
		if (!self->db_con)
			sa_log_write_all(sa_log_level_error, "Drive (%s | %s | #%td): failed to connect to default database", drive->vendor, drive->model, drive - drive->changer->drives);
	} else {
		sa_log_write_all(sa_log_level_warning, "Drive (%s | %s | #%td): there is no default database so drive is not able to synchronize with one database", drive->vendor, drive->model, drive - drive->changer->drives);
	}
}


int sa_drive_io_reader_close(struct sa_stream_reader * io) {
	if (!io || !io->data)
		return -1;

	struct sa_drive_io_reader * self = io->data;
	self->fd = -1;
	self->buffer_used = 0;

	sa_drive_generic_update_status2(self->drive, SA_DRIVE_LOADED_IDLE);
	self->drive_private->used_by_io = 0;

	return 0;
}

void sa_drive_io_reader_free(struct sa_stream_reader * io) {
	if (!io)
		return;

	struct sa_drive_io_reader * self = io->data;
	if (self) {
		self->fd = -1;
		free(self->buffer);
		free(self);
	}
	io->data = 0;
	io->ops = 0;
}

ssize_t sa_drive_io_reader_get_block_size(struct sa_stream_reader * io) {
	struct sa_drive_io_reader * self = io->data;
	return self->block_size;
}

struct sa_stream_reader * sa_drive_io_reader_new(struct sa_drive * drive) {
	ssize_t block_size = drive->ops->get_block_size(drive);

	struct sa_drive_generic * dr = drive->data;
	struct sa_drive_io_reader * self = malloc(sizeof(struct sa_drive_io_reader));
	self->fd = dr->fd_nst;
	self->buffer = malloc(block_size);
	self->buffer_used = 0;
	self->block_size = block_size;
	self->position = 0;
	self->drive = drive;
	self->drive_private = dr;

	dr->used_by_io = 1;

	struct sa_stream_reader * reader = malloc(sizeof(struct sa_stream_reader));
	reader->ops = &sa_drive_io_reader_ops;
	reader->data = self;

	return reader;
}

ssize_t sa_drive_io_reader_position(struct sa_stream_reader * io) {
	if (!io || !io->data)
		return -1;

	struct sa_drive_io_reader * self = io->data;
	return self->position;
}

ssize_t sa_drive_io_reader_read(struct sa_stream_reader * io, void * buffer, ssize_t length) {
	if (!io || !buffer || length < 0)
		return -1;

	struct sa_drive_io_reader * self = io->data;
	if (self->fd < 0)
		return -1;

	ssize_t nb_total_read = 0;
	if (self->buffer_used > 0) {
		ssize_t nb_copy = self->buffer_used >= length ? length : self->buffer_used;
		memcpy(buffer, self->buffer, nb_copy);

		if (nb_copy < self->buffer_used)
			memmove(self->buffer, self->buffer + nb_copy, self->buffer_used - nb_copy);
		self->buffer_used -= nb_copy;
		self->position += nb_copy;

		if (nb_copy == length)
			return length;

		nb_total_read += nb_copy;
	}

	char * c_buffer = buffer;
	while (length - nb_total_read >= self->block_size) {
		sa_drive_generic_operation_start(self->drive_private);
		ssize_t nb_read = read(self->fd, c_buffer + nb_total_read, self->block_size);
		sa_drive_generic_operation_stop(self->drive);

		if (nb_read > 0) {
			self->position += nb_read;
			nb_total_read += nb_read;
		} else if (nb_read == 0)
			return nb_total_read;
		else
			return -1;
	}

	if (length == nb_total_read)
		return length;

	sa_drive_generic_operation_start(self->drive_private);
	ssize_t nb_read = read(self->fd, self->buffer, self->block_size);
	sa_drive_generic_operation_stop(self->drive);
	if (nb_read < 0)
		return -1;

	if (nb_read > 0) {
		self->buffer_used = nb_read;

		ssize_t nb_copy = length - nb_total_read;
		memcpy(c_buffer + nb_total_read, self->buffer, nb_copy);

		if (nb_copy < self->buffer_used)
			memmove(self->buffer, self->buffer + nb_copy, self->buffer_used - nb_copy);
		self->buffer_used -= nb_copy;
		self->position += nb_copy;

		if (nb_copy == length)
			return length;

		nb_total_read += nb_copy;
	}

	return nb_total_read;
}


int sa_drive_io_writer_close(struct sa_stream_writer * io) {
	if (!io || !io->data)
		return -1;

	struct sa_drive_io_writer * self = io->data;
	if (self->buffer_used) {
		bzero(self->buffer + self->buffer_used, self->block_size - self->buffer_used);

		ssize_t nb_write = write(self->fd, self->buffer, self->buffer_used);
		if (nb_write < 0)
			return -1;

		self->position += nb_write;
		self->buffer_used = 0;
	}

	if (self->fd > -1) {
		static struct mtop eof = { MTWEOF, 1 };
		int failed = ioctl(self->fd, MTIOCTOP, &eof);
		if (failed)
			return -1;
	}

	self->fd = -1;
	sa_drive_generic_update_status2(self->drive, SA_DRIVE_LOADED_IDLE);
	self->drive_private->used_by_io = 0;

	return 0;
}

void sa_drive_io_writer_free(struct sa_stream_writer * io) {
	if (!io)
		return;

	struct sa_drive_io_writer * self = io->data;
	if (self) {
		self->fd = -1;
		free(self->buffer);
		free(self);
	}
	io->data = 0;
	io->ops = 0;
}

ssize_t sa_drive_io_writer_get_block_size(struct sa_stream_writer * io) {
	struct sa_drive_io_writer * self = io->data;
	return self->block_size;
}

struct sa_stream_writer * sa_drive_io_writer_new(struct sa_drive * drive) {
	ssize_t block_size = drive->ops->get_block_size(drive);

	struct sa_drive_generic * dr = drive->data;
	struct sa_drive_io_writer * self = malloc(sizeof(struct sa_drive_io_reader));
	self->fd = dr->fd_nst;
	self->buffer = malloc(block_size);
	self->buffer_used = 0;
	self->block_size = block_size;
	self->position = 0;
	self->drive = drive;
	self->drive_private = dr;

	dr->used_by_io = 1;

	struct sa_stream_writer * writer = malloc(sizeof(struct sa_stream_writer));
	writer->ops = &sa_drive_io_writer_ops;
	writer->data = self;

	return writer;
}

ssize_t sa_drive_io_writer_position(struct sa_stream_writer * io) {
	if (!io || !io->data)
		return -1;

	struct sa_drive_io_writer * self = io->data;
	return self->position;
}

ssize_t sa_drive_io_writer_write(struct sa_stream_writer * io, void * buffer, ssize_t length) {
	if (!io || !buffer || length < 0)
		return -1;

	struct sa_drive_io_writer * self = io->data;

	ssize_t buffer_available = self->block_size - self->buffer_used;
	if (buffer_available > length) {
		memcpy(self->buffer + self->buffer_used, buffer, length);

		self->buffer_used += length;
		self->position += length;
		return length;
	}

	memcpy(self->buffer + self->buffer_used, buffer, buffer_available);

	ssize_t nb_total_write = write(self->fd, self->buffer, self->block_size);
	if (nb_total_write < 0)
		return -1;

	self->buffer_used = 0;
	self->position += buffer_available;

	char * c_buffer = buffer;
	while (length - nb_total_write >= self->block_size) {
		sa_drive_generic_operation_start(self->drive_private);
		ssize_t nb_write = write(self->fd, c_buffer + nb_total_write, self->block_size);
		sa_drive_generic_operation_stop(self->drive);
		if (nb_write < 0)
			return -1;

		nb_total_write += nb_write;
		self->position += nb_write;
	}

	if (length == nb_total_write)
		return length;

	self->buffer_used = length - nb_total_write;
	self->position += self->buffer_used;
	memcpy(self->buffer, c_buffer + nb_total_write, self->buffer_used);

	return length;
}

