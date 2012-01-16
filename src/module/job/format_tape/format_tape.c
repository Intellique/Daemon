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
*  Copyright (C) 2011, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Mon, 16 Jan 2012 17:34:30 +0100                         *
\*************************************************************************/

// free, malloc
#include <stdlib.h>
// sleep
#include <unistd.h>

#include <stone/job.h>
#include <stone/library/changer.h>
#include <stone/library/drive.h>
#include <stone/library/ressource.h>
#include <stone/library/tape.h>
#include <stone/log.h>
#include <stone/user.h>

static void st_job_format_tape_free(struct st_job * job);
static void st_job_format_tape_init(void) __attribute__((constructor));
static void st_job_format_tape_new_job(struct st_database_connection * db, struct st_job * job);
static int st_job_format_tape_run(struct st_job * job);
static int st_job_format_tape_stop(struct st_job * job);

struct st_job_ops st_job_format_tape_ops = {
	.free = st_job_format_tape_free,
	.run  = st_job_format_tape_run,
	.stop = st_job_format_tape_stop,
};


struct st_job_driver st_job_format_tape_driver = {
	.name        = "format-tape",
	.new_job     = st_job_format_tape_new_job,
	.cookie      = 0,
	.api_version = STONE_JOB_APIVERSION,
};

void st_job_format_tape_free(struct st_job * job __attribute__((unused))) {}

void st_job_format_tape_init() {
	st_job_register_driver(&st_job_format_tape_driver);
}

void st_job_format_tape_new_job(struct st_database_connection * db __attribute__((unused)), struct st_job * job) {
	job->data = 0;
	job->job_ops = &st_job_format_tape_ops;
}

int st_job_format_tape_run(struct st_job * job) {
	job->db_ops->add_record(job, "Start format tape job (job id: %ld), num runs %ld", job->id, job->num_runs);

	if (!job->pool)
		job->pool = job->user->pool;

	enum {
		alert_user,
		look_for_changer,
		look_for_free_drive,
	} state = look_for_changer;

	struct st_changer * changer = 0;
	struct st_drive * drive = 0;
	struct st_slot * slot = 0;
	short stop = 0, has_alert_user = 0;
	unsigned int i;

	while (!stop) {
		switch (state) {
			case alert_user:
				job->sched_status = st_job_status_pause;
				job->db_ops->update_status(job);

				if (!has_alert_user) {
					job->db_ops->add_record(job, "Tape not found (named: %s)", job->tape->name);
					st_log_write_all(st_log_level_error, st_log_type_user_message, "Job: format tape (id:%ld) request you to put a tape (named: %s) in your changer or standalone drive", job->id, job->tape->name);
				}
				has_alert_user = 1;
				sleep(15);

				job->sched_status = st_job_status_running;
				job->db_ops->update_status(job);

				state = look_for_changer;
				break;

			case look_for_changer:
				changer = st_changer_get_by_tape(job->tape);
				drive = 0;
				slot = 0;

				if (changer) {
					for (i = 0; i < changer->nb_slots && !slot; i++) {
						slot = changer->slots + i;

						if (slot->tape != job->tape) {
							slot = 0;
							continue;
						}

						drive = slot->drive;
						if (drive && !drive->lock->ops->trylock(drive->lock)) {
							stop = 1;
							break;
						} else if (drive) {
							sleep(5);
							slot = 0;
							i = -1;
						} else if (!slot->lock->ops->trylock(slot->lock)) {
							state = look_for_free_drive;
							break;
						} else {
							sleep(5);
							slot = 0;
							i = -1;
						}
					}
				} else {
					state = alert_user;
				}
				break;

			case look_for_free_drive:
				drive = 0;
				for (i = 0; i < changer->nb_drives && !drive; i++) {
					drive = changer->drives + i;

					if (drive->lock->ops->trylock(drive->lock))
						drive = 0;
				}

				if (drive) {
					stop = 1;
				} else {
					slot->lock->ops->unlock(slot->lock);
					sleep(5);
					state = look_for_changer;
				}
				break;
		}
	}

	if ((drive->slot->tape && drive->slot->tape != job->tape) || !drive->slot->tape)
		changer->lock->ops->lock(changer->lock);

	if (drive->slot->tape && drive->slot->tape != job->tape) {
		struct st_slot * slot_to = 0;

		// look for the tape was stored
		for (i = changer->nb_drives; i < changer->nb_slots; i++) {
			slot_to = changer->slots + i;

			if (!slot_to->tape && slot_to->address != drive->slot->src_address && !slot_to->lock->ops->trylock(slot_to->lock))
				break;

			slot_to = 0;
		}

		// if not found, look for free slot
		for (i = changer->nb_drives; i < changer->nb_slots && !slot_to; i++) {
			slot_to = changer->slots + i;

			if (!slot_to->tape && !slot_to->lock->ops->trylock(slot_to->lock))
				break;

			slot_to = 0;
		}

		if (slot_to) {
			drive->ops->eject(drive);

			job->db_ops->add_record(job, "Unloading tape from drive #%td to slot #%td", drive - changer->drives, slot_to - changer->slots);
			changer->ops->unload(changer, drive, slot_to);
		} else {
			job->sched_status = st_job_status_error;
			job->db_ops->add_record(job, "Fatal error: There is no place for unloading tape (%s)", drive->slot->tape->name);

			// release lock
			changer->lock->ops->unlock(changer->lock);
			drive->lock->ops->unlock(drive->lock);

			return 1;
		}
	}

	if (!drive->slot->tape) {
		job->db_ops->add_record(job, "Loading tape from slot #%td to drive #%td", slot - changer->slots, drive - changer->drives);
		changer->ops->load(changer, slot, drive);
		slot->lock->ops->unlock(slot->lock);
		changer->lock->ops->unlock(changer->lock);

		drive->ops->reset(drive);
	}

	job->db_ops->add_record(job, "Got changer: %s %s", changer->vendor, changer->model);
	job->db_ops->add_record(job, "Got drive: %s %s", drive->vendor, drive->model);

	job->done = 0.5;
	job->db_ops->update_status(job);

	// write header
	job->db_ops->add_record(job, "Formatting new tape");
	int status = st_tape_write_header(drive, job->pool);
	job->done = 1;

	drive->lock->ops->unlock(drive->lock);

	if (status)
		job->sched_status = st_job_status_error;

	sleep(1);
	job->db_ops->update_status(job);

	if (status)
		job->db_ops->add_record(job, "Job: format tape finished with code = %d (job id: %ld), num runs %ld", status, job->id, job->num_runs);
	else
		job->db_ops->add_record(job, "Job: format tape finished with code = OK (job id: %ld), num runs %ld", job->id, job->num_runs);

	return 0;
}

int st_job_format_tape_stop(struct st_job * job __attribute__((unused))) {
	return 0;
}

