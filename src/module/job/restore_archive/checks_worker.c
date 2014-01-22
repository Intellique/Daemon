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
*  Last modified: Thu, 16 Jan 2014 16:51:40 +0100                            *
\****************************************************************************/

#define _GNU_SOURCE
// open
#include <fcntl.h>
// free, malloc
#include <stdlib.h>
// asprintf
#include <stdio.h>
// strcmp
#include <string.h>
// open
#include <sys/stat.h>
// open
#include <sys/types.h>
// close, read
#include <unistd.h>

#include <libstone/job.h>
#include <libstone/io.h>
#include <libstone/log.h>
#include <libstone/thread_pool.h>
#include <libstone/util/hashtable.h>

#include "common.h"

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
	if (file->digests != NULL)
		nb_checksum = file->digests->nb_elements;
	else
		nb_checksum = check->connect->ops->get_checksums_of_file(check->connect, file);

	if (nb_checksum > 0 && file->digests != NULL) {
		bool is_error = false;

		st_job_add_record(check->connect, st_log_level_info, check->jp->job, st_job_record_notif_important, "Start checking restored file '%s'", restore_to);

		int fd = open(restore_to, O_RDONLY);
		if (fd < 0) {
			st_job_add_record(check->connect, st_log_level_info, check->jp->job, st_job_record_notif_important, "Error while opening file (%s) because %m", restore_to);
			check->nb_errors++;
			is_error = true;
		}

		struct st_stream_writer * writer = NULL;
		if (!is_error) {
			char ** checksums = (char **) st_hashtable_keys(file->digests, NULL);
			writer = st_checksum_writer_new(NULL, checksums, nb_checksum, false);
			free(checksums);

			if (writer == NULL) {
				st_job_add_record(check->connect, st_log_level_info, check->jp->job, st_job_record_notif_important, "Error while getting handler of checksums");
				check->nb_errors++;
				is_error = true;
			}
		}

		char buffer[4096];
		ssize_t nb_read;

		if (!is_error) {
			while (nb_read = read(fd, buffer, 4096), nb_read > 0)
				writer->ops->write(writer, buffer, nb_read);

			if (nb_read < 0) {
				st_job_add_record(check->connect, st_log_level_info, check->jp->job, st_job_record_notif_important, "Error while reading from file (%s) because %m", restore_to);
				check->nb_errors++;
				is_error = true;
			}
		}

		if (fd >= 0)
			close(fd);
		if (writer != NULL)
			writer->ops->close(writer);

		if (is_error) {
			st_job_add_record(check->connect, st_log_level_info, check->jp->job, st_job_record_notif_important, "There was some error while checking file (%s)", restore_to);

			if (writer != NULL)
				writer->ops->free(writer);

			st_job_restore_archive_report_check_file(check->jp->report, file, false);
		} else {
			struct st_hashtable * results = st_checksum_writer_get_checksums(writer);
			bool ok = st_hashtable_equals(file->digests, results);
			st_hashtable_free(results);

			if (ok) {
				check->connect->ops->mark_archive_file_as_checked(check->connect, check->jp->archive, file, true);
				st_job_add_record(check->connect, st_log_level_info, check->jp->job, st_job_record_notif_normal, "Checking restored file (%s), status: OK", restore_to);
			} else
				st_job_add_record(check->connect, st_log_level_error, check->jp->job, st_job_record_notif_important, "Checking restored file (%s), status: checksum mismatch", restore_to);

			writer->ops->free(writer);

			st_job_restore_archive_report_check_file(check->jp->report, file, ok);
		}
	}

	free(restore_to);
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
	check->nb_errors = check->nb_warnings = 0;
	check->first_file = check->last_file = NULL;
	pthread_mutex_init(&check->lock, NULL);
	pthread_cond_init(&check->wait, NULL);
	check->running = true;
	check->connect = jp->connect->config->ops->connect(jp->connect->config);

	char * th_name;
	asprintf(&th_name, "check file worker: %s", jp->archive->name);

	st_thread_pool_run2(th_name, st_job_restore_archive_checks_worker_work, check, 8);

	free(th_name);

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

				pthread_mutex_lock(&self->lock);

				elt->checked = checked = true;
			}
			elt = elt->next;
		}

		if (!checked)
			pthread_cond_wait(&self->wait, &self->lock);
	}

	pthread_cond_signal(&self->wait);
	pthread_mutex_unlock(&self->lock);
}

