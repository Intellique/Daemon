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
*  Copyright (C) 2012, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Mon, 23 Jan 2012 15:49:08 +0100                         *
\*************************************************************************/

#ifndef __STONE_IO_H__
#define __STONE_IO_H__

// ssize_t
#include <sys/types.h>

struct st_stream_reader {
	struct st_stream_reader_ops {
		int (*close)(struct st_stream_reader * io);
		int (*end_of_file)(struct st_stream_reader * io);
		off_t (*forward)(struct st_stream_reader * io, off_t offset);
		void (*free)(struct st_stream_reader * io);
		ssize_t (*get_block_size)(struct st_stream_reader * io);
		int (*last_errno)(struct st_stream_reader * f);
		ssize_t (*position)(struct st_stream_reader * io);
		ssize_t (*read)(struct st_stream_reader * io, void * buffer, ssize_t length);
	} * ops;
	void * data;
};

struct st_stream_writer {
	struct st_stream_writer_ops {
		int (*close)(struct st_stream_writer * io);
		void (*free)(struct st_stream_writer * io);
		ssize_t (*get_available_size)(struct st_stream_writer * io);
		ssize_t (*get_block_size)(struct st_stream_writer * io);
		int (*last_errno)(struct st_stream_writer * f);
		ssize_t (*position)(struct st_stream_writer * io);
		ssize_t (*write)(struct st_stream_writer * io, const void * buffer, ssize_t length);
	} * ops;
	void * data;
};

struct st_stream_writer * st_stream_get_tmp_writer(void);
ssize_t st_stream_writer_printf(struct st_stream_writer * writer, const char * format, ...) __attribute__ ((format (printf, 2, 3)));

#endif

