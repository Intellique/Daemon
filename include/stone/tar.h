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
*  Last modified: Sun, 08 Jan 2012 12:16:47 +0100                         *
\*************************************************************************/

#ifndef __STONE_TAR_H__
#define __STONE_TAR_H__

// dev_t, mode_t, ssize_t, time_t
#include <sys/types.h>

struct st_stream_reader;
struct st_stream_writer;

/**
 * \brief Define a generic tar header
 */
struct st_tar_header {
	/**
	 * \brief Id of device
	 *
	 * Used only by character or block device
	 *
	 * \code
	 * int major = dev >> 8;
	 * int minor = dev & 0xFF;
	 * \endcode
	 */
	dev_t dev;
	/**
	 * \brief A partial filename as recorded into tar
	 */
	char path[256];
	char * filename;
	/**
	 * \brief Value of hard or symbolic link
	 */
	char link[256];
	/**
	 * \brief Size of file
	 */
	ssize_t size;
	/**
	 * \brief List of permission
	 */
	mode_t mode;
	/**
	 * \brief Modified time
	 */
	time_t mtime;
	/**
	 * \brief User's id
	 * \note Id of user where tar has been made.
	 */
	uid_t uid;
	/**
	 * \brief User's name
	 * \note Name of user where tar has been made.
	 */
	char uname[32];
	/**
	 * \brief Group's id
	 * \note Id of group where tar has been made.
	 */
	gid_t gid;
	/**
	 * \brief Group's name
	 * \note Name of group where tar has been made.
	 */
	char gname[32];
	char is_label;
};

enum st_tar_header_status {
	ST_TAR_HEADER_OK,
	ST_TAR_HEADER_NOT_FOUND,
	ST_TAR_HEADER_BAD_HEADER,
	ST_TAR_HEADER_BAD_CHECKSUM,
};

/**
 * \brief Used for reading tar
 */
struct st_tar_in {
	/**
	 * \brief This structure contains only functions pointers used as methods
	 */
	struct st_tar_in_ops {
		int (*close)(struct st_tar_in * f);
		/**
		 * \brief Release memory
		 * \param[in] f : a tar tar
		 */
		void (*free)(struct st_tar_in * f);
		ssize_t (*get_block_size)(struct st_tar_in * f);
		/**
		 * \brief Parse the next header
		 * \param[in] f : a tar tar
		 * \param[out] header : an already allocated header
		 */
		enum st_tar_header_status (*get_header)(struct st_tar_in * f, struct st_tar_header * header);
		/**
		 * \brief Retreive the latest error
		 * \param[in] f : a tar format
		 */
		int (*last_errno)(struct st_tar_in * f);
		ssize_t (*read)(struct st_tar_in * f, void * data, ssize_t length);
		int (*skip_file)(struct st_tar_in * f);
	} * ops;
	/**
	 * \brief Private data used by io module
	 */
	void * data;
};

struct st_tar_out {
	struct st_tar_out_ops {
		int (*add_file)(struct st_tar_out * f, const char * filename);
		int (*add_label)(struct st_tar_out * f, const char * label);
		int (*add_link)(struct st_tar_out * f, const char * src, const char * target, struct st_tar_header * header);
		int (*close)(struct st_tar_out * io);
		ssize_t (*get_block_size)(struct st_tar_out * f);
		int (*end_of_file)(struct st_tar_out * f);
		void (*free)(struct st_tar_out * f);
		int (*last_errno)(struct st_tar_out * f);
		ssize_t (*write)(struct st_tar_out * f, const void * data, ssize_t length);
	} * ops;
	void * data;
};


void st_tar_init_header(struct st_tar_header * h);

struct st_tar_in * st_tar_new_in(struct st_stream_reader * io);
struct st_tar_out * st_tar_new_out(struct st_stream_writer * io);

#endif

