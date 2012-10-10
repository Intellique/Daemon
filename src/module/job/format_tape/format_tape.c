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
*  Last modified: Wed, 10 Oct 2012 16:50:38 +0200                         *
\*************************************************************************/

// sscanf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// strcmp
#include <string.h>
// sleep
#include <unistd.h>

#include <stone/job.h>
#include <stone/library/changer.h>
#include <stone/library/drive.h>
#include <stone/library/ressource.h>
#include <stone/library/tape.h>
#include <stone/log.h>
#include <stone/util/hashtable.h>
#include <stone/user.h>

struct st_job_format_tape_private {
	int stop_request;
};

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
	struct st_job_format_tape_private * self = malloc(sizeof(struct st_job_format_tape_private));
	self->stop_request = 0;

	job->data = self;
	job->job_ops = &st_job_format_tape_ops;
}

int st_job_format_tape_run(struct st_job * job) {
	struct st_job_format_tape_private * self = job->data;

	job->db_ops->add_record(job, st_log_level_info, "Start format tape job (job id: %ld), num runs %ld", job->id, job->num_runs);

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
	short has_changer_lock = 0, has_drive_lock = 0;
	unsigned int i;

	while (!stop) {
		switch (state) {
			case alert_user:
				job->sched_status = st_job_status_pause;
				sleep(1);
				job->db_ops->update_status(job);

				if (!has_alert_user) {
					job->db_ops->add_record(job, st_log_level_warning, "Tape not found (named: %s)", job->tape->name);
					st_log_write_all(st_log_level_warning, st_log_type_user_message, "Job: format tape (id:%ld) request you to put a tape (named: %s) in your changer or standalone drive", job->id, job->tape->name);
				}
				has_alert_user = 1;
				sleep(15);

				job->sched_status = st_job_status_running;
				job->db_ops->update_status(job);

				state = look_for_changer;

				if (self->stop_request)
					stop = 1;
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
							has_drive_lock = 1;
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
					has_drive_lock = 1;
				} else {
					slot->lock->ops->unlock(slot->lock);
					sleep(5);
					state = look_for_changer;
				}
				break;
		}
	}

	if ((drive->slot->tape && drive->slot->tape != job->tape) || !drive->slot->tape) {
		changer->lock->ops->lock(changer->lock);
		has_changer_lock = 1;
	}

	if (!self->stop_request && drive->slot->tape && drive->slot->tape != job->tape) {
		drive->ops->eject(drive);

		if (changer->ops->unload(changer, drive)) {
			job->sched_status = st_job_status_error;
			job->db_ops->add_record(job, st_log_level_error, "Fatal error: There is no place for unloading tape (%s)", drive->slot->tape->name);

			// release lock
			changer->lock->ops->unlock(changer->lock);
			drive->lock->ops->unlock(drive->lock);
			has_drive_lock = 0;
			has_changer_lock = 0;

			return 1;
		}
	}

	if (!self->stop_request && !drive->slot->tape) {
		job->db_ops->add_record(job, st_log_level_info, "Loading tape from slot #%td to drive #%td", slot - changer->slots, drive - changer->drives);

		if (changer->ops->load(changer, slot, drive)) {
			job->sched_status = st_job_status_error;
			job->db_ops->add_record(job, st_log_level_error, "Fatal error: Failed to load tape");

			changer->lock->ops->unlock(changer->lock);
			drive->lock->ops->unlock(drive->lock);
			has_drive_lock = 0;
			has_changer_lock = 0;

			return 1;
		}

		slot->lock->ops->unlock(slot->lock);
		changer->lock->ops->unlock(changer->lock);
		has_changer_lock = 0;

		drive->ops->reset(drive, false, false);
	}

	if (!self->stop_request) {
		job->db_ops->add_record(job, st_log_level_info, "Got changer: %s %s", changer->vendor, changer->model);
		job->db_ops->add_record(job, st_log_level_info, "Got drive: %s %s", drive->vendor, drive->model);

		job->done = 0.5;
		job->db_ops->update_status(job);
	}

	if (!self->stop_request) {
		// check blocksize
		enum update_blocksize {
			BLOCKSIZE_NOP,
			BLOCKSIZE_SET,
			BLOCKSIZE_SET_DEFAULT,
		} do_update_block_size = BLOCKSIZE_NOP;
		ssize_t block_size = 0;
		struct st_tape * tape = job->tape;

		char * blocksize = st_hashtable_value(job->job_option, "blocksize");
		if (blocksize) {
			if (sscanf(blocksize, "%zd", &block_size) == 1) {
				if (block_size > 0) {
					do_update_block_size = BLOCKSIZE_SET;

					// round block size to power of two
					ssize_t block_size_tmp = block_size;
					short p;
					for (p = 0; block_size > 1; p++, block_size >>= 1);
					block_size <<= p;

					if (block_size != block_size_tmp)
						job->db_ops->add_record(job, st_log_level_warning, "Using block size %zd instead of %zd", block_size, block_size_tmp);
				} else {
					// ignore block size because bad value
					job->db_ops->add_record(job, st_log_level_warning, "Wrong value of block size: %zd", block_size);
				}
			}
			else if (!strcmp(blocksize, "default"))
				do_update_block_size = BLOCKSIZE_SET_DEFAULT;
		}

		// write header
		switch (do_update_block_size) {
			case BLOCKSIZE_NOP:
				job->db_ops->add_record(job, st_log_level_info, "Formatting new tape");
				break;

			case BLOCKSIZE_SET:
				if (tape->block_size != block_size) {
					job->db_ops->add_record(job, st_log_level_info, "Formatting new tape (using block size: %zd bytes, previous value: %zd bytes)", block_size, tape->block_size);
					tape->block_size = block_size;
				} else
					job->db_ops->add_record(job, st_log_level_info, "Formatting new tape (using block size: %zd bytes)", block_size);
				break;

			case BLOCKSIZE_SET_DEFAULT:
				job->db_ops->add_record(job, st_log_level_info, "Formatting new tape (using default block size: %zd bytes)", tape->format->block_size);
				tape->block_size = tape->format->block_size;
				break;
		}
	}

	int status = 0;
	if (!self->stop_request)
		status = st_tape_write_header(drive, job->pool);

	if (changer && has_changer_lock)
		changer->lock->ops->unlock(changer->lock);

	if (drive && has_drive_lock)
		drive->lock->ops->unlock(drive->lock);

	if (status)
		job->sched_status = st_job_status_error;

	if (!self->stop_request) {
		job->done = 1;
		sleep(1);
	}

	job->db_ops->update_status(job);

	if (self->stop_request)
		job->db_ops->add_record(job, st_log_level_error, "Job: format tape aborted (job id: %ld), num runs %ld", job->id, job->num_runs);
	else if (status)
		job->db_ops->add_record(job, st_log_level_error, "Job: format tape finished with code = %d (job id: %ld), num runs %ld", status, job->id, job->num_runs);
	else
		job->db_ops->add_record(job, st_log_level_info, "Job: format tape finished with code = OK (job id: %ld), num runs %ld", job->id, job->num_runs);

	free(self);

	return 0;
}

int st_job_format_tape_stop(struct st_job * job) {
	struct st_job_format_tape_private * self = job->data;
	if (self && !self->stop_request) {
		self->stop_request = 1;
		job->db_ops->add_record(job, st_log_level_warning, "Job: Stop requested");
		return 0;
	}
	return 1;
}

