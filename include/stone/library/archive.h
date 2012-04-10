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
*  Last modified: Fri, 06 Apr 2012 17:54:26 +0200                         *
\*************************************************************************/

#ifndef __STONE_ARCHIVE_H__
#define __STONE_ARCHIVE_H__

// time_t
#include <sys/time.h>
// ssize_t
#include <sys/types.h>

struct st_archive_volume;
struct st_drive;
struct st_job;
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
	long id;
	char * name;
	time_t ctime;
	time_t endtime;
	struct st_user * user;

	struct st_archive_volume * volumes;
	unsigned int nb_volumes;

	struct st_job * job;
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

	char ** digests;
	unsigned int nb_checksums;
};

struct st_archive_file {
	long id;
	char * name;
	mode_t perm;
	enum st_archive_file_type type;
	uid_t ownerid;
	char owner[32];
	gid_t groupid;
	char group[32];
	time_t ctime;
	time_t mtime;
	ssize_t size;

	char ** digests;
	unsigned int nb_checksums;

	struct st_archive * archive;
};

void st_archive_file_free(struct st_archive_file * file);
struct st_archive_file * st_archive_file_new(struct st_job * job, struct stat * file, const char * filename);
void st_archive_free(struct st_archive * archive);
struct st_archive * st_archive_new(struct st_job * job);
void st_archive_volume_free(struct st_archive_volume * volume);
struct st_archive_volume * st_archive_volume_new(struct st_job * job, struct st_drive * drive);
enum st_archive_file_type st_archive_file_string_to_type(const char * type);
const char * st_archive_file_type_to_string(enum st_archive_file_type type);

#endif

