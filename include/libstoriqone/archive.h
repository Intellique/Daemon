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

#ifndef __LIBSTORIQONE_ARCHIVE_H__
#define __LIBSTORIQONE_ARCHIVE_H__

// bool
#include <stdbool.h>
// time_t
#include <sys/time.h>
// ssize_t
#include <sys/types.h>

struct so_archive_file;
enum st_archive_file_type;
struct so_archive_volume;
struct so_media;
struct so_value;

enum so_archive_file_type {
	so_archive_file_type_block_device,
	so_archive_file_type_character_device,
	so_archive_file_type_directory,
	so_archive_file_type_fifo,
	so_archive_file_type_regular_file,
	so_archive_file_type_socket,
	so_archive_file_type_symbolic_link,

	so_archive_file_type_unknown,
};

struct so_archive {
	char uuid[37];
	char * name;

	ssize_t size;

	time_t start_time;
	time_t end_time;

	struct so_archive_volume * volumes;
	unsigned int nb_volumes;

	char * creator;
	char * owner;

	struct so_value * db_data;
};

struct so_archive_volume {
	unsigned int sequence;
	ssize_t size;

	time_t start_time;
	time_t end_time;

	bool check_ok;
	time_t check_time;

	struct so_archive * archive;
	struct so_media * media;
	unsigned int media_position;
	struct so_job * job;

	struct so_value * digests;

	struct so_archive_files {
		struct so_archive_file * file;
		ssize_t position;
		time_t archived_time;
	} * files;
	unsigned int nb_files;

	struct so_value * db_data;
};

struct so_archive_file {
	char * name;
	mode_t perm;
	enum so_archive_file_type type;
	uid_t ownerid;
	char * owner;
	gid_t groupid;
	char * group;

	time_t create_time;
	time_t modify_time;

	bool check_ok;
	time_t check_time;

	ssize_t size;

	char * mime_type;
	char * selected_path;

	struct so_value * digests;

	struct so_value * db_data;
};

struct so_archive_volume * so_archive_add_volume(struct so_archive * archive);
struct so_value * so_archive_convert(struct so_archive * archive);
void so_archive_free(struct so_archive * archive);
struct so_archive * so_archive_new(void);

enum so_archive_file_type so_archive_file_mode_to_type(mode_t mode);
enum so_archive_file_type so_archive_file_string_to_type(const char * type, bool translate);
const char * so_archive_file_type_to_string(enum so_archive_file_type type, bool translate);

#endif

