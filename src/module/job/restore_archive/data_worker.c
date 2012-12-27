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
*  Last modified: Thu, 27 Dec 2012 22:35:28 +0100                         *
\*************************************************************************/

// mknod, open
#include <fcntl.h>
// free, malloc
#include <stdlib.h>
// strcmp, strdup
#include <string.h>
// S_*, mknod, open
#include <sys/stat.h>
// futimes, utimes
#include <sys/time.h>
// S_*, lseek, open, mknod
#include <sys/types.h>
// chmod, chown, fchmod, fchown, lseek, mknod, write
#include <unistd.h>

#include <libstone/job.h>
#include <libstone/format.h>
#include <libstone/library/changer.h>
#include <libstone/library/drive.h>
#include <libstone/library/ressource.h>
#include <libstone/library/slot.h>
#include <libstone/log.h>
#include <libstone/thread_pool.h>

#include "restore_archive.h"

static void st_job_restore_archive_data_worker_work(void * arg);

void st_job_restore_archive_data_worker_free(struct st_job_restore_archive_data_worker * worker) {
	pthread_mutex_destroy(&worker->lock);
	pthread_cond_destroy(&worker->wait);

	free(worker);
}

struct st_job_restore_archive_data_worker * st_job_restore_archive_data_worker_new(struct st_job_restore_archive_private * self, struct st_drive * drive, struct st_slot * slot, struct st_job_restore_archive_path * restore_to) {
	struct st_job_restore_archive_data_worker * worker = malloc(sizeof(struct st_job_restore_archive_data_worker));
	worker->jp = self;

	worker->total_restored = 0;
	worker->restore_to = restore_to;

	worker->drive = drive;
	worker->slot = slot;
	worker->media = slot->media;

	pthread_mutex_init(&worker->lock, NULL);
	pthread_cond_init(&worker->wait, NULL);

	worker->running = true;
	worker->next = NULL;

	st_thread_pool_run(st_job_restore_archive_data_worker_work, worker);

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
	struct st_database_connection * connect = self->jp->connect->config->ops->connect(self->jp->connect->config);

	self->running = true;

	struct st_drive * dr = self->drive;
	struct st_slot * sl = self->slot;

	if (dr->slot->media != NULL && dr->slot != sl) {
		dr->changer->ops->unload(dr->changer, dr);
	}

	if (dr->slot->media == NULL) {
		dr->changer->ops->load_slot(dr->changer, sl, dr);
	}

	struct st_archive * archive = self->jp->archive;
	bool has_restore_to = connect->ops->has_restore_to_by_job(connect, self->jp->job);

	unsigned int i;
	for (i = 0; i < archive->nb_volumes; i++) {
		struct st_archive_volume * vol = archive->volumes + i;

		if (vol->media != self->media)
			continue;

		connect->ops->get_archive_files_by_job_and_archive_volume(connect, self->jp->job, vol);

		struct st_format_reader * reader = dr->ops->get_reader(dr, vol->media_position);

		unsigned int j;
		for (j = 0; j < vol->nb_files; j++) {
			struct st_archive_files * f = vol->files + j;
			struct st_archive_file * file = f->file;

			enum st_format_reader_header_status status = reader->ops->forward(reader, f->position);

			struct st_format_file header;
			status = reader->ops->get_header(reader, &header);

			while (strcmp(header.filename, file->name + 1)) {
				reader->ops->skip_file(reader);
				st_format_file_free(&header);
				status = reader->ops->get_header(reader, &header);
			}

			char * restore_to = st_job_restore_archive_path_get(self->restore_to, connect, self->jp->job, file, has_restore_to);

			if (S_ISREG(header.mode)) {
				int fd = open(restore_to, O_CREAT | O_WRONLY, header.mode & 07777);
				if (fd < 0) {
					st_job_add_record(connect, st_log_level_error, self->jp->job, "Error while opening file (%s) for writing because %m", restore_to);
				}

				if (header.position > 0) {
					lseek(fd, header.position, SEEK_SET);
				}

				ssize_t nb_read;
				char buffer[4096];
				while (nb_read = reader->ops->read(reader, buffer, 4096), nb_read > 0) {
					ssize_t nb_write = write(fd, buffer, nb_read);

					if (nb_write > 0)
						self->total_restored += nb_write;
				}

				fchown(fd, file->ownerid, file->groupid);
				fchmod(fd, file->perm);

				struct timeval tv[] = {
					{ file->mtime, 0 },
					{ file->mtime, 0 },
				};
				futimes(fd, tv);

				close(fd);
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
				chown(header.filename, file->ownerid, file->groupid);
				chmod(header.filename, file->perm);

				struct timeval tv[] = {
					{ file->mtime, 0 },
					{ file->mtime, 0 },
				};
				utimes(header.filename, tv);
			}

			st_format_file_free(&header);
		}

		reader->ops->close(reader);
		reader->ops->free(reader);
	}

	dr->lock->ops->unlock(dr->lock);

	connect->ops->free(connect);

	pthread_mutex_lock(&self->lock);
	self->running = false;
	pthread_cond_signal(&self->wait);
	pthread_mutex_unlock(&self->lock);
}

