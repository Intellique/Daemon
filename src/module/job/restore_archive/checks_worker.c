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
*  Last modified: Fri, 28 Dec 2012 22:03:36 +0100                         *
\*************************************************************************/

// open
#include <fcntl.h>
// free, malloc
#include <stdlib.h>
// strcmp
#include <string.h>
// open
#include <sys/stat.h>
// open
#include <sys/types.h>
// close, read
#include <unistd.h>

#include <libstone/io.h>
#include <libstone/thread_pool.h>

#include "restore_archive.h"

static void st_job_restore_archive_checks_worker_check(struct st_job_restore_archive_checks_worker * check, struct st_archive_file * file);
static void st_job_restore_archive_checks_worker_work(void * arg);


void st_job_restore_archive_checks_worker_add_file(struct st_job_restore_archive_checks_worker * check, struct st_archive_file * file) {
	pthread_mutex_lock(&check->lock);

	struct st_job_restore_archive_checks_files * ptr_file = check->first_file;
	while (ptr_file != NULL) {
		if (!strcmp(file->name, ptr_file->file->name))
			break;
		ptr_file = ptr_file->next;
	}

	if (ptr_file != NULL) {
		ptr_file->nb_volume_restored++;
		st_archive_file_free(file);
	} else {
		ptr_file = malloc(sizeof(struct st_job_restore_archive_checks_files));
		ptr_file->file = file;
		ptr_file->checked = false;
		ptr_file->nb_volume_restored = 1;
		ptr_file->nb_volumes = 0;
		ptr_file->next = NULL;

		if (check->first_file == NULL)
			check->first_file = check->last_file = ptr_file;
		else
			check->last_file = check->last_file->next = ptr_file;
	}

	pthread_cond_signal(&check->wait);
	pthread_mutex_unlock(&check->lock);
}

static void st_job_restore_archive_checks_worker_check(struct st_job_restore_archive_checks_worker * check, struct st_archive_file * file) {
	bool has_restore_to = check->connect->ops->has_restore_to_by_job(check->connect, check->jp->job);
	char * restore_to = st_job_restore_archive_path_get(check->jp->restore_path, check->connect, check->jp->job, file, has_restore_to);

	unsigned int nb_checksum = 0;
	char ** checksums = check->connect->ops->get_checksums_of_file(check->connect, file, &nb_checksum);

	if (nb_checksum > 0 && checksums != NULL) {
		struct st_stream_writer * writer = st_checksum_writer_new(NULL, checksums, nb_checksum, false);
		int fd = open(restore_to, O_RDONLY);
		char buffer[4096];
		ssize_t nb_read;

		while (nb_read = read(fd, buffer, 4096), nb_read > 0)
			writer->ops->write(writer, buffer, nb_read);

		close(fd);
		writer->ops->close(writer);

		char ** results = st_checksum_writer_get_checksums(writer);

		bool ok = check->connect->ops->check_checksums_of_file(check->connect, file, checksums, results, nb_checksum);

		writer->ops->free(writer);

		unsigned int i;
		for (i = 0; i < nb_checksum; i++) {
			free(checksums[i]);
			free(results[i]);
		}
		free(checksums);
		free(results);
	}
}

void st_job_restore_archive_checks_worker_free(struct st_job_restore_archive_checks_worker * check) {
	if (check == NULL)
		return;

	pthread_mutex_lock(&check->lock);
	check->running = false;
	pthread_cond_signal(&check->wait);
	pthread_cond_wait(&check->wait, &check->lock);
	pthread_mutex_unlock(&check->lock);

	struct st_job_restore_archive_checks_files * elt = check->first_file;
	while (elt != NULL) {
		st_archive_file_free(elt->file);

		struct st_job_restore_archive_checks_files * next = elt->next;
		free(elt);
		elt = next;
	}

	pthread_mutex_destroy(&check->lock);
	pthread_cond_destroy(&check->wait);

	check->connect->ops->free(check->connect);

	free(check);
}

struct st_job_restore_archive_checks_worker * st_job_restore_archive_checks_worker_new(struct st_job_restore_archive_private * jp) {
	struct st_job_restore_archive_checks_worker * check = malloc(sizeof(struct st_job_restore_archive_checks_worker));
	check->jp = jp;
	check->first_file = check->last_file = NULL;
	pthread_mutex_init(&check->lock, NULL);
	pthread_cond_init(&check->wait, NULL);
	check->running = true;
	check->connect = jp->connect->config->ops->connect(jp->connect->config);

	st_thread_pool_run2(st_job_restore_archive_checks_worker_work, check, 8);

	return check;
}

static void st_job_restore_archive_checks_worker_work(void * arg) {
	struct st_job_restore_archive_checks_worker * self = arg;

	pthread_mutex_lock(&self->lock);
	while (self->running) {
		struct st_job_restore_archive_checks_files * elt = self->first_file;
		bool checked = false;
		while (elt != NULL) {
			if (elt->nb_volumes == 0)
				elt->nb_volumes = self->connect->ops->get_nb_volume_of_file(self->connect, self->jp->job, elt->file);

			if (elt->nb_volume_restored == elt->nb_volumes && !elt->checked) {
				pthread_mutex_unlock(&self->lock);

				st_job_restore_archive_checks_worker_check(self, elt->file);
				checked = true;

				pthread_mutex_lock(&self->lock);
			}
			elt = elt->next;
		}

		if (!checked)
			pthread_cond_wait(&self->wait, &self->lock);
	}

	pthread_cond_signal(&self->wait);
	pthread_mutex_unlock(&self->lock);
}

