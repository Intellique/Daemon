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

// errno
#include <errno.h>
// dgettext
#include <libintl.h>
// free, malloc
#include <stdlib.h>
// memcpy
#include <string.h>
// bzero
#include <strings.h>
// ioctl
#include <sys/ioctl.h>
// struct mtget
#include <sys/mtio.h>
// time
#include <time.h>
// close, write
#include <unistd.h>

#include <libstoriqone/drive.h>
#include <libstoriqone/file.h>
#include <libstoriqone/log.h>
#include <libstoriqone/media.h>
#include <libstoriqone/slot.h>
#include <libstoriqone-drive/time.h>

#include "device.h"
#include "io.h"
#include "scsi.h"

struct sodr_tape_drive_writer {
	int fd;

	char * buffer;
	ssize_t buffer_used;
	ssize_t block_size;
	ssize_t position;
	int file_position;

	bool end_of_tape_warning;
	int last_errno;

	struct so_drive * drive;
	struct so_media * media;
};

static ssize_t sodr_tape_drive_writer_before_close(struct so_stream_writer * sw, void * buffer, ssize_t length);
static int sodr_tape_drive_writer_close(struct so_stream_writer * sw);
static int sodr_tape_drive_file_position(struct so_stream_writer * sw);
static void sodr_tape_drive_writer_free(struct so_stream_writer * sw);
static ssize_t sodr_tape_drive_writer_get_available_size(struct so_stream_writer * sw);
static ssize_t sodr_tape_drive_writer_get_block_size(struct so_stream_writer * sw);
static int sodr_tape_drive_writer_last_errno(struct so_stream_writer * sw);
static ssize_t sodr_tape_drive_writer_position(struct so_stream_writer * sw);
static struct so_stream_reader * sodr_tape_drive_writer_reopen(struct so_stream_writer * sw);
static ssize_t sodr_tape_drive_writer_write(struct so_stream_writer * sw, const void * buffer, ssize_t length);

static struct so_stream_writer_ops sodr_tape_drive_writer_ops = {
	.before_close       = sodr_tape_drive_writer_before_close,
	.close              = sodr_tape_drive_writer_close,
	.file_position      = sodr_tape_drive_file_position,
	.free               = sodr_tape_drive_writer_free,
	.get_available_size = sodr_tape_drive_writer_get_available_size,
	.get_block_size     = sodr_tape_drive_writer_get_block_size,
	.last_errno         = sodr_tape_drive_writer_last_errno,
	.position           = sodr_tape_drive_writer_position,
	.reopen             = sodr_tape_drive_writer_reopen,
	.write              = sodr_tape_drive_writer_write,
};


static ssize_t sodr_tape_drive_writer_before_close(struct so_stream_writer * sw, void * buffer, ssize_t length) {
	if (sw == NULL || buffer == NULL || length < 0)
		return -1;

	struct sodr_tape_drive_writer * self = sw->data;
	if (self->buffer_used > 0 && self->buffer_used < self->block_size) {
		ssize_t will_copy = self->block_size - self->buffer_used;
		if (will_copy > length)
			will_copy = length;

		bzero(buffer, will_copy);

		bzero(self->buffer + self->buffer_used, will_copy);
		self->buffer_used += will_copy;
		self->position += will_copy;

		if (self->buffer_used == self->block_size) {
			sodr_time_start();
			ssize_t nb_write = write(self->fd, self->buffer, self->block_size);
			sodr_time_stop(self->drive);

			if (nb_write < 0) {
				switch (errno) {
					case ENOSPC:
						sodr_time_start();
						nb_write = write(self->fd, self->buffer, self->block_size);
						sodr_time_stop(self->drive);

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

static int sodr_tape_drive_writer_close(struct so_stream_writer * sw) {
	if (sw == NULL || sw->data == NULL)
		return -1;

	struct sodr_tape_drive_writer * self = sw->data;
	self->last_errno = 0;

	if (self->buffer_used > 0) {
		bzero(self->buffer + self->buffer_used, self->block_size - self->buffer_used);

		sodr_time_start();
		ssize_t nb_write = write(self->fd, self->buffer, self->block_size);
		sodr_time_stop(self->drive);

		if (nb_write < 0) {
			switch (errno) {
				case ENOSPC:
					sodr_time_start();
					nb_write = write(self->fd, self->buffer, self->block_size);
					sodr_time_stop(self->drive);

					if (nb_write == self->block_size)
						break;

				default:
					so_log_write(so_log_level_error,
						dgettext("storiqone-drive-tape", "[%s | %s | #%u]: error while writing because %m"),
						self->drive->vendor, self->drive->model, self->drive->index);

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
		sodr_time_start();
		int failed = ioctl(self->fd, MTIOCTOP, &eof);
		sodr_time_stop(self->drive);

		self->media->last_write = time(NULL);

		if (failed) {
			so_log_write(so_log_level_error,
				dgettext("storiqone-drive-tape", "[%s | %s | #%u]: error while writing end of file because %m"),
				self->drive->vendor, self->drive->model, self->drive->index);

			self->last_errno = errno;
			self->media->nb_write_errors++;
			return -1;
		}

		sodr_time_start();
		close(self->fd);
		sodr_time_stop(self->drive);

		self->fd = -1;
		so_log_write(so_log_level_debug,
			dgettext("storiqone-drive-tape", "[%s | %s | #%u]: drive is closed"),
			self->drive->vendor, self->drive->model, self->drive->index);
	}

	return 0;
}

static int sodr_tape_drive_file_position(struct so_stream_writer * sw) {
	struct sodr_tape_drive_writer * self = sw->data;
	return self->file_position;
}

static void sodr_tape_drive_writer_free(struct so_stream_writer * sw) {
	if (sw == NULL)
		return;

	struct sodr_tape_drive_writer * self = sw->data;
	if (self != NULL) {
		if (self->fd > -1)
			sodr_tape_drive_writer_close(sw);

		free(self->buffer);
		free(self);
	}
	sw->data = NULL;
	sw->ops = NULL;

	free(sw);
}

static ssize_t sodr_tape_drive_writer_get_available_size(struct so_stream_writer * sw) {
	struct sodr_tape_drive_writer * self = sw->data;

	if (self->media == NULL)
		return 0;

	return self->media->free_block * self->media->block_size;
}

static ssize_t sodr_tape_drive_writer_get_block_size(struct so_stream_writer * sw) {
	struct sodr_tape_drive_writer * self = sw->data;
	return self->block_size;
}

static int sodr_tape_drive_writer_last_errno(struct so_stream_writer * sw) {
	struct sodr_tape_drive_writer * self = sw->data;
	return self->last_errno;
}

struct so_stream_writer * sodr_tape_drive_writer_get_raw_writer(struct so_drive * drive, int fd, int file_position) {
	ssize_t block_size = sodr_tape_drive_get_block_size();

	drive->status = so_drive_status_writing;

	struct sodr_tape_drive_writer * self = malloc(sizeof(struct sodr_tape_drive_writer));
	self->fd = fd;
	self->buffer = malloc(block_size);
	self->buffer_used = 0;
	self->block_size = block_size;
	self->position = 0;
	self->file_position = file_position;
	self->last_errno = 0;
	self->end_of_tape_warning = false;

	self->drive = drive;
	self->media = drive->slot->media;

	struct so_stream_writer * writer = malloc(sizeof(struct so_stream_writer));
	writer->ops = &sodr_tape_drive_writer_ops;
	writer->data = self;

	self->media->write_count++;
	self->media->last_write = time(NULL);

	return writer;
}

static ssize_t sodr_tape_drive_writer_position(struct so_stream_writer * sw) {
	if (sw == NULL || sw->data == NULL)
		return -1;

	struct sodr_tape_drive_writer * self = sw->data;
	return self->position;
}

static struct so_stream_reader * sodr_tape_drive_writer_reopen(struct so_stream_writer * sw) {
	if (sw == NULL || sw->data == NULL)
		return NULL;

	int failed = sodr_tape_drive_writer_close(sw);
	if (failed != 0)
		return NULL;

	struct sodr_tape_drive_writer * self = sw->data;
	return sodr_tape_drive_reader_get_raw_reader(self->drive, self->fd, self->file_position);
}

static ssize_t sodr_tape_drive_writer_write(struct so_stream_writer * sw, const void * buffer, ssize_t length) {
	if (sw == NULL || buffer == NULL || length < 0)
		return -1;

	struct sodr_tape_drive_writer * self = sw->data;
	self->last_errno = 0;

	if (length == 0)
		return 0;

	ssize_t buffer_available = self->block_size - self->buffer_used;
	if (buffer_available > length) {
		memcpy(self->buffer + self->buffer_used, buffer, length);

		self->buffer_used += length;
		self->position += length;
		return length;
	}

	memcpy(self->buffer + self->buffer_used, buffer, buffer_available);

	sodr_time_start();
	ssize_t nb_write = write(self->fd, self->buffer, self->block_size);
	sodr_time_stop(self->drive);

	if (nb_write < 0) {
		switch (errno) {
			case ENOSPC:
				if (!self->end_of_tape_warning) {
					self->end_of_tape_warning = true;

					int fd = sodr_tape_drive_open_scsi();
					sodr_tape_drive_scsi_size_available(fd, self->media);
					close(fd);

					char str_remaining_space[16];
					ssize_t remaining_space = self->media->block_size * self->media->free_block;
					so_file_convert_size_to_string(remaining_space, str_remaining_space, 16);

					so_log_write(so_log_level_info,
						dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Space remaing on tape '%s': %s"),
						self->drive->vendor, self->drive->model, self->drive->index, self->media->label, str_remaining_space);
				}

				sodr_time_start();
				nb_write = write(self->fd, self->buffer, self->block_size);
				sodr_time_stop(self->drive);

				if (nb_write == self->block_size)
					break;

			default:
				so_log_write(so_log_level_error,
					dgettext("storiqone-drive-tape", "[%s | %s | #%u]: error while writing because %m"),
					self->drive->vendor, self->drive->model, self->drive->index);

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
		sodr_time_start();
		nb_write = write(self->fd, c_buffer + nb_total_write, self->block_size);
		sodr_time_stop(self->drive);

		if (nb_write < 0) {
			switch (errno) {
				case ENOSPC:
					if (!self->end_of_tape_warning) {
						self->end_of_tape_warning = true;

						int fd = sodr_tape_drive_open_scsi();
						sodr_tape_drive_scsi_size_available(fd, self->media);
						close(fd);

						char str_remaining_space[16];
						ssize_t remaining_space = self->media->block_size * self->media->free_block;
						so_file_convert_size_to_string(remaining_space, str_remaining_space, 16);

						so_log_write(so_log_level_info,
							dgettext("storiqone-drive-tape", "[%s | %s | #%u]: Space remaing on tape '%s': %s"),
							self->drive->vendor, self->drive->model, self->drive->index, self->media->label, str_remaining_space);
					}

					sodr_time_start();
					nb_write = write(self->fd, self->buffer, self->block_size);
					sodr_time_stop(self->drive);

					if (nb_write == self->block_size)
						break;

				default:
					so_log_write(so_log_level_error,
						dgettext("storiqone-drive-tape", "[%s | %s | #%u]: error while writing because %m"),
						self->drive->vendor, self->drive->model, self->drive->index);

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

