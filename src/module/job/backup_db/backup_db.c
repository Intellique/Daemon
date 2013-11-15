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
*  Last modified: Fri, 15 Nov 2013 16:55:06 +0100                            *
\****************************************************************************/

#define _GNU_SOURCE
// json_*
#include <jansson.h>
// asprintf
#include <stdio.h>
// malloc, realloc
#include <stdlib.h>
// memmove
#include <string.h>
// sleep
#include <unistd.h>

#include <libstone/backup.h>
#include <libstone/database.h>
#include <libstone/io.h>
#include <libstone/job.h>
#include <libstone/library/drive.h>
#include <libstone/library/media.h>
#include <libstone/library/ressource.h>
#include <libstone/log.h>
#include <libstone/script.h>
#include <libstone/util/file.h>
#include <libstone/user.h>
#include <stoned/library/changer.h>
#include <stoned/library/slot.h>

#include <libjob-backup-db.chcksum>

struct st_job_backup_private {
	struct st_database_connection * connect;

	struct st_drive * drive;
	struct st_pool * pool;
	ssize_t backup_size;
};

static bool st_job_backup_db_check(struct st_job * job);
static void st_job_backup_db_free(struct st_job * job);
static void st_job_backup_db_init(void) __attribute__((constructor));
static void st_job_backup_db_new_job(struct st_job * job, struct st_database_connection * db);
static bool st_job_backup_db_pre_run_script(struct st_job * job);
static int st_job_backup_db_run(struct st_job * job);
static bool st_job_backup_db_select_media(struct st_job * job, struct st_job_backup_private * self);

struct st_job_ops st_job_backup_db_ops = {
	.check          = st_job_backup_db_check,
	.free           = st_job_backup_db_free,
	.pre_run_script = st_job_backup_db_pre_run_script,
	.run            = st_job_backup_db_run,
};

struct st_job_driver st_job_backup_db_driver = {
	.name        = "backup-db",
	.new_job     = st_job_backup_db_new_job,
	.cookie      = NULL,
	.api_level   = {
		.database = STONE_DATABASE_API_LEVEL,
		.job      = STONE_JOB_API_LEVEL,
	},
	.src_checksum   = STONE_JOB_BACKUPDB_SRCSUM,
};


static bool st_job_backup_db_check(struct st_job * job __attribute__((unused))) {
	return true;
}

static void st_job_backup_db_free(struct st_job * job __attribute__((unused))) {}

static void st_job_backup_db_init() {
	st_job_register_driver(&st_job_backup_db_driver);
}

static void st_job_backup_db_new_job(struct st_job * job, struct st_database_connection * db) {
	struct st_job_backup_private * self = malloc(sizeof(struct st_job_backup_private));
	self->connect = db->config->ops->connect(db->config);
	self->pool = st_pool_get_by_uuid("d9f976d4-e087-4d0a-ab79-96267f6613f0"); // pool Stone_Db_Backup

	job->ops = &st_job_backup_db_ops;
	job->data = self;
}

static bool st_job_backup_db_pre_run_script(struct st_job * job) {
	struct st_job_backup_private * self = job->data;

	json_t * data = json_object();

	json_t * returned_data = st_script_run(self->connect, job->driver->name, st_script_type_pre, self->pool, data);
	bool sr = st_io_json_should_run(returned_data);

	json_decref(returned_data);
	json_decref(data);

	return sr;
}

static int st_job_backup_db_run(struct st_job * job) {
	struct st_job_backup_private * self = job->data;
	self->drive = NULL;
	struct st_backup * backup = st_backup_new();

	st_job_add_record(self->connect, st_log_level_info, job, "Start backup job (job name: %s), num runs %ld", job->name, job->num_runs);

	if (!job->user->is_admin) {
		job->sched_status = st_job_status_error;
		st_job_add_record(self->connect, st_log_level_error, job, "User (%s) is not allowed to backup database", job->user->fullname);
		return 1;
	}

	struct st_database_config * db_config = self->connect->config;
	struct st_stream_reader * db_reader = db_config->ops->backup_db(db_config);

	char * temp_filename;
	asprintf(&temp_filename, "%s/backup_db_XXXXXX", job->user->home_directory);

	job->done = 0.01;

	struct st_stream_writer * temp_io_writer = st_io_temp_writer(temp_filename, 0);
	st_util_file_rm(temp_filename);
	free(temp_filename);

	job->done = 0.02;

	char buffer[4096];
	ssize_t nb_read;
	while (nb_read = db_reader->ops->read(db_reader, buffer, 4096), nb_read > 0) {
		temp_io_writer->ops->write(temp_io_writer, buffer, nb_read);
	}

	db_reader->ops->close(db_reader);
	self->backup_size = db_reader->ops->position(db_reader);
	db_reader->ops->free(db_reader);
	db_reader = NULL;

	job->done = 0.03;

	if (!st_job_backup_db_select_media(job, self)) {
		db_reader->ops->close(db_reader);
		db_reader->ops->free(db_reader);

		return 1;
	}

	job->done = 0.04;

	struct st_stream_reader * temp_io_reader = temp_io_writer->ops->reopen(temp_io_writer);

	temp_io_writer->ops->free(temp_io_writer);
	temp_io_writer = NULL;

	struct st_stream_writer * media_writer = self->drive->ops->get_raw_writer(self->drive, true);
	st_backup_add_volume(backup, self->drive->slot->media, self->drive->ops->get_position(self->drive));

	ssize_t total_writen = 0;
	while (nb_read = temp_io_reader->ops->read(temp_io_reader, buffer, 4096), nb_read > 0) {
		media_writer->ops->write(media_writer, buffer, nb_read);

		total_writen += nb_read;
		float done = total_writen;
		done /= self->backup_size;
		done *= 0.95;

		job->done = done + 0.04;
	}

	temp_io_reader->ops->close(temp_io_reader);
	temp_io_reader->ops->free(temp_io_reader);

	media_writer->ops->close(media_writer);
	media_writer->ops->free(media_writer);

	self->drive->lock->ops->unlock(self->drive->lock);

	job->done = 0.99;

	self->connect->ops->add_backup(self->connect, backup);

	st_backup_free(backup);

	self->connect->ops->close(self->connect);
	self->connect->ops->free(self->connect);
	self->connect = NULL;

	job->done = 1;

	return 0;
}

static bool st_job_backup_db_select_media(struct st_job * job, struct st_job_backup_private * self) {
	bool has_alerted_user = false;
	bool ok = false;
	bool stop = false;
	enum state {
		check_offline_free_size_left,
		check_online_free_size_left,
		find_free_drive,
		has_wrong_media,
		has_media,
		is_media_formatted,
		is_pool_growable1,
		is_pool_growable2,
		media_is_read_only,
	} state = check_online_free_size_left;

	struct st_changer * changer = NULL;
	struct st_drive * drive = NULL;
	struct st_slot * slot = NULL;

	ssize_t total_size = 0;

	while (!stop) {
		switch (state) {
			case check_offline_free_size_left:
				total_size += self->connect->ops->get_available_size_of_offline_media_from_pool(self->connect, self->pool);

				if (self->backup_size > total_size) {
					// alert user
					if (!has_alerted_user)
						st_job_add_record(self->connect, st_log_level_warning, job, "Please, insert media which is a part of pool named %s", self->pool->name);
					has_alerted_user = true;

					job->sched_status = st_job_status_pause;
					sleep(10);
					job->sched_status = st_job_status_running;

					if (job->db_status == st_job_status_stopped)
						return false;

					state = check_online_free_size_left;
				} else {
					state = is_pool_growable2;
				}

				break;

			case check_online_free_size_left:
				total_size = st_changer_get_online_size(self->pool);

				if (self->backup_size < total_size) {
					struct st_slot_iterator * iter = st_slot_iterator_by_pool(self->pool);

					while (iter->ops->has_next(iter)) {
						slot = iter->ops->next(iter);

						struct st_media * media = slot->media;
						if (self->backup_size > media->free_block * media->block_size) {
							if (self->drive != slot->drive && slot->drive != NULL)
								slot->drive->lock->ops->unlock(slot->drive->lock);
							else if (slot->drive == NULL)
								slot->lock->ops->unlock(slot->lock);
							slot = NULL;
							continue;
						}

						if (self->backup_size < media->free_block * media->block_size)
							break;

						if (10 * media->free_block > media->total_block)
							break;

						if (self->drive != slot->drive && slot->drive != NULL)
							slot->drive->lock->ops->unlock(slot->drive->lock);
						else if (slot->drive == NULL)
							slot->lock->ops->unlock(slot->lock);
						slot = NULL;
					}

					iter->ops->free(iter);

					if (slot != NULL) {
						changer = slot->changer;
						drive = slot->drive;

						state = find_free_drive;
						break;
					}
				}

				state = is_pool_growable1;

				break;

			case find_free_drive:
				if (self->drive != NULL) {
					if (self->drive->changer != slot->changer || (drive != NULL && drive != self->drive)) {
						self->drive->lock->ops->unlock(self->drive->lock);
						self->drive = NULL;
					}
				} else {
					if (drive == NULL)
						drive = changer->ops->find_free_drive(changer, self->pool->format, false, true);

					if (drive == NULL) {
						slot->lock->ops->unlock(slot->lock);

						changer = NULL;
						slot = NULL;

						job->sched_status = st_job_status_pause;
						sleep(20);
						job->sched_status = st_job_status_running;

						if (job->db_status == st_job_status_stopped)
							return false;

						state = check_online_free_size_left;
						break;
					}
				}

				state = has_wrong_media;
				break;

			case has_media:
				if (self->drive->slot->media == NULL) {
					struct st_media * media = slot->media;

					st_job_add_record(self->connect, st_log_level_info, job, "Loading media (%s) from slot #%td to drive #%td of changer [ %s | %s ]", media->name, slot - changer->slots, self->drive - changer->drives, changer->vendor, changer->model);

					int failed = changer->ops->load_slot(changer, slot, self->drive);
					slot->lock->ops->unlock(slot->lock);

					if (failed) {
						st_job_add_record(self->connect, st_log_level_error, job, "Loading media (%s) from slot #%td to drive #%td of changer [ %s | %s ] finished with code = %d", media->name, slot - changer->slots, self->drive - changer->drives, changer->vendor, changer->model, failed);
						return false;
					} else
						st_job_add_record(self->connect, st_log_level_info, job, "Loading media (%s) from slot #%td to drive #%td of changer [ %s | %s ] finished with code = OK", media->name, slot - changer->slots, self->drive - changer->drives, changer->vendor, changer->model);
				}

				state = media_is_read_only;
				break;

			case has_wrong_media:
				if (self->drive == NULL)
					self->drive = drive;

				if (self->drive->slot->media != NULL && self->drive->slot != slot) {
					struct st_media * media = self->drive->slot->media;

					st_job_add_record(self->connect, st_log_level_info, job, "Unloading media (%s) from drive #%td of changer [ %s | %s ]", media->name, self->drive - changer->drives, changer->vendor, changer->model);

					int failed = changer->ops->unload(changer, self->drive);
					if (failed) {
						st_job_add_record(self->connect, st_log_level_error, job, "Unloading media (%s) from drive #%td of changer [ %s | %s ] finished with code = %d", media->name, self->drive - changer->drives, changer->vendor, changer->model, failed);
						return false;
					} else
						st_job_add_record(self->connect, st_log_level_info, job, "Unloading media (%s) from drive #%td of changer [ %s | %s ] finished with code = OK", media->name, self->drive - changer->drives, changer->vendor, changer->model);
				}

				state = has_media;
				break;

			case is_media_formatted:
				if (self->drive->slot->media->status == st_media_status_new) {
					struct st_media * media = self->drive->slot->media;

					st_job_add_record(self->connect, st_log_level_info, job, "Formatting new media (%s) from drive #%td of changer [ %s | %s ]", media->name, changer->drives - self->drive, changer->vendor, changer->model);

					int failed = st_media_write_header(self->drive, self->pool, self->connect);
					if (failed) {
						st_job_add_record(self->connect, st_log_level_error, job, "Formatting new media (%s) from drive #%td of changer [ %s | %s ] finished with code = %d", media->name, changer->drives - self->drive, changer->vendor, changer->model, failed);
						return false;
					} else
						st_job_add_record(self->connect, st_log_level_info, job, "Formatting new media (%s) from drive #%td of changer [ %s | %s ] finished with code = OK", media->name, changer->drives - self->drive, changer->vendor, changer->model);
				}
				return true;

			case is_pool_growable1:
				if (self->pool->growable) {
					// assume that slot, drive, changer are NULL
					slot = st_changer_find_free_media_by_format(self->pool->format);

					if (slot != NULL) {
						changer = slot->changer;
						drive = slot->drive;

						state = find_free_drive;
						break;
					}
				}

				state = check_offline_free_size_left;
				break;

			case is_pool_growable2:
				if (self->pool->growable && !has_alerted_user) {
					st_job_add_record(self->connect, st_log_level_warning, job, "Please, insert new media which will be a part of pool %s", self->pool->name);
				} else if (!has_alerted_user) {
					st_job_add_record(self->connect, st_log_level_warning, job, "Please, you must to extent the pool (%s)", self->pool->name);
				}

				has_alerted_user = true;
				// wait

				job->sched_status = st_job_status_pause;
				sleep(60);
				job->sched_status = st_job_status_running;

				if (job->db_status == st_job_status_stopped)
					return false;

				state = check_online_free_size_left;
				break;

			case media_is_read_only:
				if (self->drive->slot->media->type == st_media_type_readonly) {
					st_job_add_record(self->connect, st_log_level_warning, job, "Media '%s' is currently read only ", self->drive->slot->media->name);
					state = check_online_free_size_left;
				} else
					state = is_media_formatted;

				break;
		}
	}

	return ok;
}

