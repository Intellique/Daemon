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
*  Last modified: Thu, 07 Feb 2013 11:32:09 +0100                         *
\*************************************************************************/

#ifndef __STONE_IO_H__
#define __STONE_IO_H__

// bool
#include <stdbool.h>
// off_t, ssize_t
#include <sys/types.h>

/**
 * \struct st_stream_reader
 * \brief Generic structure for reading from
 */
struct st_stream_reader {
	/**
	 * \brief stream operation
	 */
	struct st_stream_reader_ops {
		/**
		 * \brief Close stream
		 *
		 * \param[in] io : a stream reader
		 * \return 0 if ok
		 */
		int (*close)(struct st_stream_reader * io);
		/**
		 * \brief Test if we are at the end of stream
		 *
		 * \param[in] io : a stream reader
		 * \return 0 if remain data
		 */
		bool (*end_of_file)(struct st_stream_reader * io);
		/**
		 * \brief Forward into the stream
		 *
		 * \param[in] io : a stream reader
		 * \param[in] offset : length of forward
		 * \returns new position into the stream or -1 if error
		 */
		off_t (*forward)(struct st_stream_reader * io, off_t offset);
		/**
		 * \brief Release all resource associated to \a io
		 *
		 * \param[in] io : a stream reader
		 */
		void (*free)(struct st_stream_reader * io);
		/**
		 * \brief Get block size of this <a>stream reader</a>
		 */
		ssize_t (*get_block_size)(struct st_stream_reader * io);
		/**
		 * \brief Get the latest errno
		 *
		 * \param[in] io : a stream reader
		 * \returns latest errno or 0
		 */
		int (*last_errno)(struct st_stream_reader * io);
		/**
		 * \brief Get current position into stream \a io
		 *
		 * \param[in] io : a stream reader
		 * \returns current position
		 */
		ssize_t (*position)(struct st_stream_reader * io);
		/**
		 * \brief Read from the stream \a io
		 *
		 * \param[in] io : a stream reader
		 * \param[out] buffer : write data read into it
		 * \param[in] length : length of \a buffer
		 * \returns length read or -1 if error
		 */
		ssize_t (*read)(struct st_stream_reader * io, void * buffer, ssize_t length);
	} * ops;
	/**
	 * \brief private data
	 */
	void * data;
};

struct st_stream_writer {
	struct st_stream_writer_ops {
		ssize_t (*before_close)(struct st_stream_writer * sfw, void * buffer, ssize_t length);
		int (*close)(struct st_stream_writer * sfw);
		void (*free)(struct st_stream_writer * sfw);
		ssize_t (*get_available_size)(struct st_stream_writer * sfw);
		ssize_t (*get_block_size)(struct st_stream_writer * sfw);
		int (*last_errno)(struct st_stream_writer * sfw);
		ssize_t (*position)(struct st_stream_writer * sfw);
		ssize_t (*write)(struct st_stream_writer * sfw, const void * buffer, ssize_t length);
	} * ops;
	void * data;
};

struct st_stream_reader * st_checksum_reader_new(struct st_stream_reader * stream, char ** checksums, unsigned int nb_checksums, bool thread_helper);
struct st_stream_writer * st_checksum_writer_new(struct st_stream_writer * stream, char ** checksums, unsigned int nb_checksums, bool thread_helper);
char ** st_checksum_reader_get_checksums(struct st_stream_reader * stream);
char ** st_checksum_writer_get_checksums(struct st_stream_writer * stream);

ssize_t st_stream_writer_printf(struct st_stream_writer * writer, const char * format, ...) __attribute__ ((format (printf, 2, 3)));

#endif

