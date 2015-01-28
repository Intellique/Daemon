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
*  Copyright (C) 2013-2015, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Thu, 23 Jan 2014 13:40:04 +0100                            *
\****************************************************************************/

#define _GNU_SOURCE
// mknod, open
#include <fcntl.h>
// free, malloc
#include <stdlib.h>
// asprintf
#include <stdio.h>
// strcmp, strdup, strrchr
#include <string.h>
// S_*, mknod, open
#include <sys/stat.h>
// futimes, utimes
#include <sys/time.h>
// S_*, lseek, open, mknod
#include <sys/types.h>
// access, chmod, chown, fchmod, fchown, lseek, mknod, write
#include <unistd.h>

#include <libstone/job.h>
#include <libstone/format.h>
#include <libstone/library/changer.h>
#include <libstone/library/drive.h>
#include <libstone/library/ressource.h>
#include <libstone/library/slot.h>
#include <libstone/log.h>
#include <libstone/thread_pool.h>
#include <libstone/util/file.h>
#include <libstone/util/string.h>

#include "common.h"

static void st_job_restore_archive_data_worker_work(void * arg);

void st_job_restore_archive_data_worker_free(struct st_job_restore_archive_data_worker * worker) {
	pthread_mutex_destroy(&worker->lock);
	pthread_cond_destroy(&worker->wait);

	free(worker);
}

struct st_job_restore_archive_data_worker * st_job_restore_archive_data_worker_new(struct st_job_restore_archive_private * self, struct st_drive * drive, struct st_slot * slot) {
	struct st_job_restore_archive_data_worker * worker = malloc(sizeof(struct st_job_restore_archive_data_worker));
	worker->jp = self;
	worker->nb_warnings = 0;
	worker->nb_errors = 0;

	worker->total_restored = 0;

	worker->drive = drive;
	worker->slot = slot;
	worker->media = slot->media;

	pthread_mutex_init(&worker->lock, NULL);
	pthread_cond_init(&worker->wait, NULL);

	worker->running = true;
	worker->next = NULL;

	char * th_name;
	asprintf(&th_name, "restore archive worker: %s", self->archive->name);

	st_thread_pool_run(th_name, st_job_restore_archive_data_worker_work, worker);

	free(th_name);

	return worker;
}

bool st_job_restore_archive_data_worker_wait(struct st_job_restore_archive_data_worker * worker) {
	bool finished = false;

	struct timeval now;
	struct timespec ts_timeout;
	gettimeofday(&now, NULL);
	ts_timeout.tv_sec = now.tv_sec + 5;
	ts_timeout.tv_nsec = now.tv_usec * 1000;

	pthread_mutex_lock(&worker->lock);
	if (worker->running)
		finished = pthread_cond_timedwait(&worker->wait, &worker->lock, &ts_timeout) ? false : true;
	else
		finished = true;
	pthread_mutex_unlock(&worker->lock);

	return finished;
}

static void st_job_restore_archive_data_worker_work(void * arg) {
	struct st_job_restore_archive_data_worker * self = arg;
	struct st_database_connection * connect = self->jp->job->db_config->ops->connect(self->jp->job->db_config);

	self->running = true;

	struct st_job * job = self->jp->job;
	struct st_drive * dr = self->drive;
	struct st_slot * sl = self->slot;

	bool has_slot_lock = dr->slot != sl;

	if (job->db_status == st_job_status_stopped)
		goto end_of_work;

	if (dr->slot->media != NULL && dr->slot != sl) {
		st_job_add_record(connect, st_log_level_info, job, st_job_record_notif_normal, "Unloading media (%s)", dr->slot->media->name);
		int failed = dr->changer->ops->unload(dr->changer, dr);
		if (failed) {
			st_job_add_record(connect, st_log_level_info, job, st_job_record_notif_important, "Unloading media (%s) has failed", dr->slot->media->name);
			self->nb_errors++;
			goto end_of_work;
		}
	}

	if (job->db_status == st_job_status_stopped)
		goto end_of_work;

	if (dr->slot->media == NULL) {
		st_job_add_record(connect, st_log_level_info, job, st_job_record_notif_normal, "Loading media (%s)", sl->media->name);
		int failed = dr->changer->ops->load_slot(dr->changer, sl, dr);
		if (failed) {
			st_job_add_record(connect, st_log_level_info, job, st_job_record_notif_important, "Loading media (%s) has failed", sl->media->name);
			self->nb_errors++;
			goto end_of_work;
		} else {
			sl->lock->ops->unlock(sl->lock);
			has_slot_lock = false;
		}
	}

	struct st_archive * archive = self->jp->archive;
	bool has_restore_to = connect->ops->has_restore_to_by_job(connect, job);

	unsigned int i;
	for (i = 0; i < archive->nb_volumes && job->db_status != st_job_status_stopped; i++) {
		struct st_archive_volume * vol = archive->volumes + i;

		if (vol->media != self->media)
			continue;

		st_job_restore_archive_report_add_volume(self->jp->report, vol, NULL);

		struct st_format_reader * reader = dr->ops->get_reader(dr, vol->media_position, NULL, NULL);

		unsigned int j;
		for (j = 0; j < vol->nb_files && job->db_status != st_job_status_stopped; j++) {
			struct st_archive_files * f = vol->files + j;
			struct st_archive_file * file = f->file;

			enum st_format_reader_header_status status = reader->ops->forward(reader, f->position);
			if (status != st_format_reader_header_ok) {
				st_job_add_record(connect, st_log_level_error, job, st_job_record_notif_important, "Error while seeking to file (%s)", file->name);
				self->nb_errors++;

				st_archive_file_free(file);
				break;
			}

			struct st_format_file header;
			do {
				status = reader->ops->get_header(reader, &header);
			} while (status == st_format_reader_header_bad_header);

			if (status != st_format_reader_header_ok) {
				st_job_add_record(connect, st_log_level_error, job, st_job_record_notif_important, "Error while reading header of file (%s)", file->name);
				st_log_write_all(st_log_level_debug, st_log_type_job, "Error while reading header file, line %d", __LINE__);
				self->nb_errors++;

				st_archive_file_free(file);
				break;
			}

			st_util_string_delete_double_char(header.filename, '/');
			st_util_string_rtrim(header.filename, '/');

			while (strcmp(header.filename, file->name + 1)) {
				st_job_add_record(connect, st_log_level_debug, job, st_job_record_notif_normal, "Skipping file '%s'", header.filename);

				reader->ops->skip_file(reader);
				st_format_file_free(&header);
				status = reader->ops->get_header(reader, &header);

				if (status != st_format_reader_header_ok)
					break;

				st_util_string_delete_double_char(header.filename, '/');
				st_util_string_rtrim(header.filename, '/');
			}

			if (status != st_format_reader_header_ok) {
				st_job_add_record(connect, st_log_level_error, job, st_job_record_notif_important, "Error while reading header of file (%s)", file->name);
				st_log_write_all(st_log_level_debug, st_log_type_job, "Error while reading header file, line %d", __LINE__);
				self->nb_errors++;

				st_archive_file_free(file);
				break;
			}

			char * restore_to = st_job_restore_archive_path_get(self->jp->restore_path, connect, job, file, has_restore_to);

			st_job_restore_archive_report_add_file(self->jp->report, vol, f, restore_to);

			/*/
			 * Check and create if needed the path of file to be restored
			 */
			char * ptr = strrchr(restore_to, '/');
			if (ptr != NULL) {
				*ptr = '\0';
				if (access(restore_to, R_OK | W_OK | X_OK)) {
					st_job_add_record(connect, st_log_level_info, job, st_job_record_notif_normal, "Create missing directory '%s' with permission 0777", restore_to);
					st_util_file_mkdir(restore_to, 0777);
				}
				*ptr = '/';
			}

			st_job_add_record(connect, st_log_level_info, job, st_job_record_notif_normal, "Start restoring file '%s'", restore_to);

			if (S_ISREG(header.mode)) {
				int fd = open(restore_to, O_CREAT | O_WRONLY, header.mode & 07777);
				if (fd < 0) {
					st_job_add_record(connect, st_log_level_error, job, st_job_record_notif_important, "Error while opening file (%s) for writing because %m", restore_to);
					self->nb_errors++;

					st_archive_file_free(file);
					break;
				}

				if (header.position > 0) {
					if (lseek(fd, header.position, SEEK_SET) != header.position) {
						st_job_add_record(connect, st_log_level_error, job, st_job_record_notif_important, "Error while seeking into file (%s) because %m", restore_to);
						self->nb_errors++;
						close(fd);

						st_archive_file_free(file);
						break;
					}
				}

				ssize_t nb_read, nb_write = 0;
				char buffer[4096];
				while (nb_read = reader->ops->read(reader, buffer, 4096), nb_read > 0) {
					nb_write = write(fd, buffer, nb_read);

					if (nb_write > 0)
						self->total_restored += nb_write;
					else {
						st_job_add_record(connect, st_log_level_error, job, st_job_record_notif_important, "Error while writing to file (%s) because %m", restore_to);
						self->nb_errors++;

						st_archive_file_free(file);
						break;
					}
				}

				if (nb_read < 0) {
					st_job_add_record(connect, st_log_level_error, job, st_job_record_notif_important, "Error while reading from media (%s) because %m", vol->media->name);
					connect->ops->mark_archive_file_as_checked(connect, archive, file, false);
					self->nb_errors++;

					st_archive_file_free(file);
					break;
				} else if (nb_write >= 0) {
					if (fchown(fd, file->ownerid, file->groupid)) {
						st_job_add_record(connect, st_log_level_warning, job, st_job_record_notif_important, "Error while restoring user of file (%s) because %m", restore_to);
						self->nb_warnings++;
					}

					if (fchmod(fd, file->perm)) {
						st_job_add_record(connect, st_log_level_warning, job, st_job_record_notif_important, "Error while restoring permission of file (%s) because %m", restore_to);
						self->nb_warnings++;
					}

					struct timeval tv[] = {
						{ file->modify_time, 0 },
						{ file->modify_time, 0 },
					};
					if (futimes(fd, tv)) {
						st_job_add_record(connect, st_log_level_warning, job, st_job_record_notif_important, "Error while motification time of file (%s) because %m", restore_to);
						self->nb_warnings++;
					}
				}

				close(fd);

				st_job_restore_archive_checks_worker_add_file(self->jp->checks, file);
			} else if (S_ISDIR(header.mode)) {
				// do nothing because directory is already created
			} else if (S_ISLNK(header.mode)) {
				symlink(header.link, header.filename);
			} else if (S_ISFIFO(header.mode)) {
				mknod(file->name, S_IFIFO, 0);
			} else if (S_ISCHR(header.mode)) {
				mknod(file->name, S_IFCHR, header.dev);
			} else if (S_ISBLK(header.mode)) {
				mknod(file->name, S_IFBLK, header.dev);
			}

			if (!(S_ISREG(header.mode) || S_ISDIR(header.mode))) {
				if (chown(header.filename, file->ownerid, file->groupid)) {
					st_job_add_record(connect, st_log_level_warning, job, st_job_record_notif_important, "Error while restoring user of file (%s) because %m", header.filename);
					self->nb_warnings++;
				}

				if (chmod(header.filename, file->perm)) {
					st_job_add_record(connect, st_log_level_warning, job, st_job_record_notif_important, "Error while restoring permission of file (%s) because %m", header.filename);
					self->nb_warnings++;
				}

				struct timeval tv[] = {
					{ file->modify_time, 0 },
					{ file->modify_time, 0 },
				};
				if (utimes(header.filename, tv)) {
					st_job_add_record(connect, st_log_level_warning, job, st_job_record_notif_important, "Error while motification time of file (%s) because %m", header.filename);
					self->nb_warnings++;
				}
			}

			free(restore_to);
			st_format_file_free(&header);
		}

		reader->ops->close(reader);
		reader->ops->free(reader);
	}

end_of_work:
	if (has_slot_lock)
		sl->lock->ops->unlock(sl->lock);

	dr->lock->ops->unlock(dr->lock);

	connect->ops->free(connect);

	pthread_mutex_lock(&self->lock);
	self->running = false;
	pthread_cond_signal(&self->wait);
	pthread_mutex_unlock(&self->lock);
}

