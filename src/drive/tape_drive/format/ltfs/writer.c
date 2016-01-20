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

#include <libstoriqone/format.h>

struct sodr_tape_drive_format_ltfs_writer_private {
	int fd;
	int scsi_fd;

	char * buffer;
	size_t block_size;
	size_t buffer_size;
	size_t buffer_used;

	off_t file_position;
	ssize_t total_write;

	struct sodr_tape_drive_format_ltfs_file * file_info;
	unsigned int i_files;
	struct sodr_tape_drive_format_ltfs_extent * extent;
	unsigned int i_extent;
	int last_errno;

	struct so_drive * drive;
	struct so_media * media;
	struct sodr_tape_drive_format_ltfs * ltfs_info;
};

static enum so_format_writer_status sodr_tape_drive_format_ltfs_writer_add_file(struct so_format_writer * fw, const struct so_format_file * file);
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


static enum so_format_writer_status sodr_tape_drive_format_ltfs_writer_add_file(struct so_format_writer * fw, const struct so_format_file * file) {
}

static enum so_format_writer_status sodr_tape_drive_format_ltfs_writer_add_label(struct so_format_writer * fw, const char * label) {
}

static int sodr_tape_drive_format_ltfs_writer_close(struct so_format_writer * fw) {
}

static ssize_t sodr_tape_drive_format_ltfs_writer_compute_size_of_file(struct so_format_writer * fw, const struct so_format_file * file) {
}

static ssize_t sodr_tape_drive_format_ltfs_writer_end_of_file(struct so_format_writer * fw) {
}

static void sodr_tape_drive_format_ltfs_writer_free(struct so_format_writer * fw) {
}

static ssize_t sodr_tape_drive_format_ltfs_writer_get_available_size(struct so_format_writer * fw) {
}

static ssize_t sodr_tape_drive_format_ltfs_writer_get_block_size(struct so_format_writer * fw) {
}

static struct so_value * sodr_tape_drive_format_ltfs_writer_get_digests(struct so_format_writer * fw) {
}

static int sodr_tape_drive_format_ltfs_writer_file_position(struct so_format_writer * fw) {
}

static int sodr_tape_drive_format_ltfs_writer_last_errno(struct so_format_writer * fw) {
}

static ssize_t sodr_tape_drive_format_ltfs_writer_position(struct so_format_writer * fw) {
}

static struct so_format_reader * sodr_tape_drive_format_ltfs_writer_reopen(struct so_format_writer * fw) {
}

static enum so_format_writer_status sodr_tape_drive_format_ltfs_writer_restart_file(struct so_format_writer * fw, const struct so_format_file * file) {
}

static ssize_t sodr_tape_drive_format_ltfs_writer_write(struct so_format_writer * fw, const void * buffer, ssize_t length) {
}

