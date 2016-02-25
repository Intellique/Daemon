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
*  Copyright (C) 2013-2016, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// errno
#include <errno.h>
// dgettext
#include <libintl.h>
// free, malloc
#include <stdlib.h>
// memcpy, memmove
#include <string.h>
// ioctl
#include <sys/ioctl.h>
// struct mtget
#include <sys/mtio.h>
// close, read
#include <unistd.h>
// time
#include <time.h>

#include <libstoriqone/drive.h>
#include <libstoriqone/log.h>
#include <libstoriqone/media.h>
#include <libstoriqone/slot.h>
#include <libstoriqone-drive/time.h>

#include "io.h"

struct sodr_tape_drive_reader {
	int fd;

	char * buffer;
	ssize_t block_size;
	char * buffer_pos;

	ssize_t position;
	int file_position;
	bool end_of_file;

	int last_errno;

	struct so_drive * drive;
	struct so_media * media;
};

static int sodr_tape_drive_reader_close(struct so_stream_reader * io);
static bool sodr_tape_drive_reader_end_of_file(struct so_stream_reader * io);
static off_t sodr_tape_drive_reader_forward(struct so_stream_reader * io, off_t offset);
static void sodr_tape_drive_reader_free(struct so_stream_reader * io);
static ssize_t sodr_tape_drive_reader_get_available_size(struct so_stream_reader * sr);
static ssize_t sodr_tape_drive_reader_get_block_size(struct so_stream_reader * io);
static int sodr_tape_drive_reader_last_errno(struct so_stream_reader * io);
static ssize_t sodr_tape_drive_reader_peek(struct so_stream_reader * io, void * buffer, ssize_t length);
static ssize_t sodr_tape_drive_reader_position(struct so_stream_reader * io);
static ssize_t sodr_tape_drive_reader_read(struct so_stream_reader * io, void * buffer, ssize_t length);
static int sodr_tape_drive_reader_rewind(struct so_stream_reader * io);

static struct so_stream_reader_ops sodr_tape_drive_reader_ops = {
	.close              = sodr_tape_drive_reader_close,
	.end_of_file        = sodr_tape_drive_reader_end_of_file,
	.forward            = sodr_tape_drive_reader_forward,
	.free               = sodr_tape_drive_reader_free,
	.get_available_size = sodr_tape_drive_reader_get_available_size,
	.get_block_size     = sodr_tape_drive_reader_get_block_size,
	.last_errno         = sodr_tape_drive_reader_last_errno,
	.peek               = sodr_tape_drive_reader_peek,
	.position           = sodr_tape_drive_reader_position,
	.read               = sodr_tape_drive_reader_read,
	.rewind             = sodr_tape_drive_reader_rewind,
};


static int sodr_tape_drive_reader_close(struct so_stream_reader * io) {
	struct sodr_tape_drive_reader * self = io->data;

	if (self->fd > -1) {
		close(self->fd);

		self->fd = -1;
		self->buffer_pos = self->buffer + self->block_size;
		self->last_errno = 0;
	}

	return 0;
}

static bool sodr_tape_drive_reader_end_of_file(struct so_stream_reader * io) {
	struct sodr_tape_drive_reader * self = io->data;
	return self->end_of_file;
}

static off_t sodr_tape_drive_reader_forward(struct so_stream_reader * io, off_t offset) {
	struct sodr_tape_drive_reader * self = io->data;

	if (self->fd < 0)
		return -1;

	self->last_errno = 0;

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
		while (nb_records > 1000000) {
			int next_forward = nb_records > 8388607 ? 8388607 : nb_records;

			so_log_write(so_log_level_info,
				dgettext("storiqone-drive-tape", "[%s | %s | #%u]: forward media '%s' at file position #%u and record #%ld"),
				self->drive->vendor, self->drive->model, self->drive->index, self->media->name, self->file_position, nb_records);

			struct mtop forward = { MTFSR, next_forward };
			sodr_time_start();
			int failed = ioctl(self->fd, MTIOCTOP, &forward);
			sodr_time_stop(self->drive);

			if (failed != 0) {
				self->last_errno = errno;
				return failed;
			}

			nb_records -= next_forward;
			self->position += next_forward * self->block_size;
			nb_total_read += next_forward * self->block_size;
		}

		while (nb_records > 0) {
			sodr_time_start();
			ssize_t nb_read = read(self->fd, self->buffer, self->block_size);
			sodr_time_stop(self->drive);

			if (nb_read < 0) {
				self->last_errno = errno;
				return nb_read;
			}

			nb_records--;
			self->position += self->block_size;
			nb_total_read += self->block_size;
		}
	}

	if (nb_total_read == offset)
		return self->position;

	sodr_time_start();
	ssize_t nb_read = read(self->fd, self->buffer, self->block_size);
	sodr_time_stop(self->drive);

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

static void sodr_tape_drive_reader_free(struct so_stream_reader * io) {
	struct sodr_tape_drive_reader * self = io->data;
	if (self != NULL) {
		if (self->fd > -1)
			sodr_tape_drive_reader_close(io);

		free(self->buffer);
		free(self);
	}
	io->data = NULL;
	io->ops = NULL;
	free(io);
}

static ssize_t sodr_tape_drive_reader_get_available_size(struct so_stream_reader * io) {
	struct sodr_tape_drive_reader * self = io->data;

	ssize_t available = self->block_size - (self->buffer_pos - self->buffer);
	if (available > 0)
		return available;

	sodr_time_start();
	ssize_t nb_read = read(self->fd, self->buffer, self->block_size);
	sodr_time_stop(self->drive);

	if (nb_read < 0) {
		self->last_errno = errno;
		self->media->nb_read_errors++;
		return nb_read;
	} else if (nb_read > 0) {
		self->buffer_pos = self->buffer;
		self->media->nb_total_read++;
		return nb_read;
	} else {
		self->end_of_file = true;
		return 0;
	}
}

static ssize_t sodr_tape_drive_reader_get_block_size(struct so_stream_reader * io) {
	struct sodr_tape_drive_reader * self = io->data;
	return self->block_size;
}

struct so_stream_reader * sodr_tape_drive_reader_get_raw_reader(struct so_drive * drive, int fd, int file_position) {
	ssize_t block_size = sodr_tape_drive_get_block_size();
	if (block_size < 0)
		return NULL;

	drive->status = so_drive_status_reading;

	struct sodr_tape_drive_reader * self = malloc(sizeof(struct sodr_tape_drive_reader));
	self->fd = fd;

	self->buffer = malloc(block_size);
	self->block_size = block_size;
	self->buffer_pos = self->buffer + block_size;

	self->position = 0;
	self->file_position = file_position;
	self->last_errno = 0;
	self->end_of_file = false;

	self->drive = drive;
	self->media = drive->slot->media;

	struct so_stream_reader * reader = malloc(sizeof(struct so_stream_reader));
	reader->ops = &sodr_tape_drive_reader_ops;
	reader->data = self;

	self->media->read_count++;
	self->media->last_read = time(NULL);

	return reader;
}

static int sodr_tape_drive_reader_last_errno(struct so_stream_reader * io) {
	struct sodr_tape_drive_reader * self = io->data;
	return self->last_errno;
}

static ssize_t sodr_tape_drive_reader_peek(struct so_stream_reader * io, void * buffer, ssize_t length) {
	struct sodr_tape_drive_reader * self = io->data;
	if (self->fd < 0)
		return -1;

	self->last_errno = 0;

	if (self->end_of_file || length < 1)
		return 0;

	ssize_t available = self->block_size - (self->buffer_pos - self->buffer);
	if (available > 0) {
		ssize_t will_copy = available > length ? length : available;
		memcpy(buffer, self->buffer_pos, will_copy);

		return will_copy;
	}

	sodr_time_start();
	ssize_t nb_read = read(self->fd, self->buffer, self->block_size);
	sodr_time_stop(self->drive);

	if (nb_read < 0) {
		self->last_errno = errno;
		self->media->nb_read_errors++;
		return nb_read;
	} else if (nb_read > 0) {
		ssize_t will_copy = nb_read > length ? length : nb_read;
		memcpy(buffer, self->buffer_pos, will_copy);

		self->buffer_pos = self->buffer;
		self->media->nb_total_read++;
		return nb_read;
	} else {
		self->end_of_file = true;
		return 0;
	}
}

static ssize_t sodr_tape_drive_reader_position(struct so_stream_reader * io) {
	struct sodr_tape_drive_reader * self = io->data;
	return self->position;
}

static ssize_t sodr_tape_drive_reader_read(struct so_stream_reader * io, void * buffer, ssize_t length) {
	struct sodr_tape_drive_reader * self = io->data;
	if (self->fd < 0)
		return -1;

	self->last_errno = 0;

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
		sodr_time_start();
		ssize_t nb_read = read(self->fd, c_buffer + nb_total_read, self->block_size);
		sodr_time_stop(self->drive);

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

	sodr_time_start();
	ssize_t nb_read = read(self->fd, self->buffer, self->block_size);
	sodr_time_stop(self->drive);

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

static int sodr_tape_drive_reader_rewind(struct so_stream_reader * io) {
	struct sodr_tape_drive_reader * self = io->data;
	if (self->fd < 0)
		return -1;

	self->last_errno = 0;

	sodr_time_start();
	struct mtget status;
	int failed = ioctl(self->fd, MTIOCGET, &status);
	sodr_time_stop(self->drive);

	if (failed != 0) {
		self->last_errno = errno;
		return failed;
	}

	struct mtop bsr = { MTBSR, status.mt_blkno };
	sodr_time_start();
	failed = ioctl(self->fd, MTIOCTOP, &bsr);
	sodr_time_stop(self->drive);

	if (failed != 0)
		self->last_errno = errno;
	else {
		self->position = 0;
		self->buffer_pos = self->buffer + self->block_size;
		self->end_of_file = false;
	}

	return failed;
}

