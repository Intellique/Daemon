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
*  Last modified: Wed, 04 Jan 2012 15:40:43 +0100                         *
\*************************************************************************/

#ifndef __STONE_ARCHIVE_H__
#define __STONE_ARCHIVE_H__

// time_t
#include <sys/time.h>
// ssize_t
#include <sys/types.h>

struct st_archive_volume;

enum st_archive_file_type {
	st_archive_file_type_block_device,
	st_archive_file_type_character_device,
	st_archive_file_type_directory,
	st_archive_file_type_fifo,
	st_archive_file_type_regular_file,
	st_archive_file_type_socket,
	st_archive_file_type_symbolic_link,

	st_archive_file_type_unknown,
};

struct st_archive {
	long id;
	char * name;
	time_t ctime;
	time_t endtime;

	struct st_archive_volume * volumes;
	unsigned int nb_volumes;
};

struct st_archive_volume {
	long id;
	long sequence;
	ssize_t size;
	time_t ctime;
	time_t endtime;

	struct st_archive * archive;
	struct st_tape * tape;
	long tape_position;
};

struct st_archive_file {
	long id;
	char * name;
};

enum st_archive_file_type st_archive_file_string_to_type(const char * type);
const char * st_archive_file_type_to_string(enum st_archive_file_type type);

#endif

