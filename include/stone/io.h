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
*  Copyright (C) 2011, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Sat, 17 Dec 2011 17:39:08 +0100                         *
\*************************************************************************/

#ifndef __STORIQARCHIVER_IO_H__
#define __STORIQARCHIVER_IO_H__

// ssize_t
#include <sys/types.h>

struct sa_stream_reader {
	struct sa_stream_reader_ops {
		int (*close)(struct sa_stream_reader * io);
		void (*free)(struct sa_stream_reader * io);
		ssize_t (*get_block_size)(struct sa_stream_reader * io);
		ssize_t (*position)(struct sa_stream_reader * io);
		ssize_t (*read)(struct sa_stream_reader * io, void * buffer, ssize_t length);
	} * ops;
	void * data;
};

struct sa_stream_writer {
	struct sa_stream_writer_ops {
		int (*close)(struct sa_stream_writer * io);
		void (*free)(struct sa_stream_writer * io);
		ssize_t (*get_block_size)(struct sa_stream_writer * io);
		ssize_t (*position)(struct sa_stream_writer * io);
		ssize_t (*write)(struct sa_stream_writer * io, void * buffer, ssize_t length);
	} * ops;
	void * data;
};

#endif

