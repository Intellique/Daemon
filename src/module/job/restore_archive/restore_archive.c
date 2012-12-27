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
*  Last modified: Thu, 27 Dec 2012 13:21:28 +0100                         *
\*************************************************************************/

// free, malloc
#include <stdlib.h>
// chmod
#include <sys/stat.h>
// chown, sleep
#include <unistd.h>

#include <libstone/job.h>
#include <libstone/library/drive.h>
#include <libstone/library/ressource.h>
#include <libstone/library/slot.h>
#include <libstone/log.h>
#include <libstone/user.h>
#include <libstone/util/file.h>
#include <stoned/library/changer.h>

#include "restore_archive.h"

static bool st_job_restore_archive_check(struct st_job * job);
static void st_job_restore_archive_free(struct st_job * job);
static void st_job_restore_archive_init(void) __attribute__((constructor));
static void st_job_restore_archive_new_job(struct st_job * job, struct st_database_connection * db);
static int st_job_restore_archive_run(struct st_job * job);

static struct st_job_ops st_job_restore_archive_ops = {
	.check = st_job_restore_archive_check,
	.free  = st_job_restore_archive_free,
	.run   = st_job_restore_archive_run,
};

static struct st_job_driver st_job_restore_archive_driver = {
	.name        = "restore-archive",
	.new_job     = st_job_restore_archive_new_job,
	.cookie      = NULL,
	.api_level   = {
		.checksum = 0,
		.database = 0,
		.job      = STONE_JOB_API_LEVEL,
	},
};


static bool st_job_restore_archive_check(struct st_job * job) {
	job->sched_status = st_job_status_idle;
	job->repetition = 1;
	return true;
}

static void st_job_restore_archive_free(struct st_job * job) {
	struct st_job_restore_archive_private * self = job->data;
	if (self != NULL) {
		self->connect->ops->close(self->connect);
		self->connect->ops->free(self->connect);
	}

	free(self);
	job->data = NULL;
}

static void st_job_restore_archive_init(void) {
	st_job_register_driver(&st_job_restore_archive_driver);
}

static void st_job_restore_archive_new_job(struct st_job * job, struct st_database_connection * db) {
	struct st_job_restore_archive_private * self = malloc(sizeof(struct st_job_restore_archive_private));
	self->job = job;
	self->connect = db->config->ops->connect(db->config);

	self->archive = NULL;

	self->first_worker = self->last_worker = NULL;
	self->checks = NULL;

	job->data = self;
	job->ops = &st_job_restore_archive_ops;
}

static int st_job_restore_archive_run(struct st_job * job) {
	struct st_job_restore_archive_private * self = job->data;

	st_job_add_record(self->connect, st_log_level_info, job, "Start restore job (named: %s), num runs %ld", job->name, job->num_runs);

	if (!job->user->can_restore) {
		st_job_add_record(self->connect, st_log_level_error, job, "Error, user (%s) cannot restore archive", job->user->login);
		return 1;
	}

	struct st_archive * archive = self->archive = self->connect->ops->get_archive_volumes_by_job(self->connect, job);
	if (archive == NULL) {
		st_job_add_record(self->connect, st_log_level_error, job, "Error, no archive associated to this job were found");
		job->sched_status = st_job_status_error;
		return 2;
	}

	job->done = 0.01;

	unsigned int nb_directories;
	struct st_archive_file * directories = self->connect->ops->get_archive_file_for_restore_directory(self->connect, job, &nb_directories);
	ssize_t size = self->connect->ops->get_restore_size_by_job(self->connect, job);

	unsigned int i;
	for (i = 0; i < nb_directories; i++) {
		struct st_archive_file * directory = directories + i;
		st_util_file_mkdir(directory->name, directory->perm);
		chmod(directory->name, directory->perm);
	}

	job->done = 0.02;

	self->restore_path = st_job_restore_archive_path_new();

	for (i = 0; i < archive->nb_volumes; i++) {
		struct st_archive_volume * vol = archive->volumes + i;

		bool found = false;
		struct st_job_restore_archive_data_worker * worker = self->first_worker;
		while (!found && worker != NULL) {
			if (vol->media == worker->media)
				found = true;

			worker = worker->next;
		}

		if (found)
			continue;

		bool stop = false;
		struct st_slot * slot = NULL;
		struct st_drive * drive = NULL;
		while (!stop) {
			// compute progression
			ssize_t total_done = 0;
			struct st_job_restore_archive_data_worker * w1 = self->first_worker;
			while (w1 != NULL) {
				total_done += w1->total_restored;
				w1 = w1->next;
			}
			float done = total_done;
			done *= 0.97;
			done /= size;
			job->done = 0.02 + done;

			slot = st_changer_find_slot_by_media(vol->media);
			if (slot == NULL) {
				// slot not found
				// TODO: alert user

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

		worker = st_job_restore_archive_data_worker_new(self, drive, slot, self->restore_path);
		if (self->first_worker == NULL)
			self->first_worker = self->last_worker = worker;
		else
			self->last_worker = self->last_worker->next = worker;
	}

	struct st_job_restore_archive_data_worker * worker = self->first_worker;
	while (worker != NULL) {
		while (!st_job_restore_archive_data_worker_wait(worker)) {
			// compute progression
			ssize_t total_done = 0;
			struct st_job_restore_archive_data_worker * w1 = self->first_worker;
			while (w1 != NULL) {
				total_done += w1->total_restored;
				w1 = w1->next;
			}
			float done = total_done;
			done *= 0.97;
			done /= size;
			job->done = 0.02 + done;
		}

		struct st_job_restore_archive_data_worker * next = worker->next;
		worker = next;
	}

	worker = self->first_worker;
	while (worker != NULL) {
		struct st_job_restore_archive_data_worker * next = worker->next;
		st_job_restore_archive_data_worker_free(worker);
		worker = next;
	}
	self->first_worker = self->last_worker = NULL;

	job->done = 0.99;

	for (i = 1; i <= nb_directories; i++) {
		struct st_archive_file * directory = directories + (nb_directories - i);

		chown(directory->name, directory->ownerid, directory->groupid);
		chmod(directory->name, directory->perm);

		struct timeval tv[] = {
			{ directory->mtime, 0 },
			{ directory->mtime, 0 },
		};
		utimes(directory->name, tv);

		free(directory->name);
		free(directory->mime_type);
		free(directory->db_data);
	}
	free(directories);

	job->done = 1;

	return 0;
}

