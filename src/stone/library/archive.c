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
*  Last modified: Thu, 05 Jan 2012 16:21:58 +0100                         *
\*************************************************************************/

#define _GNU_SOURCE
// asprintf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// strcmp
#include <string.h>
// gettimeofday
#include <sys/time.h>
// localtime_r
#include <time.h>

#include <stone/job.h>
#include <stone/library/archive.h>

struct st_archive_file_type2 {
	char * name;
	enum st_archive_file_type type;
} st_archive_file_type[] = {
	{ "block device",     st_archive_file_type_block_device },
	{ "character device", st_archive_file_type_character_device },
	{ "directory",        st_archive_file_type_directory },
	{ "fifo",             st_archive_file_type_fifo },
	{ "regular file",     st_archive_file_type_regular_file },
	{ "socket",           st_archive_file_type_socket },
	{ "symbolic link",    st_archive_file_type_symbolic_link },
	{ "unknown",          st_archive_file_type_unknown },

	{ 0, st_archive_file_type_unknown },
};


struct st_archive * st_archive_new(struct st_job * job) {
	struct timeval current;
	gettimeofday(&current, 0);
	struct tm local_current;
	localtime_r(&current.tv_sec, &local_current);
	char buffer[32];
	strftime(buffer, 32, "%F_%T", &local_current);

	struct st_archive * archive = job->archive = malloc(sizeof(struct st_archive));
	archive->id = -1;
	asprintf(&archive->name, "%s_%s", job->name, buffer);
	archive->ctime = current.tv_sec;
	archive->endtime = 0;

	archive->volumes = 0;
	archive->nb_volumes = 0;

	return archive;
}

enum st_archive_file_type st_archive_file_string_to_type(const char * type) {
	struct st_archive_file_type2 * ptr;
	for (ptr = st_archive_file_type; ptr->name; ptr++)
		if (!strcmp(ptr->name, type))
			return ptr->type;
	return ptr->type;
}

const char * st_archive_file_type_to_string(enum st_archive_file_type type) {
	struct st_archive_file_type2 * ptr;
	for (ptr = st_archive_file_type; ptr->name; ptr++)
		if (ptr->type == type)
			return ptr->name;
	return "unknown";
}

