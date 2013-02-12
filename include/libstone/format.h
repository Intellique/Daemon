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
*  Copyright (C) 2013, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Mon, 11 Feb 2013 16:38:31 +0100                         *
\*************************************************************************/

#ifndef __STONE_FORMAT_H__
#define __STONE_FORMAT_H__

// bool
#include <stdbool.h>
// dev_t, gid_t, mode_t, ssize_t, time_t, uid_t
#include <sys/types.h>

struct st_stream_reader;
struct st_stream_writer;

struct st_format_file {
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

enum st_format_reader_header_status {
	st_format_reader_header_bad_header,
	st_format_reader_header_not_found,
	st_format_reader_header_ok,
};

enum st_format_writer_status {
	st_format_writer_end_of_volume,
	st_format_writer_error,
	st_format_writer_ok,
	st_format_writer_unsupported,
};

struct st_format_reader {
	struct st_format_reader_ops {
		int (*close)(struct st_format_reader * fr);
		bool (*end_of_file)(struct st_format_reader * fr);
		enum st_format_reader_header_status (*forward)(struct st_format_reader * fr, ssize_t block_position);
		void (*free)(struct st_format_reader * fr);
		ssize_t (*get_block_size)(struct st_format_reader * fr);
		enum st_format_reader_header_status (*get_header)(struct st_format_reader * fr, struct st_format_file * file);
		int (*last_errno)(struct st_format_reader * fr);
		ssize_t (*position)(struct st_format_reader * fr);
		ssize_t (*read)(struct st_format_reader * fr, void * buffer, ssize_t length);
		ssize_t (*read_to_end_of_data)(struct st_format_reader * fr);
		enum st_format_reader_header_status (*skip_file)(struct st_format_reader * fr);
	} * ops;
	void * data;
};

struct st_format_writer {
	struct st_format_writer_ops {
		enum st_format_writer_status (*add)(struct st_format_writer * sf, struct st_format_file * file);
		enum st_format_writer_status (*add_file)(struct st_format_writer * sf, const char * file);
		enum st_format_writer_status (*add_file_at)(struct st_format_writer * sf, int dir_fd, const char * file);
		enum st_format_writer_status (*add_label)(struct st_format_writer * sf, const char * label);
		int (*close)(struct st_format_writer * sf);
		ssize_t (*compute_size_of_file)(struct st_format_writer * sf, const char * file, bool recursive);
		ssize_t (*end_of_file)(struct st_format_writer * sf);
		void (*free)(struct st_format_writer * sf);
		ssize_t (*get_available_size)(struct st_format_writer * sf);
		ssize_t (*get_block_size)(struct st_format_writer * sf);
		int (*last_errno)(struct st_format_writer * sf);
		void (*new_volume)(struct st_format_writer * sf, struct st_stream_writer * sfw);
		ssize_t (*position)(struct st_format_writer * sf);
		int (*restart_file)(struct st_format_writer * sf, const char * filename, ssize_t position);
		ssize_t (*write)(struct st_format_writer * sf, const void * buffer, ssize_t length);
	} * ops;
	void * data;
};

void st_format_file_free(struct st_format_file * file);

struct st_format_reader * st_tar_get_reader(struct st_stream_reader * reader);
struct st_format_writer * st_tar_get_writer(struct st_stream_writer * writer);

#endif

