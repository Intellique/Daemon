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
*  Copyright (C) 2013-2021, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// open
#include <fcntl.h>
// magic_file, magic_load, magic_open
#include <magic.h>
// pthread_mutex_*
#include <pthread.h>
// bool
#include <stdbool.h>
// free, malloc
#include <stdlib.h>
// strdup
#include <string.h>
// bzero
#include <strings.h>
// lstat, open
#include <sys/stat.h>
// lstat, open
#include <sys/types.h>
// lstat
#include <unistd.h>

#include <libstoriqone/archive.h>
#include <libstoriqone/file.h>
#include <libstoriqone/io.h>
#include <libstoriqone/string.h>
#include <libstoriqone/thread_pool.h>
#include <libstoriqone/value.h>

#include "meta_worker.h"

struct meta_worker_list {
	char * filename;
	char * selected_file;
	struct meta_worker_list * next;
};

static void soj_create_archive_meta_worker_do(void * arg);
static void soj_create_archive_meta_worker_do2(const char * filename, struct so_value * checksums, const char * selected_file);

static volatile bool meta_worker_do = false;
static volatile bool meta_worker_stop = false;
static struct meta_worker_list * file_first = NULL, * file_last = NULL;
static struct so_value * files = NULL;
static magic_t magicFile;

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t wait = PTHREAD_COND_INITIALIZER;


void soj_create_archive_meta_worker_add_file(const char * filename, const char * selected_file) {
	struct meta_worker_list * ptr;

	pthread_mutex_lock(&lock);

	bool found = so_value_hashtable_has_key2(files, filename);
	for (ptr = file_first; !found && ptr != NULL; ptr = ptr->next)
		found = strcmp(filename, ptr->filename) == 0;

	pthread_mutex_unlock(&lock);

	if (found)
		return;

	struct meta_worker_list * file = malloc(sizeof(struct meta_worker_list));
	file->filename = so_string_unescape(filename);
	file->selected_file = strdup(selected_file);
	file->next = NULL;

	pthread_mutex_lock(&lock);
	if (file_first == NULL)
		file_first = file_last = file;
	else
		file_last = file_last->next = file;
	pthread_cond_signal(&wait);
	pthread_mutex_unlock(&lock);
}

struct so_value * soj_create_archive_meta_worker_get_files() {
	return files;
}

static void soj_create_archive_meta_worker_do(void * arg) {
	struct so_value * checksums = arg;
	magicFile = magic_open(MAGIC_MIME_TYPE);
	magic_load(magicFile, NULL);

	meta_worker_do = true;
	files = so_value_new_hashtable2();

	for (;;) {
		pthread_mutex_lock(&lock);
		if (meta_worker_stop && file_first == NULL)
			break;

		if (file_first == NULL) {
			pthread_cond_signal(&wait);
			pthread_cond_wait(&wait, &lock);
		}

		struct meta_worker_list * files = file_first;
		file_first = file_last = NULL;
		pthread_mutex_unlock(&lock);

		while (files != NULL) {
			soj_create_archive_meta_worker_do2(files->filename, checksums, files->selected_file);

			struct meta_worker_list * next = files->next;
			free(files->filename);
			free(files->selected_file);
			free(files);
			files = next;
		}
	}

	pthread_cond_signal(&wait);
	pthread_mutex_unlock(&lock);

	magic_close(magicFile);
	so_value_free(checksums);
}

static void soj_create_archive_meta_worker_do2(const char * filename, struct so_value * checksums, const char * selected_file) {
	struct stat st;
	if (lstat(filename, &st) != 0)
		return;

	struct so_archive_file * file = malloc(sizeof(struct so_archive_file));
	bzero(file, sizeof(struct so_archive_file));

	file->path = so_string_dup_and_fix(filename, NULL);
	file->perm = st.st_mode & 07777;
	file->type = so_archive_file_mode_to_type(st.st_mode);
	file->ownerid = st.st_uid;
	file->owner = so_file_uid2name(st.st_uid);
	file->groupid = st.st_gid;
	file->group = so_file_gid2name(st.st_gid);

	file->create_time = st.st_ctime;
	file->modify_time = st.st_mtime;

	file->check_ok = false;
	file->check_time = 0;

	file->size = st.st_size;
	file->selected_path = strdup(selected_file);

	so_archive_file_update_hash(file);

	const char * mime_type = magic_file(magicFile, filename);
	if (mime_type == NULL) {
		file->mime_type = strdup("");
		// st_job_add_record(self->connect, st_log_level_info, self->job, st_job_record_notif_normal, "File (%s) has not mime type", f->file);
	} else {
		file->mime_type = strdup(mime_type);
		// st_job_add_record(self->connect, st_log_level_info, self->job, st_job_record_notif_normal, "Mime type of file '%s' is '%s'", f->file, mime_type);
	}

	if (S_ISREG(st.st_mode)) {
		int fd = open(filename, O_RDONLY);
		struct so_stream_writer * writer = so_io_checksum_writer_new(NULL, checksums, false);

		static char buffer[65536];
		ssize_t nb_read;

		while (nb_read = read(fd, buffer, 65536), nb_read > 0)
			writer->ops->write(writer, buffer, nb_read);

		close(fd);
		writer->ops->close(writer);

		file->digests = so_io_checksum_writer_get_checksums(writer);

		writer->ops->free(writer);
	} else
		file->digests = NULL;

	so_value_hashtable_put2(files, file->path, so_value_new_custom(file, NULL), true);
}

void soj_create_archive_meta_worker_start(struct so_value * checksums) {
	if (!meta_worker_do)
		so_thread_pool_run2("meta worker", soj_create_archive_meta_worker_do, so_value_share(checksums), 8);
}

void soj_create_archive_meta_worker_wait(bool stop) {
	pthread_mutex_lock(&lock);
	meta_worker_stop = stop;
	pthread_cond_signal(&wait);
	pthread_cond_wait(&wait, &lock);
	pthread_mutex_unlock(&lock);
}
