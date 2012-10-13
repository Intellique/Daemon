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
*  Last modified: Sat, 13 Oct 2012 09:23:26 +0200                         *
\*************************************************************************/

// sscanf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// strcmp
#include <string.h>
// sleep
#include <unistd.h>

#include <libstone/checksum.h>
#include <libstone/database.h>
#include <libstone/io.h>
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
	.api_level   = {
		.checksum = STONE_CHECKSUM_API_LEVEL,
		.database = STONE_DATABASE_API_LEVEL,
		.job      = STONE_JOB_API_LEVEL,
	},
};


static short st_job_format_tape_check(struct st_job * job) {
	struct st_job_format_tape_private * self = job->data;

	st_job_add_record(self->connect, st_log_level_info, job, "Start format tape job (recover mode) (job name: %s), num runs %ld", job->name, job->num_runs);

	struct st_media * media = st_media_get_by_job(job, self->connect);
	if (media == NULL) {
		st_job_add_record(self->connect, st_log_level_error, job, "Media not found");
		return 0;
	}
	if (media->type == st_media_type_cleanning) {
		st_job_add_record(self->connect, st_log_level_error, job, "Try to format a cleaning media");
		return 0;
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

	while (!stop) {
		switch (state) {
			case alert_user:
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
					sleep(5);
					state = media_in_drive;
				}
				break;

			case drive_is_free:
				if (drive->lock->ops->try_lock(drive->lock)) {
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
				if (slot->lock->ops->try_lock(slot->lock)) {
					sleep(5);
					state = look_for_media;
				} else {
					has_lock_on_slot = 1;
					state = changer_has_free_drive;
				}
				break;
		}
	}

	if (drive->slot->media != NULL && drive->slot->media != media) {
		st_job_add_record(self->connect, st_log_level_info, job, "unloading media: %s from drive: { %s, %s, #%td }", drive->slot->media->name, drive->vendor, drive->model, drive - drive->changer->drives);
		int failed = slot->changer->ops->unload(slot->changer, drive);
		if (failed) {
			st_job_add_record(self->connect, st_log_level_error, job, "failed to unload media: %s from drive: { %s, %s, #%td }", drive->slot->media->name, drive->vendor, drive->model, drive - drive->changer->drives);

			if (has_lock_on_slot)
				slot->lock->ops->unlock(slot->lock);
			if (has_lock_on_drive)
				drive->lock->ops->unlock(drive->lock);

			return 0;
		}
	}

	if (drive->slot->media == NULL) {
		st_job_add_record(self->connect, st_log_level_info, job, "loading media: %s to drive: { %s, %s, #%td }", media->name, drive->vendor, drive->model, drive - drive->changer->drives);
		int failed = slot->changer->ops->load_slot(slot->changer, slot, drive);

		if (has_lock_on_slot) {
			slot->lock->ops->unlock(slot->lock);
			has_lock_on_slot = 0;
			slot = NULL;
		}

		if (failed) {
			st_job_add_record(self->connect, st_log_level_error, job, "failed to load media: %s from drive: { %s, %s, #%td }", media->name, drive->vendor, drive->model, drive - drive->changer->drives);

			if (has_lock_on_drive)
				drive->lock->ops->unlock(drive->lock);

			return 0;
		}
	}

	if (drive == NULL) {
		st_job_add_record(self->connect, st_log_level_error, job, "Internal error, drive is not supposed to be NULL");

		job->repetition = 0;
		job->done = 0;
		job->db_status = job->sched_status = st_job_status_error;

		return 1;
	}

	char buffer[512];
	struct st_stream_reader * reader = drive->ops->get_raw_reader(drive, 0);
	ssize_t nb_read = reader->ops->read(reader, buffer, 512);
	reader->ops->close(reader);
	reader->ops->free(reader);

	if (has_lock_on_drive)
		drive->lock->ops->unlock(drive->lock);

	if (nb_read <= 0) {
		st_job_add_record(self->connect, st_log_level_info, job, "no header found, re-enable job");

		job->repetition = 1;
		job->done = 0;
		job->db_status = job->sched_status = st_job_status_idle;

		return 0;
	}

	// M | STone (v0.1)
	// M | Tape format: version=1
	// O | Label: A0000002
	// M | Tape id: uuid=f680dd48-dd3e-4715-8ddc-a90d3e708914
	// M | Pool: name=Foo, uuid=07117f1a-2b13-11e1-8bcb-80ee73001df6
	// M | Block size: 32768
	// M | Checksum: crc32=1eb6931d
	char stone_version[65];
	int tape_format_version = 0;
	int nb_parsed = 0;
	if (sscanf(buffer, "STone (%64[^)])\nTape format: version=%d\n%n", stone_version, &tape_format_version, &nb_parsed) == 2) {
		char uuid[37];
		char name[65];
		char pool_id[37];
		char pool_name[65];
		ssize_t block_size;
		char checksum_name[12];
		char checksum_value[64];

		int nb_parsed2 = 0;
		int ok = 1;

		if (sscanf(buffer + nb_parsed, "Label: %36s\n%n", name, &nb_parsed2) == 1)
			nb_parsed += nb_parsed2;

		if (ok && sscanf(buffer + nb_parsed, "Tape id: uuid=%37s\n%n", uuid, &nb_parsed2) == 1)
			nb_parsed += nb_parsed2;
		else
			ok = 0;

		if (ok && sscanf(buffer + nb_parsed, "Pool: name=%65[^,], uuid=%36s\n%n", pool_name, pool_id, &nb_parsed2) == 2)
			nb_parsed += nb_parsed2;
		else
			ok = 0;

		if (ok && sscanf(buffer + nb_parsed, "Block size: %zd\n%n", &block_size, &nb_parsed2) == 1)
			nb_parsed += nb_parsed2;
		else
			ok = 0;

		if (ok)
			ok = sscanf(buffer + nb_parsed, "Checksum: %11[^=]=%64s\n", checksum_name, checksum_value) == 2;

		if (ok) {
			char * digest = st_checksum_compute(checksum_name, buffer, nb_parsed);
			ok = digest != NULL && !strcmp(checksum_value, digest);
			free(digest);
		}

		// checking parsed value
		if (ok && strcmp(uuid, media->uuid))
			ok = 0;

		if (ok && strcmp(pool_id, pool->uuid))
			ok = 0;

		if (ok) {
			st_job_add_record(self->connect, st_log_level_info, job, "header found, disable job");

			job->repetition = 0;
			job->done = 0;
			job->db_status = job->sched_status = st_job_status_idle;

			return 1;
		} else {
			st_job_add_record(self->connect, st_log_level_info, job, "wrong header found, re-enable job");

			job->repetition = 1;
			job->done = 0;
			job->db_status = job->sched_status = st_job_status_idle;

			return 1;
		}
	} else {
		st_job_add_record(self->connect, st_log_level_info, job, "media contains data but no header found, re-enable job");

		job->repetition = 1;
		job->done = 0;
		job->db_status = job->sched_status = st_job_status_idle;

		return 1;
	}
}

static void st_job_format_tape_free(struct st_job * job) {
	struct st_job_format_tape_private * self = job->data;
	if (self != NULL) {
		self->connect->ops->close(self->connect);
		self->connect->ops->free(self->connect);
	}
	free(self);

	job->data = NULL;
}

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
				if (drive->lock->ops->try_lock(drive->lock)) {
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
				if (slot->lock->ops->try_lock(slot->lock)) {
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

	if (job->db_status != st_job_status_stopped)
		job->done = 0.25;

	if (job->db_status != st_job_status_stopped && drive->slot->media != NULL && drive->slot->media != media) {
		st_job_add_record(self->connect, st_log_level_info, job, "unloading media: %s from drive: { %s, %s, #%td }", drive->slot->media->name, drive->vendor, drive->model, drive - drive->changer->drives);
		int failed = slot->changer->ops->unload(slot->changer, drive);
		if (failed) {
			st_job_add_record(self->connect, st_log_level_error, job, "failed to unload media: %s from drive: { %s, %s, #%td }", drive->slot->media->name, drive->vendor, drive->model, drive - drive->changer->drives);

			if (has_lock_on_slot)
				slot->lock->ops->unlock(slot->lock);
			if (has_lock_on_drive)
				drive->lock->ops->unlock(drive->lock);

			return 2;
		}
	}

	if (job->db_status != st_job_status_stopped)
		job->done = 0.5;

	if (job->db_status != st_job_status_stopped && drive->slot->media == NULL) {
		st_job_add_record(self->connect, st_log_level_info, job, "loading media: %s to drive: { %s, %s, #%td }", media->name, drive->vendor, drive->model, drive - drive->changer->drives);
		int failed = slot->changer->ops->load_slot(slot->changer, slot, drive);

		if (has_lock_on_slot) {
			slot->lock->ops->unlock(slot->lock);
			has_lock_on_slot = 0;
			slot = NULL;
		}

		if (failed) {
			st_job_add_record(self->connect, st_log_level_error, job, "failed to load media: %s from drive: { %s, %s, #%td }", media->name, drive->vendor, drive->model, drive - drive->changer->drives);

			if (has_lock_on_drive)
				drive->lock->ops->unlock(drive->lock);

			return 3;
		}
	}

	if (job->db_status != st_job_status_stopped)
		job->done = 0.75;

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
		else {
			job->done = 1;
			st_job_add_record(self->connect, st_log_level_info, job, "Job: format tape finished with code = OK, num runs %ld", job->num_runs);
		}
	} else {
		st_job_add_record(self->connect, st_log_level_warning, job, "Job: format tape aborted by user before formatting media");
	}

	if (drive != NULL && has_lock_on_drive)
		drive->lock->ops->unlock(drive->lock);

	return 0;
}

