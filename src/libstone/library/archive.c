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
*  Last modified: Sun, 09 Dec 2012 11:10:37 +0100                         *
\*************************************************************************/

// free, malloc
#include <stdlib.h>
// strcmp, strdup
#include <string.h>
// struct stat
#include <sys/stat.h>

#include <libstone/library/archive.h>
#include <libstone/util/file.h>

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
	free(file->name);
	free(file->mime_type);
	free(file->db_data);
	free(file);
}

struct st_archive_file * st_archive_file_new(struct stat * file, const char * filename) {
	struct st_archive_file * f = malloc(sizeof(struct st_archive_file));
	f->name = strdup(filename);
	f->perm = file->st_mode & 07777;

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
	st_util_file_uid2name(f->owner, 32, file->st_uid);
	f->groupid = file->st_gid;
	st_util_file_gid2name(f->group, 32, file->st_gid);
	f->ctime = file->st_ctime;
	f->mtime = file->st_mtime;
	f->size = file->st_size;

	f->mime_type = NULL;

	f->digests = NULL;
	f->nb_checksums = 0;

	f->archive = NULL;
	f->selected_path = NULL;

	f->db_data = NULL;

	return f;
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

