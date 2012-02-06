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
*  Last modified: Mon, 09 Jan 2012 20:47:00 +0100                         *
\*************************************************************************/

#define _GNU_SOURCE
// asprintf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// strcmp
#include <string.h>
// struct stat
#include <sys/stat.h>
// gettimeofday
#include <sys/time.h>
// localtime_r
#include <time.h>

#include <stone/job.h>
#include <stone/library/archive.h>
#include <stone/library/changer.h>
#include <stone/library/drive.h>
#include <stone/util.h>

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
	archive->user = job->user;

	archive->volumes = 0;
	archive->nb_volumes = 0;

	archive->job = job;

	return archive;
}

void st_archive_file_free(struct st_archive_file * file) {
	if (!file)
		return;

	free(file->name);
	file->digests = 0;
	file->nb_checksums = 0;
	file->archive = 0;

	free(file);
}

struct st_archive_file * st_archive_file_new(struct st_job * job, struct stat * file, const char * filename) {
	struct st_archive_file * f = malloc(sizeof(struct st_archive_file));
	f->id = -1;
	f->name = strdup(filename);
	f->perm = file->st_mode & 0777;

	if (S_ISDIR(file->st_mode))
		f->type = st_archive_file_type_directory;
	else if (S_ISREG(file->st_mode))
		f->type = st_archive_file_type_regular_file;
	else if (S_ISLNK(file->st_mode))
		f->type = st_archive_file_type_symbolic_link;
	else if (S_ISCHR(file->st_mode))
		f->type = st_archive_file_type_character_device;
	else if (S_ISBLK(file->st_mode))
		f->type = st_archive_file_type_block_device;
	else if (S_ISFIFO(file->st_mode))
		f->type = st_archive_file_type_fifo;
	else if (S_ISSOCK(file->st_mode))
		f->type = st_archive_file_type_socket;

	f->ownerid = file->st_uid;
	st_util_uid2name(f->owner, 32, file->st_uid);
	f->groupid = file->st_gid;
	st_util_gid2name(f->group, 32, file->st_gid);
	f->ctime = file->st_ctime;
	f->mtime = file->st_mtime;
	f->size = file->st_size;

	f->digests = 0;
	f->nb_checksums = 0;

	f->archive = job->archive;

	return f;
}

struct st_archive_volume * st_archive_volume_new(struct st_job * job, struct st_drive * drive) {
	struct st_archive * archive = job->archive;
	archive->volumes = realloc(archive->volumes, (archive->nb_volumes + 1) * sizeof(struct st_archive_volume));
	unsigned int index_volume = archive->nb_volumes;
	archive->nb_volumes++;

	struct st_archive_volume * volume = archive->volumes + index_volume;
	volume->id = -1;
	volume->sequence = index_volume;
	volume->size = 0;
	volume->ctime = time(0);
	volume->endtime = 0;

	volume->archive = archive;
	volume->tape = drive->slot->tape;
	volume->tape_position = drive->file_position;

	return volume;
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

