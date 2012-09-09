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
*  Last modified: Sun, 09 Sep 2012 12:08:17 +0200                         *
\*************************************************************************/

// free
#include <stdlib.h>
// sleep
#include <unistd.h>

#include <stone/io.h>
#include <stone/job.h>
#include <stone/library/backup-db.h>
#include <stone/library/changer.h>
#include <stone/library/drive.h>
#include <stone/library/ressource.h>
#include <stone/library/tape.h>
#include <stone/log.h>
#include <stone/user.h>

struct st_job_restoredb_private {
	char * buffer;
	ssize_t length;

	ssize_t nb_total_read;
	ssize_t total_size;

	struct st_changer * changer;
	struct st_drive * drive;
	struct st_slot * slot;

	int stop_request;
};

static void st_job_restoredb_free(struct st_job * job);
static void st_job_restoredb_init(void) __attribute__((constructor));
static int st_job_restoredb_load_tape(struct st_job * job, struct st_tape * tape);
static void st_job_restoredb_new_job(struct st_database_connection * db, struct st_job * job);
static int st_job_restoredb_run(struct st_job * job);
static int st_job_restoredb_stop(struct st_job * job);

struct st_job_ops st_job_restoredb_ops = {
	.free = st_job_restoredb_free,
	.run  = st_job_restoredb_run,
	.stop = st_job_restoredb_stop,
};

struct st_job_driver st_job_restoredb_driver = {
	.name        = "restore-db",
	.new_job     = st_job_restoredb_new_job,
	.cookie      = 0,
	.api_version = STONE_JOB_APIVERSION,
};


void st_job_restoredb_free(struct st_job * job) {
	struct st_job_restoredb_private * self = job->data;

	free(self->buffer);
	free(job->data);
}

void st_job_restoredb_init() {
	st_job_register_driver(&st_job_restoredb_driver);
}

int st_job_restoredb_load_tape(struct st_job * job, struct st_tape * tape) {
	struct st_job_restoredb_private * jp = job->data;
	short has_alert_user = 0;
	enum {
		alert_user,
		drive_is_free,
		look_for_changer,
		look_for_free_drive,
	} state = look_for_changer;

	unsigned int i;
	for (;;) {
		switch (state) {
			case alert_user:
				job->sched_status = st_job_status_pause;
				sleep(1);
				job->db_ops->update_status(job);

				if (!has_alert_user) {
					job->db_ops->add_record(job, st_log_level_warning, "Tape not found (named: %s)", tape->name);
					st_log_write_all(st_log_level_error, st_log_type_user_message, "Job: restore (id:%ld) request you to put a tape (named: %s) in your changer or standalone drive", job->id, tape->name);
				}
				has_alert_user = 1;
				sleep(5);

				job->sched_status = st_job_status_running;
				job->db_ops->update_status(job);

				state = look_for_changer;
				break;

			case drive_is_free:
				while (jp->drive->lock->ops->trylock(jp->drive->lock)) {
					sleep(5);

					if (jp->drive->slot->tape != tape) {
						state = look_for_changer;
						break;
					}
				}

				if (jp->drive->slot->tape == tape)
					return 0;

				break;

			case look_for_changer:
				jp->changer = st_changer_get_by_tape(tape);

				if (jp->changer) {
					jp->slot = jp->changer->ops->get_tape(jp->changer, tape);
					jp->drive = jp->slot->drive;
					state = jp->drive ? drive_is_free : look_for_free_drive;
				} else {
					state = alert_user;
				}
				break;

			case look_for_free_drive:
				jp->slot->lock->ops->lock(jp->slot->lock);

				if (jp->slot->tape != tape) {
					// it seem that someone has load tape before
					jp->slot->lock->ops->unlock(jp->slot->lock);

					state = look_for_changer;
					break;
				}

				// look for only unloaded free drive
				jp->drive = 0;
				for (i = 0; i < jp->changer->nb_drives && !jp->drive; i++) {
					jp->drive = jp->changer->drives + i;

					if (jp->drive->lock->ops->trylock(jp->drive->lock)) {
						jp->drive = 0;
						continue;
					}

					if (jp->drive->slot->tape) {
						jp->drive->lock->ops->unlock(jp->drive->lock);
						jp->drive = 0;
					}
				}

				// second attempt, look for free drive
				for (i = 0; i < jp->changer->nb_drives && !jp->drive; i++) {
					jp->drive = jp->changer->drives + i;

					if (jp->drive->lock->ops->trylock(jp->drive->lock))
						jp->drive = 0;
				}

				if (jp->drive) {
					if (jp->drive->slot->tape) {
						jp->drive->ops->eject(jp->drive);
						jp->changer->ops->unload(jp->changer, jp->drive);
					}

					jp->changer->lock->ops->lock(jp->changer->lock);
					jp->changer->ops->load(jp->changer, jp->slot, jp->drive);
					jp->changer->lock->ops->unlock(jp->changer->lock);
					jp->slot->lock->ops->unlock(jp->slot->lock);
					jp->drive->ops->reset(jp->drive);

					return 0;
				} else {
					jp->slot->lock->ops->unlock(jp->slot->lock);
					sleep(5);
					state = look_for_changer;
				}
				break;
		}
	}

	return 1;
}

void st_job_restoredb_new_job(struct st_database_connection * db __attribute__((unused)), struct st_job * job) {
	struct st_job_restoredb_private * self = malloc(sizeof(struct st_job_restoredb_private));
	self->buffer = 0;
	self->length = 0;

	self->nb_total_read = 0;
	self->total_size = 0;

	self->changer = 0;
	self->drive = 0;
	self->slot = 0;

	self->stop_request = 0;

	job->data = self;
	job->job_ops = &st_job_restoredb_ops;
}

int st_job_restoredb_run(struct st_job * job) {
	struct st_job_restoredb_private * self = job->data;

	job->db_ops->add_record(job, st_log_level_info, "Start restore database backup job (job id: %ld), num runs %ld", job->id, job->num_runs);

	if (!job->user->is_admin) {
		job->sched_status = st_job_status_error;
		job->db_ops->add_record(job, st_log_level_error, "User (%s) is not allowed to backup database", job->user->fullname);
		return 1;
	}

	// grace period
	sleep(15);

	if (self->stop_request) {
		job->db_ops->add_record(job, st_log_level_error, "Job: restore aborted (job id: %ld), num runs %ld", job->id, job->num_runs);
		return 0;
	}

	unsigned int i;
	struct st_backupdb * backup = job->backup;

	struct st_database * driver = st_db_get_default_db();
	struct st_stream_writer * writer = driver->ops->restore_db(driver);

	int status = 0;
	for (i = 0; i < backup->nb_volumes && !status; i++) {
		struct st_backupdb_volume * vol = backup->volumes + i;

		status = st_job_restoredb_load_tape(job, vol->tape);
		if (status) {
			job->db_ops->add_record(job, st_log_level_error, "Failed to load tape: %s", vol->tape->name);
			return status;
		}

		self->drive->ops->set_file_position(self->drive, vol->tape_position);

		struct st_stream_reader * tape_reader = self->drive->ops->get_reader(self->drive);

		self->length = tape_reader->ops->get_block_size(tape_reader);
		self->buffer = malloc(self->length);

		ssize_t nb_read;
		while ((nb_read = tape_reader->ops->read(tape_reader, self->buffer, self->length)) > 0) {
			writer->ops->write(writer, self->buffer, nb_read);

			self->nb_total_read += nb_read;
		}

		tape_reader->ops->close(tape_reader);
	}

	writer->ops->close(writer);
	writer->ops->free(writer);

	return status;
}

int st_job_restoredb_stop(struct st_job * job) {
	struct st_job_restoredb_private * self = job->data;
	if (self && !self->stop_request) {
		self->stop_request = 1;
		job->db_ops->add_record(job, st_log_level_warning, "Job: Stop requested");
		return 0;
	}
	return 1;
}

