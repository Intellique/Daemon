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

// dirname
#include <libgen.h>
// free, malloc
#include <stdlib.h>
// memcpy, strdup
#include <string.h>
// bzero
#include <strings.h>
// close, read
#include <unistd.h>

#include <libstoriqone/config.h>
#include <libstoriqone/drive.h>
#include <libstoriqone/format.h>
#include <libstoriqone/media.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/string.h>
#include <libstoriqone/value.h>
#include <libstoriqone-drive/time.h>

#include "ltfs.h"
#include "../../media.h"
#include "../../util/scsi.h"

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
	struct sodr_tape_drive_format_ltfs_extent * extents;
	unsigned int i_extent;
	int last_errno;

	struct sodr_tape_drive_format_ltfs_file * next_file;

	struct so_drive * drive;
	struct so_media * media;
	unsigned int volume_number;
	struct sodr_tape_drive_format_ltfs * ltfs_info;
};

static int sodr_tape_drive_format_ltfs_reader_close(struct so_format_reader * fr);
static bool sodr_tape_drive_format_ltfs_reader_end_of_file(struct so_format_reader * fr);
static struct sodr_tape_drive_format_ltfs_file * sodr_tape_drive_format_ltfs_reader_find_by_path(struct sodr_tape_drive_format_ltfs_reader_private * self, char * path, struct so_format_file * file);
static void sodr_tape_drive_format_ltfs_reader_find_next_file(struct sodr_tape_drive_format_ltfs_reader_private * self);
static enum so_format_reader_header_status sodr_tape_drive_format_ltfs_reader_forward(struct so_format_reader * fr, off_t offset);
static void sodr_tape_drive_format_ltfs_reader_free(struct so_format_reader * fr);
static ssize_t sodr_tape_drive_format_ltfs_reader_get_block_size(struct so_format_reader * fr);
static struct so_value * sodr_tape_drive_format_ltfs_reader_get_digests(struct so_format_reader * fr);
static enum so_format_reader_header_status sodr_tape_drive_format_ltfs_reader_get_header(struct so_format_reader * fr, struct so_format_file * file, const char * expected_path, const char * selected_path);
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


struct so_format_reader * sodr_tape_drive_format_ltfs_new_reader(struct so_drive * drive, int fd, int scsi_fd, unsigned int volume_number) {
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
	self->extents = NULL;
	self->i_extent = 0;
	self->last_errno = 0;

	self->next_file = mp->data.ltfs.root.first_child;
	struct so_value * config = so_config_get();
	so_value_unpack(config, "{s{s{ss}}}",
		"format",
			"ltfs",
				"base directory", &self->base_directory
	);

	self->drive = drive;
	self->media = media;
	self->volume_number = volume_number;
	self->ltfs_info = &mp->data.ltfs;

	if (self->next_file != NULL && (self->next_file->ignored || self->next_file->volume_number != self->volume_number))
		sodr_tape_drive_format_ltfs_reader_find_next_file(self);

	struct so_format_reader * reader = malloc(sizeof(struct so_format_reader));
	reader->ops = &sodr_tape_drive_format_ltfs_reader_ops;
	reader->data = self;

	drive->status = so_drive_status_reading;

	return reader;
}


static int sodr_tape_drive_format_ltfs_reader_close(struct so_format_reader * fr) {
	struct sodr_tape_drive_format_ltfs_reader_private * self = fr->data;

	if (self->fd > -1) {
		close(self->fd);
		close(self->scsi_fd);

		self->fd = -1;
		self->scsi_fd = -1;
		self->last_errno = 0;
	}

	self->drive->status = so_drive_status_loaded_idle;

	return 0;
}

static bool sodr_tape_drive_format_ltfs_reader_end_of_file(struct so_format_reader * fr) {
	struct sodr_tape_drive_format_ltfs_reader_private * self = fr->data;
	return self->file_info == &self->ltfs_info->root || self->file_info == NULL;
}

static struct sodr_tape_drive_format_ltfs_file * sodr_tape_drive_format_ltfs_reader_find_by_path(struct sodr_tape_drive_format_ltfs_reader_private * self, char * path, struct so_format_file * file) {
	struct sodr_tape_drive_format_ltfs_file * ptr = self->ltfs_info->root.first_child;
	while (ptr != NULL) {
		char * ptr_path_end = strchr(path, '/');
		if (ptr_path_end != NULL)
			*ptr_path_end = '\0';
		const unsigned long long hash_name = so_string_compute_hash2(path);
		if (ptr_path_end != NULL)
			*ptr_path_end = '/';

		if (ptr->hash_name == hash_name) {
			if (ptr_path_end == NULL) {
				so_format_file_copy(file, &ptr->file);
				self->file_info = ptr;
				return ptr;
			} else {
				ptr = ptr->first_child;
				path = ptr_path_end;

				while (*path == '/')
					path++;
			}
		} else
			ptr = ptr->next_sibling;
	}
	return ptr;
}

static void sodr_tape_drive_format_ltfs_reader_find_next_file(struct sodr_tape_drive_format_ltfs_reader_private * self) {
	if (self->next_file == &self->ltfs_info->root || self->next_file == NULL)
		return;

	do {
		if (self->next_file->first_child != NULL)
			self->next_file = self->next_file->first_child;
		else if (self->next_file->next_sibling != NULL)
			self->next_file = self->next_file->next_sibling;
		else {
			do {
				self->next_file = self->next_file->parent;
			} while (self->next_file->parent != NULL && self->next_file->next_sibling == NULL);

			if (self->next_file->parent == NULL)
				break;
			else if (self->next_file->next_sibling != NULL)
				self->next_file = self->next_file->next_sibling;
		}

	} while (self->next_file != NULL && (self->next_file->ignored || self->next_file->volume_number != self->volume_number));
}

static enum so_format_reader_header_status sodr_tape_drive_format_ltfs_reader_forward(struct so_format_reader * fr __attribute__((unused)), off_t offset __attribute__((unused))) {
	return so_format_reader_header_ok;
}

static void sodr_tape_drive_format_ltfs_reader_free(struct so_format_reader * fr) {
	struct sodr_tape_drive_format_ltfs_reader_private * self = fr->data;
	if (self != NULL) {
		if (self->fd > -1)
			sodr_tape_drive_format_ltfs_reader_close(fr);

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

static enum so_format_reader_header_status sodr_tape_drive_format_ltfs_reader_get_header(struct so_format_reader * fr, struct so_format_file * file, const char * expected_path, const char * selected_path) {
	struct sodr_tape_drive_format_ltfs_reader_private * self = fr->data;

	if (expected_path != NULL) {
		char * selected_path_dup = strdup(selected_path);
		dirname(selected_path_dup);
		ssize_t selected_path_length = strlen(selected_path_dup);
		free(selected_path_dup);

		char * path_dup = strdup(expected_path);
		char * ptr_path = path_dup + selected_path_length;
		while (*ptr_path == '/')
			ptr_path++;

		struct sodr_tape_drive_format_ltfs_file * ptr = sodr_tape_drive_format_ltfs_reader_find_by_path(self, ptr_path, file);

		if (ptr == NULL) {
			ptr_path = path_dup + strlen(selected_path);
			while (*ptr_path == '/')
				ptr_path++;

			ptr = sodr_tape_drive_format_ltfs_reader_find_by_path(self, ptr_path, file);
		}

		free(path_dup);

		if (ptr == NULL)
			return so_format_reader_header_not_found;
	} else {
		self->file_info = self->next_file;

		if (self->file_info == &self->ltfs_info->root || self->file_info == NULL)
			return so_format_reader_header_not_found;

		so_format_file_copy(file, &self->file_info->file);
		sodr_tape_drive_format_ltfs_reader_find_next_file(self);
	}

	self->buffer_used = self->buffer_size = 0;
	self->file_position = 0;
	self->extents = self->file_info->extents;
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
		if (self->extents == NULL) {
			ssize_t will_blank = length - nb_total_read;
			if (self->file_info->file.size - self->file_position < will_blank)
				will_blank = self->file_info->file.size - self->file_position;

			bzero(buffer + nb_total_read, will_blank);

			self->file_position += will_blank;
			self->total_read += will_blank;
			nb_total_read += will_blank;

			return nb_total_read;
		}

		if (self->file_position < self->extents->file_offset) {
			ssize_t will_blank = length - nb_total_read;
			if (self->file_position + will_blank > self->extents->file_offset)
				will_blank = self->extents->file_offset - self->file_position;

			bzero(buffer + nb_total_read, will_blank);

			self->file_position += will_blank;
			self->total_read += will_blank;
			nb_total_read += will_blank;

			if (nb_total_read == length)
				return nb_total_read;
		}

		// Positionnement vers un nouveau extent
		if (self->file_position == self->extents->file_offset) {
			struct sodr_tape_drive_scsi_position position = {
				.partition = self->extents->partition,
				.block_position = self->extents->start_block,
				.end_of_partition = false,
			};

			self->drive->status = so_drive_status_positioning;

			sodr_time_start();
			int failed = sodr_tape_drive_scsi_locate(self->scsi_fd, &position, self->media->media_format);
			sodr_time_stop(self->drive);

			self->drive->status = so_drive_status_reading;

			if (failed != 0)
				return -1;

			if (self->extents->byte_offset > 0) {
				sodr_time_start();
				ssize_t nb_read = read(self->fd, self->buffer, self->block_size);
				sodr_time_stop(self->drive);

				if (nb_read < 0)
					return nb_read;

				if (nb_read == 0)
					return nb_total_read;

				self->buffer_used = self->extents->byte_offset;
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
		if (self->file_position == (off_t) (self->extents->file_offset + self->extents->byte_count)) {
			self->i_extent++;

			if (self->i_extent < self->file_info->nb_extents) {
				self->extents++;

				struct sodr_tape_drive_scsi_position position = {
					.partition = self->extents->partition,
					.block_position = self->extents->start_block,
					.end_of_partition = false,
				};

				self->drive->status = so_drive_status_positioning;

				sodr_time_start();
				int failed = sodr_tape_drive_scsi_locate(self->scsi_fd, &position, self->media->media_format);
				sodr_time_stop(self->drive);

				self->drive->status = so_drive_status_reading;

				if (failed != 0)
					return -1;

				if (self->extents->byte_offset > 0) {
					sodr_time_start();
					ssize_t nb_read = read(self->fd, self->buffer, self->block_size);
					sodr_time_stop(self->drive);

					if (nb_read < 0)
						return nb_read;

					if (nb_read == 0)
						return nb_total_read;

					self->buffer_used = self->extents->byte_offset;
					self->buffer_size = nb_read;

					continue;
				}
			} else {
				self->extents = NULL;
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

			if (self->file_position == (off_t) (self->extents->file_offset + self->extents->byte_count))
				break;
		}

		if (self->file_position == (off_t) (self->extents->file_offset + self->extents->byte_count))
			continue;

		sodr_time_start();
		ssize_t nb_read = read(self->fd, self->buffer, self->block_size);
		sodr_time_stop(self->drive);

		if (nb_read < 0)
			return nb_read;

		if (nb_read == 0)
			return nb_total_read;

		self->buffer_used = self->extents->byte_offset;
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

static ssize_t sodr_tape_drive_format_ltfs_reader_read_to_end_of_data(struct so_format_reader * fr __attribute__((unused))) {
	return 0;
}

static int sodr_tape_drive_format_ltfs_reader_rewind(struct so_format_reader * fr) {
	struct sodr_tape_drive_format_ltfs_reader_private * self = fr->data;

	self->buffer_size = 0;
	self->buffer_used = 0;

	self->total_read = 0;
	self->file_position = 0;

	self->file_info = NULL;
	self->extents = NULL;
	self->i_extent = 0;
	self->last_errno = 0;

	self->next_file = self->ltfs_info->root.first_child;
	if (self->next_file != NULL && (self->next_file->ignored || self->next_file->volume_number != self->volume_number))
		sodr_tape_drive_format_ltfs_reader_find_next_file(self);

	return 0;
}

static enum so_format_reader_header_status sodr_tape_drive_format_ltfs_reader_skip_file(struct so_format_reader * fr) {
	struct sodr_tape_drive_format_ltfs_reader_private * self = fr->data;

	self->file_position = self->file_info->file.size;
	self->buffer_used = self->buffer_size;

	return so_format_reader_header_ok;
}

