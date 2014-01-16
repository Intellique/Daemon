/****************************************************************************\
*                             __________                                     *
*                            / __/_  __/__  ___  ___                         *
*                           _\ \  / / / _ \/ _ \/ -_)                        *
*                          /___/ /_/  \___/_//_/\__/                         *
*                                                                            *
*  ------------------------------------------------------------------------  *
*  This file is a part of STone                                              *
*                                                                            *
*  STone is free software; you can redistribute it and/or modify             *
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
*  Copyright (C) 2014, Clercin guillaume <gclercin@intellique.com>           *
*  Last modified: Tue, 28 May 2013 18:24:13 +0200                            *
\****************************************************************************/

#ifndef __STONE_ARCHIVE_H__
#define __STONE_ARCHIVE_H__

// bool
#include <stdbool.h>
// time_t
#include <sys/time.h>
// ssize_t
#include <sys/types.h>

struct st_archive_file;
struct st_archive_volume;
struct st_hashtable;
struct st_job;
struct st_media;
struct st_job_selected_path;
struct st_user;
struct stat;

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
	char uuid[37];
	char * name;

	time_t start_time;
	time_t end_time;

	bool check_ok;
	time_t check_time;

	struct st_archive_volume * volumes;
	unsigned int nb_volumes;

	struct st_archive * copy_of;
	struct st_user * user;

	char * metadatas;

	void * db_data;
};

struct st_archive_volume {
	long sequence;
	ssize_t size;

	time_t start_time;
	time_t end_time;

	bool check_ok;
	time_t check_time;

	struct st_archive * archive;
	struct st_media * media;
	long media_position;

	struct st_hashtable * digests;

	struct st_archive_files {
		struct st_archive_file * file;
		ssize_t position;
	} * files;
	unsigned int nb_files;

	void * db_data;
};

struct st_archive_file {
	char * name;
	mode_t perm;
	enum st_archive_file_type type;
	uid_t ownerid;
	char owner[32];
	gid_t groupid;
	char group[32];

	time_t create_time;
	time_t modify_time;
	time_t archived_time;

	bool check_ok;
	time_t check_time;

	ssize_t size;

	char * mime_type;

	struct st_hashtable * digests;

	struct st_archive * archive;
	struct st_job_selected_path * selected_path;

	void * db_data;
};

struct st_archive_volume * st_archive_add_volume(struct st_archive * archive, struct st_media * media, long media_position);
void st_archive_free(struct st_archive * archive);
struct st_archive * st_archive_new(const char * name, struct st_user * user);
void st_archive_file_free(struct st_archive_file * file);
struct st_archive_file * st_archive_file_new(struct stat * file, const char * filename);
enum st_archive_file_type st_archive_file_string_to_type(const char * type);
const char * st_archive_file_type_to_string(enum st_archive_file_type type);
void st_archive_volume_free(struct st_archive_volume * volume);

#endif

