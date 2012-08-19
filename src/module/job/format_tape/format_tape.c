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
*  Last modified: Sun, 19 Aug 2012 13:44:27 +0200                         *
\*************************************************************************/

// sscanf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// strcmp
#include <string.h>
// sleep
#include <unistd.h>

#include <libstone/database.h>
#include <libstone/library/drive.h>
#include <libstone/library/media.h>
#include <libstone/library/ressource.h>
#include <libstone/library/slot.h>
#include <libstone/log.h>
#include <libstone/job.h>
#include <libstone/user.h>
#include <libstone/util/hashtable.h>
#include <stoned/library/changer.h>


struct st_job_format_tape_private {
	struct st_database_connection * connect;
};

static short st_job_format_tape_check(struct st_job * job);
static void st_job_format_tape_free(struct st_job * job);
static void st_job_format_tape_init(void) __attribute__((constructor));
static void st_job_format_tape_new_job(struct st_job * job, struct st_database_connection * db);
static int st_job_format_tape_run(struct st_job * job);

static struct st_job_ops st_job_format_tape_ops = {
	.check = st_job_format_tape_check,
	.free  = st_job_format_tape_free,
	.run   = st_job_format_tape_run,
};

static struct st_job_driver st_job_format_tape_driver = {
	.name        = "format-tape",
	.new_job     = st_job_format_tape_new_job,
	.cookie      = NULL,
	.api_version = STONE_JOB_API_LEVEL,
};


static short st_job_format_tape_check(struct st_job * job) {
	return 0;
}

static void st_job_format_tape_free(struct st_job * job __attribute__((unused))) {}

static void st_job_format_tape_init(void) {
	st_job_register_driver(&st_job_format_tape_driver);
}

static void st_job_format_tape_new_job(struct st_job * job, struct st_database_connection * db) {
	struct st_job_format_tape_private * self = malloc(sizeof(struct st_job_format_tape_private));
	self->connect = db->config->ops->connect(db->config);

	job->data = self;
	job->ops = &st_job_format_tape_ops;
}

int st_job_format_tape_run(struct st_job * job) {
	struct st_job_format_tape_private * self = job->data;

	st_job_add_record(self->connect, st_log_level_info, job, "Start format tape job (job name: %s), num runs %ld", job->name, job->num_runs);

	struct st_media * media = st_media_get_by_job(job, self->connect);
	if (media == NULL) {
		st_job_add_record(self->connect, st_log_level_error, job, "Media not found");
		return 1;
	}
	if (media->type == st_media_type_cleanning) {
		st_job_add_record(self->connect, st_log_level_error, job, "Try to format a cleaning media");
		return 1;
	}
	if (media->type == st_media_type_readonly && media->nb_volumes > 0) {
		st_job_add_record(self->connect, st_log_level_error, job, "Try to format a worm media with data");
		return 1;
	}

	struct st_pool * pool = st_pool_get_by_job(job, self->connect);
	if (pool == NULL) {
		pool = job->user->pool;
		st_job_add_record(self->connect, st_log_level_warning, job, "using default pool '%s' of user '%s'", pool->name, job->user->login);
	}

	enum {
		alert_user,
		changer_has_free_drive,
		drive_is_free,
		look_for_media,
		media_in_drive,
		slot_is_free,
	} state = look_for_media;
	short stop = 0, has_alert_user = 0, has_lock_on_drive = 0, has_lock_on_slot = 0;
	struct st_drive * drive = NULL;
	struct st_slot * slot = NULL;

	while (!stop && job->db_status != st_job_status_stopped) {
		switch (state) {
			case alert_user:
				job->sched_status = st_job_status_waiting;
				if (!has_alert_user)
					st_job_add_record(self->connect, st_log_level_warning, job, "Media not found (named: %s)", media->name);
				has_alert_user = 1;
				sleep(15);
				state = look_for_media;
				break;

			case changer_has_free_drive:
				// if drive is not NULL, we own also a lock on it
				drive = slot->changer->ops->find_free_drive(slot->changer);
				if (drive != NULL) {
					has_lock_on_drive = 1;
					stop = 1;
				} else {
					job->sched_status = st_job_status_waiting;
					sleep(5);
					state = media_in_drive;
				}
				break;

			case drive_is_free:
				if (drive->lock->ops->trylock(drive->lock)) {
					job->sched_status = st_job_status_waiting;
					sleep(5);
					state = media_in_drive;
				} else if (slot->media == media) {
					has_lock_on_drive = 1;
					stop = 1;
				} else {
					// media has been vannished
					drive->lock->ops->unlock(drive->lock);
					drive = NULL;
					slot = NULL;
					state = look_for_media;
				}
				break;

			case look_for_media:
				// lock of slot can be owned by another job
				slot = st_changer_find_slot_by_media(media);
				state = slot != NULL ? media_in_drive : alert_user;
				break;

			case media_in_drive:
				drive = slot->drive;
				state = drive != NULL ? drive_is_free : slot_is_free;
				break;

			case slot_is_free:
				if (slot->lock->ops->trylock(slot->lock)) {
					job->sched_status = st_job_status_waiting;
					sleep(5);
					state = look_for_media;
				} else {
					has_lock_on_slot = 1;
					state = changer_has_free_drive;
				}
				break;
		}
	}

	job->sched_status = st_job_status_running;

	if (job->db_status != st_job_status_stopped && drive->slot->media != NULL && drive->slot->media != media) {
		st_job_add_record(self->connect, st_log_level_error, job, "unloading media: %s from drive: %s %s #%td", drive->slot->media->name, drive->vendor, drive->model, drive - drive->changer->drives);
		int failed = slot->changer->ops->unload(slot->changer, drive);
		if (failed)
			st_job_add_record(self->connect, st_log_level_error, job, "failed to unload media: %s from drive: %s %s #%td", drive->slot->media->name, drive->vendor, drive->model, drive - drive->changer->drives);

		return 2;
	}

	if (job->db_status != st_job_status_stopped && drive->slot->media == NULL) {
		st_job_add_record(self->connect, st_log_level_error, job, "loading media: %s to drive: %s %s #%td", media->name, drive->vendor, drive->model, drive - drive->changer->drives);
		int failed = slot->changer->ops->load_slot(slot->changer, slot, drive);
		if (failed)
			st_job_add_record(self->connect, st_log_level_error, job, "failed to load media: %s from drive: %s %s #%td", media->name, drive->vendor, drive->model, drive - drive->changer->drives);

		if (has_lock_on_slot) {
			slot->lock->ops->unlock(slot->lock);
			has_lock_on_slot = 0;
			slot = NULL;
		}

		return 3;
	}

	if (job->db_status != st_job_status_stopped)
		job->done = 0.5;

	// check blocksize
	enum update_blocksize {
		blocksize_nop,
		blocksize_set,
		blocksize_set_default,
	} do_update_block_size = blocksize_nop;
	ssize_t block_size = 0;
	char * blocksize = st_hashtable_value(job->option, "blocksize");
	if (blocksize != NULL) {
		if (sscanf(blocksize, "%zd", &block_size) == 1) {
			if (block_size > 0) {
				do_update_block_size = blocksize_set;

				// round block size to power of two
				ssize_t block_size_tmp = block_size;
				short p;
				for (p = 0; block_size > 1; p++, block_size >>= 1);
				block_size <<= p;

				if (block_size != block_size_tmp)
					st_job_add_record(self->connect, st_log_level_info, job, "Using block size %zd instead of %zd", block_size, block_size_tmp);
			} else {
				// ignore block size because bad value
				st_job_add_record(self->connect, st_log_level_info, job, "Wrong value of block size: %zd", block_size);
			}
		}
		else if (!strcmp(blocksize, "default"))
			do_update_block_size = blocksize_set_default;
	}

	// write header
	switch (do_update_block_size) {
		case blocksize_nop:
			st_job_add_record(self->connect, st_log_level_info, job, "Formatting new media");
			break;

		case blocksize_set:
			if (media->block_size != block_size) {
				st_job_add_record(self->connect, st_log_level_info, job, "Formatting new media (using block size: %zd bytes, previous value: %zd bytes)", block_size, media->block_size);
				media->block_size = block_size;
			} else
				st_job_add_record(self->connect, st_log_level_info, job, "Formatting new media (using block size: %zd bytes)", block_size);
			break;

		case blocksize_set_default:
			st_job_add_record(self->connect, st_log_level_info, job, "Formatting new media (using default block size: %zd bytes)", media->format->block_size);
			media->block_size = media->format->block_size;
			break;
	}

	int status = 0;
	if (job->db_status != st_job_status_stopped) {
		st_job_add_record(self->connect, st_log_level_info, job, "Formatting media in progress");
		status = st_media_write_header(drive, pool);

		if (status)
			st_job_add_record(self->connect, st_log_level_info, job, "Job: format tape finished with code = %d, num runs %ld", status, job->num_runs);
		else
			st_job_add_record(self->connect, st_log_level_info, job, "Job: format tape finished with code = OK, num runs %ld", job->num_runs);
	} else {
		st_job_add_record(self->connect, st_log_level_warning, job, "Job: format tape aborted by user before formatting media");
	}

	if (drive != NULL && has_lock_on_drive)
		drive->lock->ops->unlock(drive->lock);

	return 0;
}

