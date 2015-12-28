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

// free, malloc
#include <stdlib.h>
// memcpy, strdup
#include <string.h>
// bzero
#include <strings.h>
// read
#include <unistd.h>

#include <libstoriqone/config.h>
#include <libstoriqone/drive.h>
#include <libstoriqone/format.h>
#include <libstoriqone/media.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/value.h>
#include <libstoriqone-drive/time.h>

#include "ltfs.h"
#include "../../media.h"
#include "../../scsi.h"

struct sodr_tape_drive_format_ltfs_reader_private {
	int fd;
	int scsi_fd;

	char * buffer;
	size_t block_size;
	size_t buffer_size;
	size_t buffer_used;

	off_t file_position;
	ssize_t total_read;

	char * base_directory;

	struct sodr_tape_drive_format_ltfs_file * file_info;
	unsigned int i_files;
	struct sodr_tape_drive_format_ltfs_extent * extent;
	unsigned int i_extent;
	int last_errno;

	struct so_drive * drive;
	struct so_media * media;
	struct sodr_tape_drive_format_ltfs * ltfs_info;
};

static int sodr_tape_drive_format_ltfs_reader_close(struct so_format_reader * fr);
static bool sodr_tape_drive_format_ltfs_reader_end_of_file(struct so_format_reader * fr);
static enum so_format_reader_header_status sodr_tape_drive_format_ltfs_reader_forward(struct so_format_reader * fr, off_t offset);
static void sodr_tape_drive_format_ltfs_reader_free(struct so_format_reader * fr);
static ssize_t sodr_tape_drive_format_ltfs_reader_get_block_size(struct so_format_reader * fr);
static struct so_value * sodr_tape_drive_format_ltfs_reader_get_digests(struct so_format_reader * fr);
static enum so_format_reader_header_status sodr_tape_drive_format_ltfs_reader_get_header(struct so_format_reader * fr, struct so_format_file * file);
static char * sodr_tape_drive_format_ltfs_reader_get_root(struct so_format_reader * fr);
static int sodr_tape_drive_format_ltfs_reader_last_errno(struct so_format_reader * fr);
static ssize_t sodr_tape_drive_format_ltfs_reader_position(struct so_format_reader * fr);
static ssize_t sodr_tape_drive_format_ltfs_reader_read(struct so_format_reader * fr, void * buffer, ssize_t length);
static ssize_t sodr_tape_drive_format_ltfs_reader_read_to_end_of_data(struct so_format_reader * fr);
static int sodr_tape_drive_format_ltfs_reader_rewind(struct so_format_reader * fr);
static enum so_format_reader_header_status sodr_tape_drive_format_ltfs_reader_skip_file(struct so_format_reader * fr);

static struct so_format_reader_ops sodr_tape_drive_format_ltfs_reader_ops = {
	.close               = sodr_tape_drive_format_ltfs_reader_close,
	.end_of_file         = sodr_tape_drive_format_ltfs_reader_end_of_file,
	.forward             = sodr_tape_drive_format_ltfs_reader_forward,
	.free                = sodr_tape_drive_format_ltfs_reader_free,
	.get_block_size      = sodr_tape_drive_format_ltfs_reader_get_block_size,
	.get_digests         = sodr_tape_drive_format_ltfs_reader_get_digests,
	.get_header          = sodr_tape_drive_format_ltfs_reader_get_header,
	.get_root            = sodr_tape_drive_format_ltfs_reader_get_root,
	.last_errno          = sodr_tape_drive_format_ltfs_reader_last_errno,
	.position            = sodr_tape_drive_format_ltfs_reader_position,
	.read                = sodr_tape_drive_format_ltfs_reader_read,
	.read_to_end_of_data = sodr_tape_drive_format_ltfs_reader_read_to_end_of_data,
	.rewind              = sodr_tape_drive_format_ltfs_reader_rewind,
	.skip_file           = sodr_tape_drive_format_ltfs_reader_skip_file,
};


struct so_format_reader * sodr_tape_drive_format_ltfs_new_reader(struct so_drive * drive, int fd, int scsi_fd) {
	struct sodr_tape_drive_format_ltfs_reader_private * self = malloc(sizeof(struct sodr_tape_drive_format_ltfs_reader_private));
	bzero(self, sizeof(struct sodr_tape_drive_format_ltfs_reader_private));

	struct so_media * media = drive->slot->media;
	struct sodr_tape_drive_media * mp = media->private_data;

	self->fd = fd;
	self->scsi_fd = scsi_fd;
	self->buffer = malloc(media->block_size);
	self->block_size = media->block_size;
	self->buffer_size = 0;
	self->buffer_used = 0;

	self->total_read = 0;
	self->file_position = 0;

	self->file_info = NULL;
	self->i_files = 0;
	self->extent = NULL;
	self->i_extent = 0;
	self->last_errno = 0;

	struct so_value * config = so_config_get();
	so_value_unpack(config, "{s{s{ss}}}",
		"format",
			"ltfs",
				"base directory", &self->base_directory
	);

	self->drive = drive;
	self->media = media;
	self->ltfs_info = &mp->data.ltfs;

	struct so_format_reader * reader = malloc(sizeof(struct so_format_reader));
	reader->ops = &sodr_tape_drive_format_ltfs_reader_ops;
	reader->data = self;

	return reader;
}


static int sodr_tape_drive_format_ltfs_reader_close(struct so_format_reader * fr) {
	struct sodr_tape_drive_format_ltfs_reader_private * self = fr->data;

	if (self->fd > -1) {
		self->fd = -1;
		self->last_errno = 0;
	}

	return 0;
}

static bool sodr_tape_drive_format_ltfs_reader_end_of_file(struct so_format_reader * fr) {
	struct sodr_tape_drive_format_ltfs_reader_private * self = fr->data;
	return self->i_files >= self->ltfs_info->nb_files;
}

static enum so_format_reader_header_status sodr_tape_drive_format_ltfs_reader_forward(struct so_format_reader * fr, off_t offset) {
	struct sodr_tape_drive_format_ltfs_reader_private * self = fr->data;

	off_t nb_total_skip = 0;
	ssize_t available_size = self->buffer_size - self->buffer_used;
	if (available_size > 0) {
		nb_total_skip = available_size;
		if (nb_total_skip > offset)
			nb_total_skip = offset;

		self->buffer_used += nb_total_skip;

		if (nb_total_skip == offset)
			return so_format_reader_header_ok;
	}

	while (nb_total_skip < offset) {
		if (self->extent == NULL) {
			ssize_t will_skip = offset - nb_total_skip;
			if (self->file_info->file.size - self->file_position < will_skip)
				will_skip = self->file_info->file.size - self->file_position;

			self->file_position += will_skip;
			self->total_read += will_skip;
			nb_total_skip += will_skip;

			return so_format_reader_header_ok;
		}

		if (self->file_position < self->extent->file_offset) {
			ssize_t will_skip = offset - nb_total_skip;
			if (self->file_position + will_skip > self->extent->file_offset)
				will_skip = self->extent->file_offset - self->file_position;

			self->file_position += will_skip;
			self->total_read += will_skip;
			nb_total_skip += will_skip;

			if (nb_total_skip == offset)
				return so_format_reader_header_ok;
		}

		if (self->file_position + (offset - nb_total_skip) >= self->extent->file_offset + (off_t) self->extent->byte_count) {
			ssize_t will_skip = self->extent->file_offset + self->extent->byte_count;

			self->file_position += will_skip;
			self->total_read += will_skip;
			nb_total_skip += will_skip;

			self->i_extent++;

			if (self->i_extent < self->file_info->nb_extents)
				self->extent++;
			else
				self->extent = NULL;

			if (self->file_position == self->file_info->file.size)
				return so_format_reader_header_ok;

			continue;
		}

		size_t nb_blocks = 0;
		if (offset - nb_total_skip > (off_t) (self->block_size - self->extent->byte_offset)) {
			nb_blocks = 1;
			size_t will_skip = self->block_size - self->extent->byte_offset;

			self->file_position += will_skip;
			self->total_read += will_skip;
			nb_total_skip += will_skip;
		}

		if (nb_blocks > 0) {
			size_t will_skip = offset - nb_total_skip;
			will_skip = will_skip - will_skip % self->block_size;

			nb_blocks += will_skip / self->block_size;

			self->file_position += will_skip;
			self->total_read += will_skip;
			nb_total_skip += will_skip;
		}

		struct sodr_tape_drive_scsi_position position = {
			.partition = self->extent->partition,
			.block_position = self->extent->start_block + nb_blocks,
			.end_of_partition = false,
		};

		sodr_time_start();
		int failed = sodr_tape_drive_scsi_locate16(self->scsi_fd, &position);
		sodr_time_stop(self->drive);

		if (failed != 0)
			return so_format_reader_header_not_found;

		if ((self->extent->byte_offset > 0 && nb_blocks == 0) || offset > nb_total_skip) {
			sodr_time_start();
			ssize_t nb_read = read(self->fd, self->buffer, self->block_size);
			sodr_time_stop(self->drive);

			if (nb_read < 0)
				return so_format_reader_header_not_found;

			if (nb_read == 0)
				return so_format_reader_header_ok;

			self->buffer_used = self->extent->byte_offset;
			self->buffer_size = nb_read;

			size_t will_skip = offset - nb_total_skip;
			self->file_position += will_skip;
			self->total_read += will_skip;
			nb_total_skip += will_skip;

			return so_format_reader_header_ok;
		}
	}

	return so_format_reader_header_ok;
}

static void sodr_tape_drive_format_ltfs_reader_free(struct so_format_reader * fr) {
	struct sodr_tape_drive_format_ltfs_reader_private * self = fr->data;
	if (self != NULL) {
		self->fd = -1;
		free(self->buffer);
		free(self);
	}
	fr->data = NULL;
	fr->ops = NULL;
	free(fr);
}

static ssize_t sodr_tape_drive_format_ltfs_reader_get_block_size(struct so_format_reader * fr) {
	struct sodr_tape_drive_format_ltfs_reader_private * self = fr->data;
	return self->media->block_size;
}

static struct so_value * sodr_tape_drive_format_ltfs_reader_get_digests(struct so_format_reader * fr __attribute__((unused))) {
	return so_value_new_linked_list();
}

static enum so_format_reader_header_status sodr_tape_drive_format_ltfs_reader_get_header(struct so_format_reader * fr, struct so_format_file * file) {
	struct sodr_tape_drive_format_ltfs_reader_private * self = fr->data;

	if (self->i_files >= self->ltfs_info->nb_files)
		return so_format_reader_header_not_found;

	if (self->i_files == 0)
		self->file_info = self->ltfs_info->first_file;
	else
		self->file_info++;

	so_format_file_copy(file, &self->file_info->file);

	self->buffer_used = self->buffer_size = 0;
	self->file_position = 0;
	self->i_files++;
	self->extent = self->file_info->extents;
	self->i_extent = 0;

	return so_format_reader_header_ok;
}

static char * sodr_tape_drive_format_ltfs_reader_get_root(struct so_format_reader * fr) {
	struct sodr_tape_drive_format_ltfs_reader_private * self = fr->data;
	return strdup(self->base_directory);
}

static int sodr_tape_drive_format_ltfs_reader_last_errno(struct so_format_reader * fr) {
	struct sodr_tape_drive_format_ltfs_reader_private * self = fr->data;
	return self->last_errno;
}

static ssize_t sodr_tape_drive_format_ltfs_reader_position(struct so_format_reader * fr) {
	struct sodr_tape_drive_format_ltfs_reader_private * self = fr->data;
	return self->total_read;
}

static ssize_t sodr_tape_drive_format_ltfs_reader_read(struct so_format_reader * fr, void * buffer, ssize_t length) {
	struct sodr_tape_drive_format_ltfs_reader_private * self = fr->data;

	if (self->file_position >= self->file_info->file.size)
		return 0;

	ssize_t nb_total_read = 0;
	while (nb_total_read < length) {
		if (self->extent == NULL) {
			ssize_t will_blank = length - nb_total_read;
			if (self->file_info->file.size - self->file_position < will_blank)
				will_blank = self->file_info->file.size - self->file_position;

			bzero(buffer + nb_total_read, will_blank);

			self->file_position += will_blank;
			self->total_read += will_blank;
			nb_total_read += will_blank;

			return nb_total_read;
		}

		if (self->file_position < self->extent->file_offset) {
			ssize_t will_blank = length - nb_total_read;
			if (self->file_position + will_blank > self->extent->file_offset)
				will_blank = self->extent->file_offset - self->file_position;

			bzero(buffer + nb_total_read, will_blank);

			self->file_position += will_blank;
			self->total_read += will_blank;
			nb_total_read += will_blank;

			if (nb_total_read == length)
				return nb_total_read;
		}

		// Positionnement vers un nouveau extent
		if (self->file_position == self->extent->file_offset) {
			struct sodr_tape_drive_scsi_position position = {
				.partition = self->extent->partition,
				.block_position = self->extent->start_block,
				.end_of_partition = false,
			};

			sodr_time_start();
			int failed = sodr_tape_drive_scsi_locate16(self->scsi_fd, &position);
			sodr_time_stop(self->drive);

			if (failed != 0)
				return -1;

			if (self->extent->byte_offset > 0) {
				sodr_time_start();
				ssize_t nb_read = read(self->fd, self->buffer, self->block_size);
				sodr_time_stop(self->drive);

				if (nb_read < 0)
					return nb_read;

				if (nb_read == 0)
					return nb_total_read;

				self->buffer_used = self->extent->byte_offset;
				self->buffer_size = nb_read;
			}
		}

		ssize_t available_size = self->buffer_size - self->buffer_used;
		if (available_size > 0) {
			ssize_t will_copy = available_size;
			if (length - nb_total_read < available_size)
				will_copy = length - nb_total_read;

			memcpy(buffer + nb_total_read, self->buffer + self->buffer_used, will_copy);

			self->buffer_used += will_copy;
			self->total_read += will_copy;
			self->file_position += will_copy;
			nb_total_read += will_copy;

			if (nb_total_read == length || self->file_position >= self->file_info->file.size)
				return nb_total_read;
		}

		// changement d'extent
		if (self->file_position == (off_t) (self->extent->file_offset + self->extent->byte_count)) {
			self->i_extent++;

			if (self->i_extent < self->file_info->nb_extents) {
				self->extent++;

				struct sodr_tape_drive_scsi_position position = {
					.partition = self->extent->partition,
					.block_position = self->extent->start_block,
					.end_of_partition = false,
				};

				sodr_time_start();
				int failed = sodr_tape_drive_scsi_locate16(self->scsi_fd, &position);
				sodr_time_stop(self->drive);

				if (failed != 0)
					return -1;

				if (self->extent->byte_offset > 0) {
					sodr_time_start();
					ssize_t nb_read = read(self->fd, self->buffer, self->block_size);
					sodr_time_stop(self->drive);

					if (nb_read < 0)
						return nb_read;

					if (nb_read == 0)
						return nb_total_read;

					self->buffer_used = self->extent->byte_offset;
					self->buffer_size = nb_read;

					continue;
				}
			} else {
				self->extent = NULL;
				continue;
			}
		}

		while (length - nb_total_read > (ssize_t) self->block_size && (ssize_t) (self->file_position + self->block_size) < self->file_info->file.size) {
			sodr_time_start();
			ssize_t nb_read = read(self->fd, buffer + nb_total_read, self->block_size);
			sodr_time_stop(self->drive);

			if (nb_read < 0)
				return nb_read;

			if (nb_read == 0)
				return nb_total_read;

			self->total_read += nb_read;
			self->file_position += nb_read;
			nb_total_read += nb_read;

			if (self->file_position == (off_t) (self->extent->file_offset + self->extent->byte_count))
				break;
		}

		if (self->file_position == (off_t) (self->extent->file_offset + self->extent->byte_count))
			continue;

		sodr_time_start();
		ssize_t nb_read = read(self->fd, self->buffer, self->block_size);
		sodr_time_stop(self->drive);

		if (nb_read < 0)
			return nb_read;

		if (nb_read == 0)
			return nb_total_read;

		self->buffer_used = self->extent->byte_offset;
		self->buffer_size = nb_read;

		available_size = self->buffer_size - self->buffer_used;
		if (available_size > 0) {
			ssize_t will_copy = available_size;
			if (length - nb_total_read < available_size)
				will_copy = length - nb_total_read;

			memcpy(buffer + nb_total_read, self->buffer + self->buffer_used, will_copy);

			self->buffer_used += will_copy;
			self->total_read += will_copy;
			self->file_position += will_copy;
			nb_total_read += will_copy;

			if (nb_total_read == length || self->file_position >= self->file_info->file.size)
				return nb_total_read;
		}
	}

	return nb_total_read;
}

static ssize_t sodr_tape_drive_format_ltfs_reader_read_to_end_of_data(struct so_format_reader * fr) {
	struct sodr_tape_drive_format_ltfs_reader_private * self = fr->data;

	ssize_t nb_total_read = 0;
	while (self->i_files < self->ltfs_info->nb_files) {
		nb_total_read += self->file_info->file.size - self->file_position;

		self->file_position = 0;
		self->buffer_used = self->buffer_size;

		self->i_files++;
		if (self->i_files < self->ltfs_info->nb_files)
			self->file_info++;
	}

	return nb_total_read;
}

static int sodr_tape_drive_format_ltfs_reader_rewind(struct so_format_reader * fr) {
	struct sodr_tape_drive_format_ltfs_reader_private * self = fr->data;

	self->buffer_size = 0;
	self->buffer_used = 0;

	self->total_read = 0;
	self->file_position = 0;

	self->file_info = NULL;
	self->i_files = 0;
	self->extent = NULL;
	self->i_extent = 0;
	self->last_errno = 0;

	return 0;
}

static enum so_format_reader_header_status sodr_tape_drive_format_ltfs_reader_skip_file(struct so_format_reader * fr) {
	struct sodr_tape_drive_format_ltfs_reader_private * self = fr->data;

	self->file_position = self->file_info->file.size;
	self->buffer_used = self->buffer_size;

	return so_format_reader_header_ok;
}

