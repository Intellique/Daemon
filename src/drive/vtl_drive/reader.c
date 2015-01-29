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
*  Copyright (C) 2015, Guillaume Clercin <gclercin@intellique.com>           *
\****************************************************************************/

// errno
#include <errno.h>
// free, malloc
#include <stdlib.h>
// memcpy
#include <string.h>
// fstat
#include <sys/stat.h>
// fstat, lseek
#include <sys/types.h>
// time
#include <time.h>
// close, fstat, lseek, read
#include <unistd.h>

#include <libstoriqone/drive.h>
#include <libstoriqone/log.h>
#include <libstoriqone/slot.h>
#include <libstoriqone-drive/time.h>

#include "device.h"
#include "io.h"

static int sodr_vtl_drive_reader_close(struct so_stream_reader * sr);
static bool sodr_vtl_drive_reader_end_of_file(struct so_stream_reader * sr);
static off_t sodr_vtl_drive_reader_forward(struct so_stream_reader * sr, off_t offset);
static void sodr_vtl_drive_reader_free(struct so_stream_reader * sr);
static ssize_t sodr_vtl_drive_reader_get_block_size(struct so_stream_reader * sr);
static int sodr_vtl_drive_reader_last_errno(struct so_stream_reader * sr);
static ssize_t sodr_vtl_drive_reader_position(struct so_stream_reader * sr);
static ssize_t sodr_vtl_drive_reader_read(struct so_stream_reader * sr, void * buffer, ssize_t length);

static struct so_stream_reader_ops sodr_vtl_drive_reader_ops = {
	.close          = sodr_vtl_drive_reader_close,
	.end_of_file    = sodr_vtl_drive_reader_end_of_file,
	.forward        = sodr_vtl_drive_reader_forward,
	.free           = sodr_vtl_drive_reader_free,
	.get_block_size = sodr_vtl_drive_reader_get_block_size,
	.last_errno     = sodr_vtl_drive_reader_last_errno,
	.position       = sodr_vtl_drive_reader_position,
	.read           = sodr_vtl_drive_reader_read,
};


static int sodr_vtl_drive_reader_close(struct so_stream_reader * sr) {
	struct sodr_vtl_drive_io * self = sr->data;

	if (self->fd < 0)
		return 0;

	int failed = sodr_vtl_drive_io_close(self);

	if (failed != 0)
		self->media->nb_read_errors++;
	else
		self->media->last_read = time(NULL);

	return failed;
}

static bool sodr_vtl_drive_reader_end_of_file(struct so_stream_reader * sr) {
	struct sodr_vtl_drive_io * self = sr->data;

	struct stat st;
	int failed = fstat(self->fd, &st);
	if (failed != 0) {
		self->last_errno = errno;
		self->media->nb_read_errors++;
		return true;
	}

	return self->position == st.st_size;
}

static off_t sodr_vtl_drive_reader_forward(struct so_stream_reader * sr, off_t offset) {
	struct sodr_vtl_drive_io * self = sr->data;

	struct stat st;
	int failed = fstat(self->fd, &st);
	if (failed != 0) {
		self->last_errno = errno;
		self->media->nb_read_errors++;
		return -1;
	}

	if (self->buffer_used < self->buffer_length) {
		ssize_t available = self->buffer_length - self->buffer_used;

		if (available > offset) {
			self->buffer_used += offset;
			self->position += offset;
			return self->position;
		}

		offset -= available;
		self->buffer_used = self->buffer_length;
		self->position += available;
	}

	struct so_drive * dr = sodr_vtl_drive_get_device();

	off_t move = offset - (offset % self->buffer_length);
	if (move > 0) {
		sodr_time_start();
		off_t new_position = lseek(self->fd, move, SEEK_CUR);
		sodr_time_stop(dr);

		if (new_position == (off_t) -1) {
			self->last_errno = errno;
			self->media->nb_read_errors++;
			return -1;
		}

		self->position = new_position;
		self->media->last_read = time(NULL);
		self->media->nb_total_read++;
		offset -= move;
	}

	if (offset > 0) {
		sodr_time_start();
		ssize_t nb_read = read(self->fd, self->buffer, self->buffer_length);
		sodr_time_stop(dr);

		if (nb_read < 0) {
			self->last_errno = errno;
			self->media->nb_read_errors++;
			return -1;
		}

		self->buffer_used += offset;
		self->position += offset;
		self->media->last_read = time(NULL);
		self->media->nb_total_read++;
	}

	return self->position;
}

static void sodr_vtl_drive_reader_free(struct so_stream_reader * sr) {
	if (sr == NULL)
		return;

	struct sodr_vtl_drive_io * self = sr->data;
	if (self->fd > -1)
		sodr_vtl_drive_io_close(self);

	sodr_vtl_drive_io_free(self);
	free(sr);
}

static ssize_t sodr_vtl_drive_reader_get_block_size(struct so_stream_reader * sr) {
	struct sodr_vtl_drive_io * self = sr->data;
	return self->buffer_length;
}

struct so_stream_reader * sodr_vtl_drive_reader_get_raw_reader(int fd, int file_position) {
	struct so_drive * drive = sodr_vtl_drive_get_device();

	struct sodr_vtl_drive_io * self = malloc(sizeof(struct sodr_vtl_drive_io));
	self->fd = fd;
	self->buffer_used = self->buffer_length = drive->slot->media->block_size;
	self->buffer = malloc(self->buffer_length);
	self->position = 0;
	self->file_position = file_position;
	self->last_errno = 0;
	struct so_media * media = self->media = drive->slot->media;

	media->last_read = time(NULL);
	media->read_count++;
	media->operation_count++;

	struct so_stream_reader * reader = malloc(sizeof(struct so_stream_reader));
	reader->ops = &sodr_vtl_drive_reader_ops;
	reader->data = self;

	return reader;
}

static int sodr_vtl_drive_reader_last_errno(struct so_stream_reader * sr) {
	struct sodr_vtl_drive_io * self = sr->data;
	return self->last_errno;
}

static ssize_t sodr_vtl_drive_reader_position(struct so_stream_reader * sr) {
	struct sodr_vtl_drive_io * self = sr->data;
	return self->position;
}

static ssize_t sodr_vtl_drive_reader_read(struct so_stream_reader * sr, void * buffer, ssize_t length) {
	if (sr == NULL || buffer == NULL || length < 0)
		return -1;

	if (length == 0)
		return 0;

	struct sodr_vtl_drive_io * self = sr->data;
	self->last_errno = 0;

	ssize_t available = self->buffer_length - self->buffer_used;
	if (available > 0) {
		ssize_t will_copy = available;
		if (will_copy > length)
			will_copy = length;

		memcpy(buffer, self->buffer + self->buffer_used, will_copy);

		self->buffer_used += will_copy;
		self->position += will_copy;

		if (length == will_copy)
			return length;
	}

	struct so_drive * dr = sodr_vtl_drive_get_device();

	ssize_t nb_total_read = available;
	while (nb_total_read + self->buffer_length <= length) {
		sodr_time_start();
		ssize_t nb_read = read(self->fd, buffer + nb_total_read, self->buffer_length);
		sodr_time_stop(dr);

		if (nb_read < 0) {
			self->last_errno = errno;
			self->media->nb_read_errors++;
			return nb_read;
		}

		if (nb_read == 0)
			return nb_total_read;

		nb_total_read += nb_read;
		self->position += nb_read;
		self->media->last_read = time(NULL);
		self->media->nb_total_read++;
	}

	if (nb_total_read == length)
		return length;

	sodr_time_start();
	ssize_t nb_read = read(self->fd, self->buffer, self->buffer_length);
	sodr_time_stop(dr);

	if (nb_read < 0) {
		self->last_errno = errno;
		self->media->nb_read_errors++;
		return nb_read;
	}

	if (nb_read == 0)
		return nb_total_read;

	self->buffer_used = length - nb_total_read;
	memcpy(buffer + nb_total_read, self->buffer, self->buffer_used);
	self->position += self->buffer_used;
	self->media->last_read = time(NULL);
	self->media->nb_total_read++;

	return length;
}

