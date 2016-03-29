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
// bzero
#include <strings.h>

#include <libstoriqone/drive.h>
#include <libstoriqone/format.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/value.h>

#include "ltfs.h"
#include "../../media.h"
#include "../../io/io.h"
#include "../../util/scsi.h"

struct sodr_tape_drive_format_ltfs_writer_private {
	int fd;
	int scsi_fd;

	ssize_t total_write;

	struct sodr_tape_drive_format_ltfs_file * current_file;
	struct sodr_tape_drive_format_ltfs_extent * current_extent;

	int last_errno;

	struct so_drive * drive;
	struct so_media * media;
	struct sodr_tape_drive_format_ltfs * ltfs_info;
	struct so_stream_writer * writer;
};

static enum so_format_writer_status sodr_tape_drive_format_ltfs_writer_add_file(struct so_format_writer * fw, const struct so_format_file * file, const char * selected_path);
static enum so_format_writer_status sodr_tape_drive_format_ltfs_writer_add_label(struct so_format_writer * fw, const char * label);
static int sodr_tape_drive_format_ltfs_writer_close(struct so_format_writer * fw);
static ssize_t sodr_tape_drive_format_ltfs_writer_compute_size_of_file(struct so_format_writer * fw, const struct so_format_file * file);
static ssize_t sodr_tape_drive_format_ltfs_writer_end_of_file(struct so_format_writer * fw);
static void sodr_tape_drive_format_ltfs_writer_free(struct so_format_writer * fw);
static ssize_t sodr_tape_drive_format_ltfs_writer_get_available_size(struct so_format_writer * fw);
static ssize_t sodr_tape_drive_format_ltfs_writer_get_block_size(struct so_format_writer * fw);
static struct so_value * sodr_tape_drive_format_ltfs_writer_get_digests(struct so_format_writer * fw);
static int sodr_tape_drive_format_ltfs_writer_file_position(struct so_format_writer * fw);
static int sodr_tape_drive_format_ltfs_writer_last_errno(struct so_format_writer * fw);
static ssize_t sodr_tape_drive_format_ltfs_writer_position(struct so_format_writer * fw);
static struct so_format_reader * sodr_tape_drive_format_ltfs_writer_reopen(struct so_format_writer * fw);
static enum so_format_writer_status sodr_tape_drive_format_ltfs_writer_restart_file(struct so_format_writer * fw, const struct so_format_file * file);
static ssize_t sodr_tape_drive_format_ltfs_writer_write(struct so_format_writer * fw, const void * buffer, ssize_t length);

static struct so_format_writer_ops sodr_tape_drive_format_ltfs_writer_ops = {
	.add_file             = sodr_tape_drive_format_ltfs_writer_add_file,
	.add_label            = sodr_tape_drive_format_ltfs_writer_add_label,
	.close                = sodr_tape_drive_format_ltfs_writer_close,
	.compute_size_of_file = sodr_tape_drive_format_ltfs_writer_compute_size_of_file,
	.end_of_file          = sodr_tape_drive_format_ltfs_writer_end_of_file,
	.free                 = sodr_tape_drive_format_ltfs_writer_free,
	.get_available_size   = sodr_tape_drive_format_ltfs_writer_get_available_size,
	.get_block_size       = sodr_tape_drive_format_ltfs_writer_get_block_size,
	.get_digests          = sodr_tape_drive_format_ltfs_writer_get_digests,
	.file_position        = sodr_tape_drive_format_ltfs_writer_file_position,
	.last_errno           = sodr_tape_drive_format_ltfs_writer_last_errno,
	.position             = sodr_tape_drive_format_ltfs_writer_position,
	.reopen               = sodr_tape_drive_format_ltfs_writer_reopen,
	.restart_file         = sodr_tape_drive_format_ltfs_writer_restart_file,
	.write                = sodr_tape_drive_format_ltfs_writer_write,
};


struct so_format_writer * sodr_tape_drive_format_ltfs_new_writer(struct so_drive * drive, int fd, int scsi_fd) {
	struct sodr_tape_drive_format_ltfs_writer_private * self = malloc(sizeof(struct sodr_tape_drive_format_ltfs_writer_private));
	bzero(self, sizeof(struct sodr_tape_drive_format_ltfs_writer_private));

	struct so_media * media = drive->slot->media;
	struct sodr_tape_drive_media * mp = media->private_data;

	self->fd = fd;
	self->scsi_fd = scsi_fd;

	self->current_file = NULL;
	self->current_extent = NULL;

	self->drive = drive;
	self->media = media;
	self->ltfs_info = &mp->data.ltfs;

	struct so_format_writer * writer = malloc(sizeof(struct so_format_writer));
	writer->ops = &sodr_tape_drive_format_ltfs_writer_ops;
	writer->data = self;

	drive->status = so_drive_status_writing;

	return writer;
}


static enum so_format_writer_status sodr_tape_drive_format_ltfs_writer_add_file(struct so_format_writer * fw, const struct so_format_file * file, const char * selected_path) {
	struct sodr_tape_drive_format_ltfs_writer_private * self = fw->data;

	struct sodr_tape_drive_scsi_position position;
	int failed = sodr_tape_drive_scsi_read_position(self->scsi_fd, &position);
	if (failed != 0) {

		return so_format_writer_error;
	}

	struct sodr_tape_drive_format_ltfs_file * previous_file = self->current_file;

	self->current_file = malloc(sizeof(struct sodr_tape_drive_format_ltfs_file));
	bzero(self->current_file, sizeof(struct sodr_tape_drive_format_ltfs_file));

	so_format_file_copy(&self->current_file->file, file);

	self->current_extent = malloc(sizeof(struct sodr_tape_drive_format_ltfs_extent));
	bzero(self->current_extent, sizeof(struct sodr_tape_drive_format_ltfs_extent));

	self->current_file->extents = self->current_extent;
	self->current_file->nb_extents = 1;

	self->current_extent->partition = position.partition;
	self->current_extent->start_block = position.block_position;

//	if (previous_file != NULL)
//		previous_file->next = self->current_file;

	return so_format_writer_ok;
}

static enum so_format_writer_status sodr_tape_drive_format_ltfs_writer_add_label(struct so_format_writer * fw __attribute__((unused)), const char * label __attribute__((unused))) {
	return so_format_writer_ok;
}

static int sodr_tape_drive_format_ltfs_writer_close(struct so_format_writer * fw) {
	struct sodr_tape_drive_format_ltfs_writer_private * self = fw->data;

	// TODO: write index

	// TODO: write index and update medium auxiliary memory

	return 0;
}

static ssize_t sodr_tape_drive_format_ltfs_writer_compute_size_of_file(struct so_format_writer * fw __attribute__((unused)), const struct so_format_file * file) {
	return file->size;
}

static ssize_t sodr_tape_drive_format_ltfs_writer_end_of_file(struct so_format_writer * fw) {
	struct sodr_tape_drive_format_ltfs_writer_private * self = fw->data;
	return sodr_tape_drive_writer_close2(self->writer, false);
}

static void sodr_tape_drive_format_ltfs_writer_free(struct so_format_writer * fw) {
	struct sodr_tape_drive_format_ltfs_writer_private * self = fw->data;

	if (self->fd >= 0)
		sodr_tape_drive_format_ltfs_writer_close(fw);

	free(self);
	free(fw);
}

static ssize_t sodr_tape_drive_format_ltfs_writer_get_available_size(struct so_format_writer * fw) {
	struct sodr_tape_drive_format_ltfs_writer_private * self = fw->data;
	return self->writer->ops->get_available_size(self->writer);
}

static ssize_t sodr_tape_drive_format_ltfs_writer_get_block_size(struct so_format_writer * fw) {
	struct sodr_tape_drive_format_ltfs_writer_private * self = fw->data;
	return self->writer->ops->get_block_size(self->writer);
}

static struct so_value * sodr_tape_drive_format_ltfs_writer_get_digests(struct so_format_writer * fw __attribute__((unused))) {
	return so_value_new_linked_list();
}

static int sodr_tape_drive_format_ltfs_writer_file_position(struct so_format_writer * fw) {
	struct sodr_tape_drive_format_ltfs_writer_private * self = fw->data;
	return self->writer->ops->file_position(self->writer);
}

static int sodr_tape_drive_format_ltfs_writer_last_errno(struct so_format_writer * fw) {
	struct sodr_tape_drive_format_ltfs_writer_private * self = fw->data;
	return self->writer->ops->last_errno(self->writer);
}

static ssize_t sodr_tape_drive_format_ltfs_writer_position(struct so_format_writer * fw) {
	struct sodr_tape_drive_format_ltfs_writer_private * self = fw->data;
	return self->writer->ops->position(self->writer);
}

static struct so_format_reader * sodr_tape_drive_format_ltfs_writer_reopen(struct so_format_writer * fw) {
}

static enum so_format_writer_status sodr_tape_drive_format_ltfs_writer_restart_file(struct so_format_writer * fw, const struct so_format_file * file) {
}

static ssize_t sodr_tape_drive_format_ltfs_writer_write(struct so_format_writer * fw, const void * buffer, ssize_t length) {
}

