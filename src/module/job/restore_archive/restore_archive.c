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
*  Last modified: Fri, 15 Nov 2013 12:33:36 +0100                            *
\****************************************************************************/

// json_*
#include <jansson.h>
// free, malloc
#include <stdlib.h>
// chmod
#include <sys/stat.h>
// utimes
#include <sys/time.h>
// utimes
#include <sys/types.h>
// chown, sleep
#include <unistd.h>
// utimes
#include <utime.h>

#include <libstone/io.h>
#include <libstone/job.h>
#include <libstone/library/drive.h>
#include <libstone/library/ressource.h>
#include <libstone/library/slot.h>
#include <libstone/log.h>
#include <libstone/script.h>
#include <libstone/user.h>
#include <libstone/util/file.h>
#include <stoned/library/changer.h>

#include <libjob-restore-archive.chcksum>

#include "common.h"

static bool st_job_restore_archive_check(struct st_job * job);
static void st_job_restore_archive_free(struct st_job * job);
static void st_job_restore_archive_init(void) __attribute__((constructor));
static void st_job_restore_archive_new_job(struct st_job * job, struct st_database_connection * db);
static bool st_job_restore_archive_pre_run_script(struct st_job * job);
static int st_job_restore_archive_run(struct st_job * job);

static struct st_job_ops st_job_restore_archive_ops = {
	.check          = st_job_restore_archive_check,
	.free           = st_job_restore_archive_free,
	.pre_run_script = st_job_restore_archive_pre_run_script,
	.run            = st_job_restore_archive_run,
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
	.src_checksum   = STONE_JOB_RESTOREARCHIVE_SRCSUM,
};


static bool st_job_restore_archive_check(struct st_job * job) {
	job->sched_status = st_job_status_scheduled;
	job->repetition = 1;
	return true;
}

static void st_job_restore_archive_free(struct st_job * job) {
	struct st_job_restore_archive_private * self = job->data;
	if (self->connect != NULL) {
		self->connect->ops->close(self->connect);
		self->connect->ops->free(self->connect);
		self->connect = NULL;
	}

	if (self->archive != NULL)
		st_archive_free(self->archive);
	self->archive = NULL;

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

	self->archive = self->connect->ops->get_archive_volumes_by_job(self->connect, job);

	self->first_worker = self->last_worker = NULL;
	self->checks = NULL;

	job->data = self;
	job->ops = &st_job_restore_archive_ops;
}

static bool st_job_restore_archive_pre_run_script(struct st_job * job) {
	struct st_job_restore_archive_private * self = job->data;

	json_t * data = json_object();

	struct st_pool * pool = st_pool_get_by_archive(self->archive, self->connect);
	json_t * returned_data = st_script_run(self->connect, st_script_type_pre, pool, data);
	bool sr = st_io_json_should_run(returned_data);

	json_decref(returned_data);
	json_decref(data);

	return sr;
}

static int st_job_restore_archive_run(struct st_job * job) {
	struct st_job_restore_archive_private * self = job->data;

	st_job_add_record(self->connect, st_log_level_info, job, "Start restore job (named: %s), num runs %ld", job->name, job->num_runs);

	if (!job->user->can_restore) {
		st_job_add_record(self->connect, st_log_level_error, job, "Error, user (%s) cannot restore archive", job->user->login);
		return 1;
	}

	if (self->archive == NULL) {
		st_job_add_record(self->connect, st_log_level_error, job, "Error, no archive associated to this job were found");
		job->sched_status = st_job_status_error;
		return 2;
	}

	self->report = st_job_restore_archive_report_new(job, self->archive);

	self->checks = st_job_restore_archive_checks_worker_new(self);

	job->done = 0.01;

	unsigned int nb_directories;
	struct st_archive_file * directories = self->connect->ops->get_archive_file_for_restore_directory(self->connect, job, &nb_directories);
	ssize_t size = self->connect->ops->get_restore_size_by_job(self->connect, job);

	unsigned int i;
	unsigned int nb_errors = 0, nb_warnings = 0;
	for (i = 0; i < nb_directories; i++) {
		struct st_archive_file * directory = directories + i;
		st_util_file_mkdir(directory->name, directory->perm);

		if (chown(directory->name, directory->ownerid, directory->groupid)) {
			st_job_add_record(self->connect, st_log_level_warning, job, "Warning, failed to restore owner of directory (%s) because %m", directory->name);
			nb_warnings++;
		}

		if (chmod(directory->name, directory->perm)) {
			st_job_add_record(self->connect, st_log_level_warning, job, "Warning, failed to restore permission of directory (%s) because %m", directory->name);
			nb_warnings++;
		}
	}

	job->done = 0.02;

	self->restore_path = st_job_restore_archive_path_new();

	for (i = 0; i < self->archive->nb_volumes; i++) {
		struct st_archive_volume * vol = self->archive->volumes + i;

		bool found = false;
		struct st_job_restore_archive_data_worker * worker = self->first_worker;
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
			unsigned int nb_running_worker = 0;

			// compute progression
			ssize_t total_done = 0;
			struct st_job_restore_archive_data_worker * w1 = self->first_worker;
			while (w1 != NULL) {
				total_done += w1->total_restored;

				if (w1->running)
					nb_running_worker++;

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
				if (!has_alerted_user)
					st_job_add_record(self->connect, st_log_level_warning, job, "Warning, media named (%s) is not found, please insert it", vol->media->name);
				has_alerted_user = true;

				if (nb_running_worker == 0)
					job->sched_status = st_job_status_pause;

				sleep(5);

				continue;
			}

			struct st_changer * changer = slot->changer;
			drive = slot->drive;

			if (drive != NULL) {
				if (drive->lock->ops->timed_lock(drive->lock, 5000)) {
					if (nb_running_worker == 0)
						job->sched_status = st_job_status_pause;

					continue;
				}

				stop = true;
			} else {
				if (slot->lock->ops->timed_lock(slot->lock, 5000)) {
					if (nb_running_worker == 0)
						job->sched_status = st_job_status_pause;

					continue;
				}

				drive = changer->ops->find_free_drive(changer, slot->media->format, true, false);

				if (drive == NULL) {
					slot->lock->ops->unlock(slot->lock);

					if (nb_running_worker == 0)
						job->sched_status = st_job_status_pause;

					sleep(5);
					continue;
				}

				stop = true;
			}
		}

		job->sched_status = st_job_status_running;

		worker = st_job_restore_archive_data_worker_new(self, drive, slot);
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
		nb_errors += worker->nb_errors;
		nb_warnings += worker->nb_warnings;

		struct st_job_restore_archive_data_worker * next = worker->next;
		st_job_restore_archive_data_worker_free(worker);
		worker = next;
	}
	self->first_worker = self->last_worker = NULL;

	job->done = 0.99;

	for (i = 1; i <= nb_directories; i++) {
		struct st_archive_file * directory = directories + (nb_directories - i);

		struct timeval tv[] = {
			{ directory->modify_time, 0 },
			{ directory->modify_time, 0 },
		};
		if (utimes(directory->name, tv)) {
			st_job_add_record(self->connect, st_log_level_warning, job, "Warning, failed to restore motification time of directory (%s) because %m", directory->name);
			nb_warnings++;
		}

		free(directory->name);
		free(directory->mime_type);
		free(directory->db_data);
	}
	free(directories);

	st_job_restore_archive_checks_worker_free(self->checks);

	char * report = st_job_restore_archive_report_make(self->report);
	if (report != NULL)
		self->connect->ops->add_report(self->connect, job, self->archive, report);
	free(report);

	job->done = 1;

	st_job_add_record(self->connect, st_log_level_info, job, "Job restore-archive is finished (named: %s) with %d warning(%c) and %d error(%c)", job->name, nb_warnings, nb_warnings != 1 ? 's' : '\0', nb_errors, nb_errors != 1 ? 's' : '\0');

	st_job_restore_archive_report_free(self->report);
	st_job_restore_archive_path_free(self->restore_path);

	return 0;
}

