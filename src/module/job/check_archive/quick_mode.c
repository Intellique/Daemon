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
*  Copyright (C) 2013, Clercin guillaume <gclercin@intellique.com>           *
*  Last modified: Sun, 17 Feb 2013 16:39:01 +0100                            *
\****************************************************************************/

// pthread_cond_t
#include <pthread.h>
// free, malloc
#include <stdlib.h>
// sleep
#include <unistd.h>

#include <libstone/io.h>
#include <libstone/job.h>
#include <libstone/library/drive.h>
#include <libstone/library/ressource.h>
#include <libstone/library/slot.h>
#include <libstone/log.h>
#include <libstone/thread_pool.h>
#include <libstone/util/hashtable.h>
#include <stoned/library/changer.h>

#include "check_archive.h"

struct st_job_check_archive_quick_mode_private {
	struct st_job_check_archive_private * jp;
	unsigned int nb_warnings;
	unsigned int nb_errors;

	ssize_t total_read;

	struct st_drive * drive;
	struct st_slot * slot;
	struct st_media * media;

	pthread_mutex_t lock;
	pthread_cond_t wait;
	volatile bool running;

	struct st_job_check_archive_quick_mode_private * next;
};


static void st_job_check_archive_quick_mode_free(struct st_job_check_archive_quick_mode_private * qm);
static struct st_job_check_archive_quick_mode_private * st_job_check_archive_quick_mode_new(struct st_job_check_archive_private * self, struct st_drive * drive, struct st_slot * sl);
static bool st_job_check_archive_quick_mode_wait(struct st_job_check_archive_quick_mode_private * qm);
static void st_job_check_archive_quick_mode_work(void * arg);

int st_job_check_archive_quick_mode(struct st_job_check_archive_private * self) {
	struct st_archive * archive = self->archive = self->connect->ops->get_archive_volumes_by_job(self->connect, self->job);

	unsigned int i;
	ssize_t total_size = 0;
	for (i = 0; i < archive->nb_volumes; i++)
		total_size += archive->volumes[i].size;

	struct st_job_check_archive_quick_mode_private * first_worker = NULL, * last_worker = NULL;
	for (i = 0; i < archive->nb_volumes; i++) {
		struct st_archive_volume * vol = archive->volumes + i;

		bool found = false;
		struct st_job_check_archive_quick_mode_private * worker = first_worker;
		while (!found && worker != NULL) {
			if (vol->media == worker->media)
				found = true;

			worker = worker->next;
		}

		if (found)
			continue;

		bool stop = false, has_alerted_user = false;
		struct st_slot * slot = NULL;
		struct st_drive * drive = NULL;
		while (!stop) {
			ssize_t total_done = 0;
			worker = first_worker;
			while (worker != NULL) {
				total_done += worker->total_read;
				worker = worker->next;
			}
			float done = total_done;
			done *= 0.97;
			done /= total_size;
			self->job->done = 0.02 + done;

			slot = st_changer_find_slot_by_media(vol->media);
			if (slot == NULL) {
				// slot not found
				// TODO: alert user
				if (!has_alerted_user)
					st_job_add_record(self->connect, st_log_level_warning, self->job, "Warning, media named (%s) is not found, please insert it", vol->media->name);
				has_alerted_user = true;

				sleep(5);

				continue;
			}

			struct st_changer * changer = slot->changer;
			drive = slot->drive;

			if (drive != NULL) {
				if (drive->lock->ops->timed_lock(drive->lock, 5000))
					continue;

				stop = true;
			} else {
				if (slot->lock->ops->timed_lock(slot->lock, 5000))
					continue;

				drive = changer->ops->find_free_drive(changer, slot->media->format, true, false);

				if (drive == NULL) {
					sleep(5);
					continue;
				}

				stop = true;
			}
		}

		worker = st_job_check_archive_quick_mode_new(self, drive, slot);
		if (first_worker == NULL)
			first_worker = last_worker = worker;
		else
			last_worker = last_worker->next = worker;
	}

	struct st_job_check_archive_quick_mode_private * worker = first_worker;
	unsigned int nb_errors = 0;
	unsigned int nb_warnings = 0;
	do {
		bool finished = false;
		ssize_t total_done = 0;

		while (worker != NULL) {
			finished = st_job_check_archive_quick_mode_wait(worker);

			nb_errors += worker->nb_errors;
			nb_warnings += worker->nb_warnings;

			total_done += worker->total_read;

			worker = worker->next;
		}

		float done = total_done;
		done *= 0.97;
		done /= total_size;
		self->job->done = 0.02 + done;

		if (!finished)
			worker = first_worker;
	} while (worker != NULL);

	self->job->done = 0.99;

	self->connect->ops->mark_archive_as_checked(self->connect, archive, nb_errors == 0 && nb_warnings == 0);

	self->job->done = 1;

	st_job_check_archive_quick_mode_free(first_worker);

	return 0;
}

static void st_job_check_archive_quick_mode_free(struct st_job_check_archive_quick_mode_private * qm) {
	pthread_mutex_destroy(&qm->lock);
	pthread_cond_destroy(&qm->wait);

	if (qm->next != NULL)
		st_job_check_archive_quick_mode_free(qm->next);

	free(qm);
}

static struct st_job_check_archive_quick_mode_private * st_job_check_archive_quick_mode_new(struct st_job_check_archive_private * self, struct st_drive * drive, struct st_slot * sl) {
	struct st_job_check_archive_quick_mode_private * qm = malloc(sizeof(struct st_job_check_archive_quick_mode_private));
	qm->jp = self;
	qm->nb_warnings = qm->nb_errors = 0;
	qm->total_read = 0;

	qm->drive = drive;
	qm->slot = sl;
	qm->media = sl->media;

	pthread_mutex_init(&qm->lock, NULL);
	pthread_cond_init(&qm->wait, NULL);

	qm->running = true;
	qm->next = NULL;

	st_thread_pool_run(st_job_check_archive_quick_mode_work, qm);

	return qm;
}

static bool st_job_check_archive_quick_mode_wait(struct st_job_check_archive_quick_mode_private * qm) {
	bool finished = false;

	struct timeval now;
	struct timespec ts_timeout;
	gettimeofday(&now, NULL);
	ts_timeout.tv_sec = now.tv_sec + 1;
	ts_timeout.tv_nsec = now.tv_usec * 1000;

	pthread_mutex_lock(&qm->lock);
	if (qm->running)
		finished = pthread_cond_timedwait(&qm->wait, &qm->lock, &ts_timeout) ? false : true;
	else
		finished = true;
	pthread_mutex_unlock(&qm->lock);

	return finished;
}

static void st_job_check_archive_quick_mode_work(void * arg) {
	struct st_job_check_archive_quick_mode_private * qm = arg;
	struct st_database_connection * connect = qm->jp->connect->config->ops->connect(qm->jp->connect->config);

	qm->running = true;

	struct st_drive * dr = qm->drive;
	struct st_slot * sl = qm->slot;

	if (dr->slot->media != NULL && dr->slot != sl) {
		st_job_add_record(connect, st_log_level_info, qm->jp->job, "Unloading media (%s)", dr->slot->media->name);
		int failed = dr->changer->ops->unload(dr->changer, dr);
		if (failed) {
			st_job_add_record(connect, st_log_level_info, qm->jp->job, "Unloading media (%s) has failed", dr->slot->media->name);
			qm->nb_errors++;
			goto end_of_work;
		}
	}

	if (dr->slot->media == NULL) {
		st_job_add_record(connect, st_log_level_info, qm->jp->job, "Loading media (%s)", sl->media->name);
		int failed = dr->changer->ops->load_slot(dr->changer, sl, dr);
		if (failed) {
			st_job_add_record(connect, st_log_level_info, qm->jp->job, "Loading media (%s) has failed", sl->media->name);
			qm->nb_errors++;
			goto end_of_work;
		}
	}

	struct st_archive * archive = qm->jp->archive;

	unsigned int i;
	for (i = 0; i < archive->nb_volumes; i++) {
		struct st_archive_volume * vol = archive->volumes + i;

		if (vol->media != qm->media)
			continue;

		unsigned int nb_checksums = connect->ops->get_checksums_of_archive_volume(connect, vol);
		const void ** checksums = st_hashtable_keys(vol->digests, NULL);

		struct st_stream_writer * writer = st_checksum_writer_new(NULL, (char **) checksums, nb_checksums, true);
		struct st_stream_reader * reader = dr->ops->get_raw_reader(dr, vol->media_position);

		free(checksums);
		checksums = NULL;

		char buffer[4096];
		ssize_t nb_read;

		while (nb_read = reader->ops->read(reader, buffer, 4096), nb_read > 0) {
			writer->ops->write(writer, buffer, nb_read);

			qm->total_read += nb_read;
		}

		bool ok = true;
		if (nb_read < 0) {
			st_job_add_record(connect, st_log_level_info, qm->jp->job, "Error while reading from volume #%lu of archive (%s) because %m", vol->sequence, archive->name);
			qm->nb_errors++;
			ok = false;
		}

		reader->ops->close(reader);
		writer->ops->close(writer);

		if (ok) {
			struct st_hashtable * results = st_checksum_writer_get_checksums(writer);
			bool ok = st_hashtable_equals(vol->digests, results);
			st_hashtable_free(results);

			if (ok) {
				connect->ops->mark_archive_volume_as_checked(connect, vol, true);
				st_job_add_record(connect, st_log_level_info, qm->jp->job, "Checking volume #%lu, status: OK", vol->sequence);
			} else {
				connect->ops->mark_archive_volume_as_checked(connect, vol, false);
				st_job_add_record(connect, st_log_level_error, qm->jp->job, "Checking volume #%lu, status: checksum mismatch", vol->sequence);
			}
		}

		reader->ops->free(reader);
		writer->ops->free(writer);
	}

end_of_work:
	dr->lock->ops->unlock(dr->lock);

	connect->ops->free(connect);

	pthread_mutex_lock(&qm->lock);
	qm->running = false;
	pthread_cond_signal(&qm->wait);
	pthread_mutex_unlock(&qm->lock);
}

