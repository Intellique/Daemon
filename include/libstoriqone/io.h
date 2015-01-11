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
*  Copyright (C) 2015, Guillaume Clercin <gclercin@intellique.com>           *
\****************************************************************************/

#ifndef __LIBSTORIQONE_IO_H__
#define __LIBSTORIQONE_IO_H__

// bool
#include <stdbool.h>
// ssize_t
#include <sys/types.h>

struct so_stream_reader {
	/**
	 * \brief stream operation
	 */
	struct so_stream_reader_ops {
		/**
		 * \brief Close stream
		 *
		 * \param[in] sr : a stream reader
		 * \return 0 if ok
		 */
		int (*close)(struct so_stream_reader * sr);
		/**
		 * \brief Test if we are at the end of stream
		 *
		 * \param[in] sr : a stream reader
		 * \return 0 if remain data
		 */
		bool (*end_of_file)(struct so_stream_reader * sr);
		/**
		 * \brief Forward into the stream
		 *
		 * \param[in] sr : a stream reader
		 * \param[in] offset : length of forward
		 * \returns new position into the stream or -1 if error
		 */
		off_t (*forward)(struct so_stream_reader * sr, off_t offset);
		/**
		 * \brief Release all resource associated to \a sr
		 *
		 * \param[in] sr : a stream reader
		 */
		void (*free)(struct so_stream_reader * sr);
		/**
		 * \brief Get block size of this <a>stream reader</a>
		 */
		ssize_t (*get_block_size)(struct so_stream_reader * sr);
		/**
		 * \brief Get the latest errno
		 *
		 * \param[in] sr : a stream reader
		 * \returns latest errno or 0
		 */
		int (*last_errno)(struct so_stream_reader * sr);
		/**
		 * \brief Get current position into stream \a sr
		 *
		 * \param[in] sr : a stream reader
		 * \returns current position
		 */
		ssize_t (*position)(struct so_stream_reader * sr);
		/**
		 * \brief Read from the stream \a sr
		 *
		 * \param[in] sr : a stream reader
		 * \param[out] buffer : write data read into it
		 * \param[in] length : length of \a buffer
		 * \returns length read or -1 if error
		 */
		ssize_t (*read)(struct so_stream_reader * sr, void * buffer, ssize_t length);
	} * ops;
	/**
	 * \brief private data
	 */
	void * data;
};

struct so_stream_writer {
	struct so_stream_writer_ops {
		ssize_t (*before_close)(struct so_stream_writer * sw, void * buffer, ssize_t length);
		int (*close)(struct so_stream_writer * sw);
		int (*file_position)(struct so_stream_writer * sw);
		void (*free)(struct so_stream_writer * sw);
		ssize_t (*get_available_size)(struct so_stream_writer * sw);
		ssize_t (*get_block_size)(struct so_stream_writer * sw);
		int (*last_errno)(struct so_stream_writer * sw);
		ssize_t (*position)(struct so_stream_writer * sw);
		struct so_stream_reader * (*reopen)(struct so_stream_writer * sw);
		ssize_t (*write)(struct so_stream_writer * sw, const void * buffer, ssize_t length);
	} * ops;
	void * data;
};

struct so_stream_writer * so_io_tmp_writer(void);

#endif

