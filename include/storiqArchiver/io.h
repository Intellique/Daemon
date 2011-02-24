/***********************************************************************\
*       ______           ____     ___           __   _                  *
*      / __/ /____  ____/  _/__ _/ _ | ________/ /  (_)  _____ ____     *
*     _\ \/ __/ _ \/ __// // _ `/ __ |/ __/ __/ _ \/ / |/ / -_) __/     *
*    /___/\__/\___/_/ /___/\_, /_/ |_/_/  \__/_//_/_/|___/\__/_/        *
*                           /_/                                         *
*  -------------------------------------------------------------------  *
*  This file is a part of StorIqArchiver                                *
*                                                                       *
*  StorIqArchiver is free software; you can redistribute it and/or      *
*  modify it under the terms of the GNU General Public License          *
*  as published by the Free Software Foundation; either version 3       *
*  of the License, or (at your option) any later version.               *
*                                                                       *
*  This program is distributed in the hope that it will be useful,      *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of       *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
*  GNU General Public License for more details.                         *
*                                                                       *
*  You should have received a copy of the GNU General Public License    *
*  along with this program; if not, write to the Free Software          *
*  Foundation, Inc., 51 Franklin Street, Fifth Floor,                   *
*  Boston, MA  02110-1301, USA.                                         *
*                                                                       *
*  -------------------------------------------------------------------  *
*  Copyright (C) 2011, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Thu, 24 Feb 2011 21:23:46 +0100                       *
\***********************************************************************/

#ifndef __STORIQARCHIVER_IO_H__
#define __STORIQARCHIVER_IO_H__

struct hashtable;

struct stream_read_io {
	struct stream_read_io_ops {
		int (*close)(struct stream_read_io * io);
		void (*free)(struct stream_read_io * io);
		long long int (*position)(struct stream_read_io * io);
		int (*read)(struct stream_read_io * io, void * buffer, int length);
		int (*reopen)(struct stream_read_io * io);
	} * ops;
	void * data;
};

struct stream_write_io {
	struct stream_write_io_ops {
		int (*close)(struct stream_write_io * io);
		int (*flush)(struct stream_write_io * io);
		void (*free)(struct stream_write_io * io);
		long long int (*position)(struct stream_write_io * io);
		int (*write)(struct stream_write_io * io, const void * buffer, int length);
	} * ops;
	void * data;
};

struct stream_io_driver {
	char * name;
	struct stream_read_io * (*new_streamRead)(struct stream_read_io * self, struct stream_read_io * to);
	struct stream_write_io * (*new_streamWrite)(struct stream_write_io * self, struct stream_write_io * io);
	void * cookie;
};

struct hashtable * io_checksum_read_completeDigest(struct stream_read_io * io);
struct hashtable * io_checksum_read_partialDigest(struct stream_read_io * io);
struct stream_read_io * io_checksum_read_new(struct stream_read_io * io, struct stream_read_io * to, char ** checksums, unsigned int nbChecksums);
struct stream_read_io * io_file_read_file(struct stream_read_io * io, const char * filename);
struct stream_read_io * io_file_read_file2(struct stream_read_io * io, const char * filename, int blockSize);
struct stream_write_io * io_file_write_fd(struct stream_write_io * io, int fd);
struct stream_write_io * io_file_write_fd2(struct stream_write_io * io, int fd, int blockSize);
struct stream_write_io * io_file_write_file(struct stream_write_io * io, const char * filename, int option);
struct stream_write_io * io_file_write_file2(struct stream_write_io * io, const char * filename, int option, int blockSize);

struct stream_io_driver * io_getDriver(const char * driver);
int io_loadDriver(const char * driver);
void io_registerDriver(struct stream_io_driver * driver);

#endif

