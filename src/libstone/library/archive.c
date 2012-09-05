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
*  Last modified: Tue, 04 Sep 2012 13:01:42 +0200                         *
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
// time
#include <time.h>

#include <stone/job.h>
#include <stone/library/archive.h>
#include <stone/library/changer.h>
#include <stone/library/drive.h>
#include <stone/util.h>

static struct st_archive_file_type2 {
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


void st_archive_file_free(struct st_archive_file * file) {
	if (!file)
		return;

    free(file->name);
    file->name = 0;

	file->digests = 0;
	file->nb_checksums = 0;
	file->archive = 0;

	free(file);
}

struct st_archive_file * st_archive_file_new(struct st_job * job, struct stat * file, const char * filename, ssize_t block_number) {
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
	f->block_number = block_number;

	f->digests = 0;
	f->nb_checksums = 0;

	f->archive = job->archive;

	return f;
}

void st_archive_free(struct st_archive * archive) {
	if (!archive)
		return;

	if (archive->name)
		free(archive->name);
	archive->name = 0;
	archive->user = 0;

	archive->job = 0;
	archive->copy_of = 0;
}

struct st_archive * st_archive_new(struct st_job * job) {
	struct timeval current;
	gettimeofday(&current, 0);

	struct st_archive * archive = malloc(sizeof(struct st_archive));
	archive->id = -1;
	archive->name = strdup(job->name);
	archive->ctime = current.tv_sec;
	archive->endtime = 0;
	archive->user = job->user;
	archive->copy_of = job->archive;
	job->archive = archive;

	archive->volumes = 0;
	archive->nb_volumes = 0;
	archive->next_sequence = 0;

	archive->job = job;

	return archive;
}

void st_archive_volume_add_file(struct st_archive_volume * volume, struct st_archive_file * file, ssize_t position) {
	volume->files = realloc(volume->files, (volume->nb_files + 1) * sizeof(struct st_archive_files));
	struct st_archive_files * f = volume->files + volume->nb_files;
	volume->nb_files++;

	f->file = file;
	f->position = position;
}

void st_archive_volume_free(struct st_archive_volume * volume) {
	if (!volume)
		return;

	volume->archive = 0;
	volume->tape = 0;

	unsigned int i;
	for (i = 0; i < volume->nb_checksums; i++)
		free(volume->digests[i]);
	free(volume->digests[i]);
	volume->digests[i] = 0;

	for (i = 0; i < volume->nb_files; i++)
		st_archive_file_free(volume->files[i].file);
	if (volume->files)
		free(volume->files);
	volume->files = 0;
}

struct st_archive_volume * st_archive_volume_new(struct st_job * job, struct st_drive * drive) {
	struct st_archive * archive = job->archive;
	archive->volumes = realloc(archive->volumes, (archive->nb_volumes + 1) * sizeof(struct st_archive_volume));
	unsigned int index_volume = archive->nb_volumes;
	archive->nb_volumes++;

	struct st_archive_volume * volume = archive->volumes + index_volume;
	volume->id = -1;
	volume->sequence = index_volume + archive->next_sequence;
	volume->size = 0;
	volume->ctime = time(0);
	volume->endtime = 0;

	volume->archive = archive;
	volume->tape = drive->slot->tape;
	volume->tape_position = drive->file_position;

	volume->files = 0;
	volume->nb_files = 0;

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

