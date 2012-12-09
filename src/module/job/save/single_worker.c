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
*  Last modified: Sun, 09 Dec 2012 14:24:02 +0100                         *
\*************************************************************************/

// bool
#include <stdbool.h>
// free, malloc
#include <stdlib.h>
// sleep
#include <unistd.h>
// time
#include <time.h>

#include <libstone/format.h>
#include <libstone/library/archive.h>
#include <libstone/library/drive.h>
#include <libstone/library/ressource.h>
#include <libstone/log.h>
#include <stoned/library/changer.h>
#include <stoned/library/slot.h>

#include "save.h"

struct st_job_save_single_worker_private {
	struct st_job * job;
	struct st_database_connection * connect;

	struct st_pool * pool;

	struct st_archive * archive;

	ssize_t total_done;
	ssize_t archive_size;

	struct st_drive * drive;
	struct st_format_writer * writer;
};

static int st_job_save_single_worker_add_file(struct st_job_save_data_worker * worker, const char * path);
static void st_job_save_single_worker_close(struct st_job_save_data_worker * worker);
static void st_job_save_single_worker_free(struct st_job_save_data_worker * worker);
static int st_job_save_single_worker_load_media(struct st_job_save_data_worker * worker);
static bool st_job_save_single_worker_select_media(struct st_job_save_single_worker_private * self);
static ssize_t st_job_save_single_worker_write(struct st_job_save_data_worker * worker, void * buffer, ssize_t length);

static struct st_job_save_data_worker_ops st_job_save_single_worker_ops = {
	.add_file   = st_job_save_single_worker_add_file,
	.close      = st_job_save_single_worker_close,
	.free       = st_job_save_single_worker_free,
	.load_media = st_job_save_single_worker_load_media,
	.write      = st_job_save_single_worker_write,
};


struct st_job_save_data_worker * st_job_save_single_worker(struct st_job * job, struct st_pool * pool, ssize_t archive_size, struct st_database_connection * connect) {
	struct st_job_save_single_worker_private * self = malloc(sizeof(struct st_job_save_single_worker_private));
	self->job = job;
	self->connect = connect;

	self->pool = pool;

	self->archive = st_archive_new(job->name, job->user);

	self->total_done = 0;
	self->archive_size = archive_size;

	self->drive = NULL;
	self->writer = NULL;

	struct st_job_save_data_worker * worker = malloc(sizeof(struct st_job_save_data_worker));
	worker->ops = &st_job_save_single_worker_ops;
	worker->data = self;

	return worker;
}

static int st_job_save_single_worker_add_file(struct st_job_save_data_worker * worker, const char * path) {
	struct st_job_save_single_worker_private * self = worker->data;

	self->writer->ops->add_file(self->writer, path);

	return 0;
}

static void st_job_save_single_worker_close(struct st_job_save_data_worker * worker) {
	struct st_job_save_single_worker_private * self = worker->data;

	self->writer->ops->close(self->writer);

	struct st_archive_volume * last_volume = self->archive->volumes + (self->archive->nb_volumes - 1);
	last_volume->endtime = self->archive->endtime = time(NULL);
	last_volume->size = self->writer->ops->position(self->writer);

	// TODO: checksum
}

static void st_job_save_single_worker_free(struct st_job_save_data_worker * worker) {
	struct st_job_save_single_worker_private * self = worker->data;

	if (self->drive != NULL)
		self->drive->lock->ops->unlock(self->drive->lock);

	self->writer->ops->free(self->writer);

	st_archive_free(self->archive);

	free(worker->data);
	free(worker);
}

static int st_job_save_single_worker_load_media(struct st_job_save_data_worker * worker) {
	struct st_job_save_single_worker_private * self = worker->data;

	if (st_job_save_single_worker_select_media(self)) {
		self->writer = self->drive->ops->get_writer(self->drive, true, NULL, NULL);

		int position = self->drive->ops->get_position(self->drive);

		st_archive_add_volume(self->archive, self->drive->slot->media, position);

		return self->writer == NULL;
	}

	return 1;
}

static bool st_job_save_single_worker_select_media(struct st_job_save_single_worker_private * self) {
	bool has_alerted_user = false;
	bool ok = false;
	bool stop = false;
	enum state {
		check_offline_free_size_left,
		check_online_free_size_left,
		find_free_drive,
		is_pool_growable1,
		is_pool_growable2,
	} state = check_online_free_size_left;

	struct st_changer * changer = NULL;
	struct st_drive * drive = NULL;
	struct st_slot * slot = NULL;

	ssize_t total_size = 0;

	while (!stop) {
		switch (state) {
			case check_offline_free_size_left:
				total_size += self->connect->ops->get_available_size_of_offline_media_from_pool(self->connect, self->pool);

				if (self->archive_size - self->total_done > total_size) {
					// alert user
					if (!has_alerted_user)
						st_job_add_record(self->connect, st_log_level_warning, self->job, "Please, insert media which is a part of pool named %s", self->pool->name);
					has_alerted_user = true;

					state = check_online_free_size_left;
				} else {
					state = is_pool_growable2;
				}

				break;

			case check_online_free_size_left:
				total_size = st_changer_get_online_size(self->pool);

				if (self->archive_size - self->total_done < total_size) {
					struct st_slot_iterator * iter = st_slot_iterator_by_pool(self->pool);

					while (iter->ops->has_next(iter)) {
						slot = iter->ops->next(iter);

						struct st_media * media = slot->media;
						if (self->archive_size - self->total_done < media->available_block * media->block_size)
							break;

						if (10 * media->available_block < media->total_block)
							break;

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
						drive = changer->ops->find_free_drive(changer);

					if (drive == NULL) {
						slot->lock->ops->unlock(slot->lock);

						changer = NULL;
						slot = NULL;

						self->job->sched_status = st_job_status_pause;
						sleep(20);
						self->job->sched_status = st_job_status_running;

						state = check_online_free_size_left;
						break;
					}
				}

				stop = true;
				ok = true;
				break;

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
					st_job_add_record(self->connect, st_log_level_warning, self->job, "Please, insert new media which will be a part of pool %s", self->pool->name);
				} else if (!has_alerted_user) {
					st_job_add_record(self->connect, st_log_level_warning, self->job, "Please, you must to extent the pool (%s)", self->pool->name);
				}

				has_alerted_user = true;
				// wait

				self->job->sched_status = st_job_status_pause;
				sleep(60);
				self->job->sched_status = st_job_status_running;

				state = check_online_free_size_left;
				break;
		}
	}

	if (ok) {
		if (self->drive == NULL)
			self->drive = drive;

		if (self->drive->slot->media != NULL && self->drive->slot != slot) {
			struct st_media * media = self->drive->slot->media;

			st_job_add_record(self->connect, st_log_level_info, self->job, "Unloading media (%s) from drive #%td of changer [ %s | %s ]", media->name, self->drive - changer->drives, changer->vendor, changer->model);

			int failed = changer->ops->unload(changer, self->drive);
			if (failed) {
				st_job_add_record(self->connect, st_log_level_error, self->job, "Unloading media (%s) from drive #%td of changer [ %s | %s ] finished with code = %d", media->name, self->drive - changer->drives, changer->vendor, changer->model, failed);
				ok = false;
			} else
				st_job_add_record(self->connect, st_log_level_info, self->job, "Unloading media (%s) from drive #%td of changer [ %s | %s ] finished with code = OK", media->name, self->drive - changer->drives, changer->vendor, changer->model);
		}
	}

	if (ok) {
		if (self->drive->slot->media == NULL) {
			struct st_media * media = slot->media;

			st_job_add_record(self->connect, st_log_level_info, self->job, "Loading media (%s) from slot #%td to drive #%td of changer [ %s | %s ]", media->name, slot - changer->slots, self->drive - changer->drives, changer->vendor, changer->model);

			int failed = changer->ops->load_slot(changer, slot, self->drive);
			if (failed) {
				st_job_add_record(self->connect, st_log_level_error, self->job, "Loading media (%s) from slot #%td to drive #%td of changer [ %s | %s ] finished with code = %d", media->name, slot - changer->slots, self->drive - changer->drives, changer->vendor, changer->model, failed);
				ok = false;
			} else
				st_job_add_record(self->connect, st_log_level_info, self->job, "Loading media (%s) from slot #%td to drive #%td of changer [ %s | %s ] finished with code = OK", media->name, slot - changer->slots, self->drive - changer->drives, changer->vendor, changer->model);
		}
	}

	if (ok) {
		if (self->drive->slot->media->status == st_media_status_new) {
			struct st_media * media = self->drive->slot->media;

			st_job_add_record(self->connect, st_log_level_info, self->job, "Formatting new media (%s) from drive #%td of changer [ %s | %s ]", media->name, changer->drives - self->drive, changer->vendor, changer->model);

			int failed = st_media_write_header(self->drive, self->pool);
			if (failed) {
				st_job_add_record(self->connect, st_log_level_error, self->job, "Formatting new media (%s) from drive #%td of changer [ %s | %s ] finished with code = %d", media->name, changer->drives - self->drive, changer->vendor, changer->model, failed);
				ok = false;
			} else
				st_job_add_record(self->connect, st_log_level_info, self->job, "Formatting new media (%s) from drive #%td of changer [ %s | %s ] finished with code = OK", media->name, changer->drives - self->drive, changer->vendor, changer->model);
		}
	}

	return ok;
}

static ssize_t st_job_save_single_worker_write(struct st_job_save_data_worker * worker, void * buffer, ssize_t length) {
	struct st_job_save_single_worker_private * self = worker->data;
	ssize_t nb_write = self->writer->ops->write(self->writer, buffer, length);
	if (nb_write > 0)
		self->total_done += nb_write;
	return nb_write;
}

