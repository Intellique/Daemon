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

// errno
#include <errno.h>
// open
#include <fcntl.h>
// free, malloc
#include <stdlib.h>
// memcpy
#include <string.h>
// strings
#include <strings.h>
// open
#include <sys/stat.h>
// lseek, open
#include <sys/types.h>
// time
#include <time.h>
// lseek, write
#include <unistd.h>

#include <libstoriqone/drive.h>
#include <libstoriqone/log.h>
#include <libstoriqone/slot.h>
#include <libstoriqone-drive/time.h>

#include "device.h"
#include "io.h"

static ssize_t sodr_vtl_drive_writer_before_close(struct so_stream_writer * sfw, void * buffer, ssize_t length);
static int sodr_vtl_drive_writer_close(struct so_stream_writer * sfw);
static void sodr_vtl_drive_writer_free(struct so_stream_writer * sfw);
static ssize_t sodr_vtl_drive_writer_get_available_size(struct so_stream_writer * sfw);
static ssize_t sodr_vtl_drive_writer_get_block_size(struct so_stream_writer * sfw);
static int sodr_vtl_drive_writer_last_errno(struct so_stream_writer * sfw);
static ssize_t sodr_vtl_drive_writer_position(struct so_stream_writer * sfw);
static struct so_stream_reader * sodr_vtl_drive_writer_reopen(struct so_stream_writer * sfw);
static ssize_t sodr_vtl_drive_writer_write(struct so_stream_writer * sfw, const void * buffer, ssize_t length);

static struct so_stream_writer_ops sodr_vtl_drive_writer_ops = {
	.before_close       = sodr_vtl_drive_writer_before_close,
	.close              = sodr_vtl_drive_writer_close,
	.free               = sodr_vtl_drive_writer_free,
	.get_available_size = sodr_vtl_drive_writer_get_available_size,
	.get_block_size     = sodr_vtl_drive_writer_get_block_size,
	.last_errno         = sodr_vtl_drive_writer_last_errno,
	.position           = sodr_vtl_drive_writer_position,
	.reopen             = sodr_vtl_drive_writer_reopen,
	.write              = sodr_vtl_drive_writer_write,
};


static ssize_t sodr_vtl_drive_writer_before_close(struct so_stream_writer * sfw, void * buffer, ssize_t length) {
	if (sfw == NULL || buffer == NULL || length < 0)
		return -1;

	if (length == 0)
		return 0;

	struct sodr_vtl_drive_io * self = sfw->data;
	if (self->buffer_used > 0 && self->buffer_used < self->buffer_length) {
		if (self->buffer_used < length)
			length = self->buffer_used;

		bzero(buffer, length);

		bzero(self->buffer + self->buffer_used, length);
		self->buffer_used += length;

		if (self->buffer_used == self->buffer_length) {
			struct so_drive * dr = sodr_vtl_drive_get_device();

			sodr_time_start();
			ssize_t nb_write = write(self->fd, self->buffer, self->buffer_length);
			sodr_time_stop(dr);

			if (nb_write < 0) {
				self->last_errno = errno;
				self->media->nb_write_errors++;
				return -1;
			}

			self->buffer_used = 0;
			self->media->free_block--;
			self->media->nb_total_write++;
		}

		return length;
	}

	return 0;
}

static int sodr_vtl_drive_writer_close(struct so_stream_writer * sfw) {
	if (sfw == NULL)
		return -1;

	struct sodr_vtl_drive_io * self = sfw->data;
	self->last_errno = 0;

	if (self->fd < 0)
		return 0;

	struct so_drive * dr = sodr_vtl_drive_get_device();

	if (self->buffer_used > 0) {
		bzero(self->buffer + self->buffer_used, self->buffer_length - self->buffer_used);

		sodr_time_start();
		ssize_t nb_write = write(self->fd, self->buffer, self->buffer_length);
		sodr_time_stop(dr);

		if (nb_write < 0) {
			self->last_errno = errno;
			self->media->nb_write_errors++;
			return -1;
		}

		self->position += nb_write;
		self->buffer_used = 0;
		self->media->free_block--;
		self->media->nb_total_write++;
	}

	if (self->fd > -1) {
		int failed = sodr_vtl_drive_io_close(self);

		if (failed != 0)
			self->media->nb_write_errors++;
		else
			self->media->last_write = time(NULL);

		return failed;
	}

	return 0;
}

static void sodr_vtl_drive_writer_free(struct so_stream_writer * sfw) {
	if (sfw == NULL)
		return;

	struct sodr_vtl_drive_io * self = sfw->data;
	if (self->fd > -1)
		sodr_vtl_drive_writer_close(sfw);

	sodr_vtl_drive_io_free(self);
	free(sfw);
}

static ssize_t sodr_vtl_drive_writer_get_available_size(struct so_stream_writer * sfw) {
	if (sfw == NULL)
		return -1;

	struct sodr_vtl_drive_io * self = sfw->data;
	return self->media->free_block * self->buffer_length - self->buffer_used;
}

static ssize_t sodr_vtl_drive_writer_get_block_size(struct so_stream_writer * sfw) {
	if (sfw == NULL)
		return -1;

	struct sodr_vtl_drive_io * self = sfw->data;
	return self->buffer_length;
}

struct so_stream_writer * sodr_vtl_drive_writer_get_raw_writer(const char * filename) {
	struct so_drive * drive = sodr_vtl_drive_get_device();

	int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		so_log_write(so_log_level_error, "[%s %s %d] Error while opening file (%s) because %m", drive->vendor, drive->model, drive->index, filename);
		return NULL;
	}

	struct sodr_vtl_drive_io * self = malloc(sizeof(struct sodr_vtl_drive_io));
	self->fd = fd;
	self->buffer_length = drive->slot->media->block_size;
	self->buffer = malloc(self->buffer_length);
	self->buffer_used = 0;
	self->position = 0;
	self->last_errno = 0;
	self->media = drive->slot->media;

	struct so_stream_writer * writer = malloc(sizeof(struct so_stream_writer));
	writer->ops = &sodr_vtl_drive_writer_ops;
	writer->data = self;

	return writer;
}

static int sodr_vtl_drive_writer_last_errno(struct so_stream_writer * sfw) {
	if (sfw == NULL)
		return -1;

	struct sodr_vtl_drive_io * self = sfw->data;
	return self->last_errno;
}

static ssize_t sodr_vtl_drive_writer_position(struct so_stream_writer * sfw) {
	if (sfw == NULL)
		return -1;

	struct sodr_vtl_drive_io * self = sfw->data;
	return self->position;
}

static struct so_stream_reader * sodr_vtl_drive_writer_reopen(struct so_stream_writer * sfw) {
	struct sodr_vtl_drive_io * self = sfw->data;
	struct so_drive * dr = sodr_vtl_drive_get_device();

	if (self->buffer_used > 0) {
		bzero(self->buffer + self->buffer_used, self->buffer_length - self->buffer_used);

		sodr_time_start();
		ssize_t nb_write = write(self->fd, self->buffer, self->buffer_length);
		sodr_time_stop(dr);

		if (nb_write < 0) {
			self->last_errno = errno;
			self->media->nb_write_errors++;
			return NULL;
		}

		self->position += nb_write;
		self->buffer_used = 0;
		self->media->free_block--;
		self->media->nb_total_write++;
	}

	off_t new_position = lseek(self->fd, 0, SEEK_SET);
	if (new_position == (off_t) -1) {
		// error
		return NULL;
	}

	int fd = self->fd;
	self->fd = -1;

	return sodr_vtl_drive_reader_get_raw_reader(fd);
}

static ssize_t sodr_vtl_drive_writer_write(struct so_stream_writer * sfw, const void * buffer, ssize_t length) {
	if (sfw == NULL || buffer == NULL || length < 0)
		return -1;

	if (length == 0)
		return 0;

	struct so_drive * dr = sodr_vtl_drive_get_device();
	struct sodr_vtl_drive_io * self = sfw->data;
	self->last_errno = 0;

	ssize_t buffer_available = self->buffer_length - self->buffer_used;
	if (buffer_available > length) {
		memcpy(self->buffer + self->buffer_used, buffer, length);

		self->buffer_used += length;
		self->position += length;
		return length;
	}

	memcpy(self->buffer + self->buffer_used, buffer, buffer_available);

	sodr_time_start();
	ssize_t nb_write = write(self->fd, self->buffer, self->buffer_length);
	sodr_time_stop(dr);

	if (nb_write < 0) {
		self->last_errno = errno;
		self->media->nb_write_errors++;
		return -1;
	}

	self->media->free_block--;
	self->media->nb_total_write++;

	ssize_t nb_total_write = buffer_available;
	self->buffer_used = 0;
	self->position += buffer_available;

	const char * c_buffer = buffer;
	while (length - nb_total_write >= self->buffer_length) {
		sodr_time_start();
		nb_write = write(self->fd, c_buffer + nb_total_write, self->buffer_length);
		sodr_time_stop(dr);

		if (nb_write < 0) {
			self->last_errno = errno;
			self->media->nb_write_errors++;
			return -1;
		}

		nb_total_write += nb_write;
		self->position += nb_write;
		self->media->free_block--;
		self->media->nb_total_write++;
	}

	if (length == nb_total_write)
		return length;

	self->buffer_used = length - nb_total_write;
	self->position += self->buffer_used;
	memcpy(self->buffer, c_buffer + nb_total_write, self->buffer_used);

	return length;
}

