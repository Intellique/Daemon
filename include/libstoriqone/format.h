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

#ifndef __LIBSTORIQONE_FORMAT_H__
#define __LIBSTORIQONE_FORMAT_H__

// bool
#include <stdbool.h>
// dev_t, gid_t, mode_t, ssize_t, time_t, uid_t
#include <sys/types.h>

struct so_value;

struct so_format_file {
	char * filename;
	char * link;

	ssize_t position;
	ssize_t size;

	dev_t dev;
	dev_t rdev;
	mode_t mode;
	uid_t uid;
	char * user;
	gid_t gid;
	char * group;

	time_t ctime;
	time_t mtime;

	bool is_label;
};

enum so_format_reader_header_status {
	so_format_reader_header_bad_header,
	so_format_reader_header_not_found,
	so_format_reader_header_ok,
};

enum so_format_writer_status {
	so_format_writer_end_of_volume,
	so_format_writer_error,
	so_format_writer_ok,
	so_format_writer_unsupported,
};

struct so_format_reader {
	struct so_format_reader_ops {
		int (*close)(struct so_format_reader * fr);
		bool (*end_of_file)(struct so_format_reader * fr);
		enum so_format_reader_header_status (*forward)(struct so_format_reader * fr, off_t offset);
		void (*free)(struct so_format_reader * fr);
		ssize_t (*get_block_size)(struct so_format_reader * fr);
		struct so_value * (*get_digests)(struct so_format_reader * fr);
		enum so_format_reader_header_status (*get_header)(struct so_format_reader * fr, struct so_format_file * file);
		int (*last_errno)(struct so_format_reader * fr);
		ssize_t (*position)(struct so_format_reader * fr);
		ssize_t (*read)(struct so_format_reader * fr, void * buffer, ssize_t length);
		ssize_t (*read_to_end_of_data)(struct so_format_reader * fr);
		enum so_format_reader_header_status (*skip_file)(struct so_format_reader * fr);
	} * ops;
	void * data;
};

struct so_format_writer {
	struct so_format_writer_ops {
		enum so_format_writer_status (*add_file)(struct so_format_writer * fw, struct so_format_file * file);
		enum so_format_writer_status (*add_label)(struct so_format_writer * fw, const char * label);
		int (*close)(struct so_format_writer * fw);
		ssize_t (*end_of_file)(struct so_format_writer * fw);
		void (*free)(struct so_format_writer * fw);
		ssize_t (*get_available_size)(struct so_format_writer * fw);
		ssize_t (*get_block_size)(struct so_format_writer * fw);
		struct so_value * (*get_digests)(struct so_format_writer * fw);
		int (*last_errno)(struct so_format_writer * fw);
		ssize_t (*position)(struct so_format_writer * fw);
		ssize_t (*write)(struct so_format_writer * fw, const void * buffer, ssize_t length);

		// enum so_format_writer_status (*add_file)(struct so_format_writer * fw, const char * file);
		// enum so_format_writer_status (*add_file_at)(struct so_format_writer * fw, int dir_fd, const char * file);
		// ssize_t (*compute_size_of_file)(struct so_format_writer * fw, const char * file, bool recursive);
	} * ops;
	void * data;
};

struct so_value * so_format_file_convert(struct so_format_file * file);
void so_format_file_free(struct so_format_file * file);
void so_format_file_init(struct so_format_file * file);
void so_format_file_sync(struct so_format_file * file, struct so_value * new_file);

#endif

