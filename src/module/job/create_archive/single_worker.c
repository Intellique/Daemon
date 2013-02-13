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
*  Copyright (C) 2013, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Wed, 13 Feb 2013 11:10:53 +0100                         *
\*************************************************************************/

// asprintf
#define _GNU_SOURCE
// bool
#include <stdbool.h>
// asprintf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// strdup
#include <string.h>
// sleep
#include <unistd.h>
// time
#include <time.h>

#include <libstone/format.h>
#include <libstone/io.h>
#include <libstone/library/archive.h>
#include <libstone/library/drive.h>
#include <libstone/library/ressource.h>
#include <libstone/log.h>
#include <libstone/util/hashtable.h>
#include <libstone/user.h>
#include <stoned/io.h>
#include <stoned/library/changer.h>
#include <stoned/library/slot.h>

#include "create_archive.h"

struct st_job_create_archive_single_worker_private {
	struct st_job * job;
	struct st_database_connection * connect;

	struct st_pool * pool;

	struct st_archive * archive;

	ssize_t total_done;
	ssize_t archive_size;

	struct st_drive * drive;
	struct st_format_writer * writer;

	char ** checksums;
	unsigned int nb_checksums;

	struct st_linked_list_files {
		char * path;
		ssize_t block_position;
		struct st_linked_list_files * next;
	} * first_file, * last_file;
	unsigned int nb_files;

	ssize_t file_position;

	struct st_stream_writer * checksum_writer;
	struct st_job_create_archive_meta_worker * meta_worker;
};

static int st_job_create_archive_single_worker_add_file(struct st_job_create_archive_data_worker * worker, struct st_job_selected_path * selected_path, const char * path);
static struct st_stream_writer * st_job_create_archive_single_worker_add_filter(struct st_stream_writer * writer, void * param);
static int st_job_create_archive_single_worker_change_volume(struct st_job_create_archive_single_worker_private * self);
static void st_job_create_archive_single_worker_close(struct st_job_create_archive_data_worker * worker);
static int st_job_create_archive_single_worker_end_file(struct st_job_create_archive_data_worker * worker);
static void st_job_create_archive_single_worker_free(struct st_job_create_archive_data_worker * worker);
static int st_job_create_archive_single_worker_load_media(struct st_job_create_archive_data_worker * worker);
static int st_job_create_archive_single_schedule_auto_check_archive(struct st_job_create_archive_data_worker * worker);
static int st_job_create_archive_single_worker_schedule_check_archive(struct st_job_create_archive_data_worker * worker, time_t start_time, bool quick_mode);
static bool st_job_create_archive_single_worker_select_media(struct st_job_create_archive_single_worker_private * self);
static int st_job_create_archive_single_worker_sync_db(struct st_job_create_archive_data_worker * worker);
static ssize_t st_job_create_archive_single_worker_write(struct st_job_create_archive_data_worker * worker, void * buffer, ssize_t length);
static int st_job_create_archive_single_worker_write_meta(struct st_job_create_archive_data_worker * worker);

static struct st_job_create_archive_data_worker_ops st_job_create_archive_single_worker_ops = {
	.add_file                    = st_job_create_archive_single_worker_add_file,
	.close                       = st_job_create_archive_single_worker_close,
	.end_file                    = st_job_create_archive_single_worker_end_file,
	.free                        = st_job_create_archive_single_worker_free,
	.load_media                  = st_job_create_archive_single_worker_load_media,
	.schedule_auto_check_archive = st_job_create_archive_single_schedule_auto_check_archive,
	.schedule_check_archive      = st_job_create_archive_single_worker_schedule_check_archive,
	.sync_db                     = st_job_create_archive_single_worker_sync_db,
	.write                       = st_job_create_archive_single_worker_write,
	.write_meta                  = st_job_create_archive_single_worker_write_meta,
};


struct st_job_create_archive_data_worker * st_job_create_archive_single_worker(struct st_job * job, ssize_t archive_size, struct st_database_connection * connect, struct st_job_create_archive_meta_worker * meta_worker) {
	struct st_job_create_archive_private * jp = job->data;

	struct st_job_create_archive_single_worker_private * self = malloc(sizeof(struct st_job_create_archive_single_worker_private));
	self->job = job;
	self->connect = connect;

	self->archive = jp->connect->ops->get_archive_by_job(jp->connect, job);
	if (self->archive == NULL) {
		self->archive = st_archive_new(job->name, job->user);
		self->archive->metadatas = strdup(job->meta);
	}

	self->pool = st_pool_get_by_job(job, connect);

	if (self->pool == NULL && self->archive != NULL) {
		self->pool = st_pool_get_by_archive(self->archive, connect);

		if (self->pool != NULL)
			st_job_add_record(connect, st_log_level_info, job, "Using pool (%s) of archive (%s)", self->pool->name, self->archive->name);
	}

	if (self->pool == NULL) {
		self->pool = job->user->pool;
		st_job_add_record(connect, st_log_level_info, job, "Using pool (%s) of user (%s)", self->pool->name, job->user->login);
	}

	self->total_done = 0;
	self->archive_size = archive_size;

	self->drive = NULL;
	self->writer = NULL;

	self->checksums = jp->connect->ops->get_checksums_by_pool(jp->connect, self->pool, &self->nb_checksums);

	self->checksum_writer = NULL;
	self->meta_worker = meta_worker;

	self->first_file = self->last_file = NULL;
	self->nb_files = 0;

	self->file_position = 0;

	struct st_job_create_archive_data_worker * worker = malloc(sizeof(struct st_job_create_archive_data_worker));
	worker->ops = &st_job_create_archive_single_worker_ops;
	worker->data = self;

	return worker;
}

static int st_job_create_archive_single_worker_add_file(struct st_job_create_archive_data_worker * worker, struct st_job_selected_path * selected_path, const char * path) {
	struct st_job_create_archive_single_worker_private * self = worker->data;

	ssize_t position = self->writer->ops->position(self->writer) / self->writer->ops->get_block_size(self->writer);

	if (self->pool->unbreakable_level == st_pool_unbreakable_level_file) {
		ssize_t available_size = self->writer->ops->get_available_size(self->writer);
		ssize_t file_size = self->writer->ops->compute_size_of_file(self->writer, path, false);

		if (available_size < file_size && st_job_create_archive_single_worker_change_volume(self))
			return 1;
	}

	enum st_format_writer_status status = self->writer->ops->add_file(self->writer, path);
	int failed;
	switch (status) {
		case st_format_writer_end_of_volume:
			failed = st_job_create_archive_single_worker_change_volume(self);
			if (failed)
				return failed;
			break;

		case st_format_writer_error:
		case st_format_writer_unsupported:
			return -1;

		case st_format_writer_ok:
			break;
	}

	struct st_linked_list_files * f = malloc(sizeof(struct st_linked_list_files));
	f->path = strdup(path);
	f->block_position = position;
	f->next = NULL;

	if (self->first_file == NULL)
		self->first_file = self->last_file = f;
	else
		self->last_file = self->last_file->next = f;
	self->nb_files++;
	self->file_position = 0;

	self->meta_worker->ops->add_file(self->meta_worker, selected_path, path, self->pool);

	return 0;
}

static struct st_stream_writer * st_job_create_archive_single_worker_add_filter(struct st_stream_writer * writer, void * param) {
	struct st_job_create_archive_single_worker_private * self = param;

	if (self->nb_checksums > 0)
		return self->checksum_writer = st_checksum_writer_new(writer, self->checksums, self->nb_checksums, true);

	return writer;
}

static int st_job_create_archive_single_worker_change_volume(struct st_job_create_archive_single_worker_private * self) {
	self->writer->ops->close(self->writer);

	struct st_archive_volume * last_volume = self->archive->volumes + (self->archive->nb_volumes - 1);
	last_volume->end_time = self->archive->end_time = time(NULL);
	last_volume->size = self->writer->ops->position(self->writer);

	last_volume->digests = st_checksum_writer_get_checksums(self->checksum_writer);
	last_volume->nb_digests = self->nb_checksums;

	self->meta_worker->ops->wait(self->meta_worker, false);

	last_volume->files = calloc(self->nb_files, sizeof(struct st_archive_files));
	unsigned int i;
	struct st_linked_list_files * from_file = self->first_file;
	for (i = 0; i < self->nb_files && from_file != NULL; i++) {
		char * hash;
		asprintf(&hash, "%s:%s", self->pool->uuid, from_file->path);
		struct st_archive_file * f = st_hashtable_get(self->meta_worker->meta_files, hash).value.custom;
		free(hash);

		struct st_archive_files * to_file = last_volume->files + i;
		to_file->file = f;
		to_file->position = from_file->block_position;

		struct st_linked_list_files * next = from_file->next;
		if (from_file->next != NULL) {
			free(from_file->path);
			free(from_file);
		}

		from_file = next;
	}

	self->first_file = NULL;
	last_volume->nb_files = self->nb_files;

	if (!st_job_create_archive_single_worker_select_media(self))
		return 1;

	struct st_stream_writer * writer = self->drive->ops->get_raw_writer(self->drive, true);
	if (writer == NULL)
		return -1;

	self->checksum_writer = st_checksum_writer_new(writer, self->checksums, self->nb_checksums, true);
	self->writer->ops->new_volume(self->writer, self->checksum_writer);

	int position = self->drive->ops->get_position(self->drive);

	st_archive_add_volume(self->archive, self->drive->slot->media, position);

	struct st_linked_list_files * f = self->first_file = self->last_file;
	f->block_position = 0;
	f->next = NULL;
	self->nb_files = 1;

	return 0;
}

static void st_job_create_archive_single_worker_close(struct st_job_create_archive_data_worker * worker) {
	struct st_job_create_archive_single_worker_private * self = worker->data;

	self->writer->ops->close(self->writer);

	struct st_archive_volume * last_volume = self->archive->volumes + (self->archive->nb_volumes - 1);
	last_volume->end_time = self->archive->end_time = time(NULL);
	last_volume->size = self->writer->ops->position(self->writer);

	last_volume->digests = st_checksum_writer_get_checksums(self->checksum_writer);
	last_volume->nb_digests = self->nb_checksums;

	self->meta_worker->ops->wait(self->meta_worker, false);

	last_volume->files = calloc(self->nb_files, sizeof(struct st_archive_files));
	unsigned int i;
	struct st_linked_list_files * from_file = self->first_file;
	for (i = 0; i < self->nb_files && from_file != NULL; i++) {
		char * hash;
		asprintf(&hash, "%s:%s", self->pool->uuid, from_file->path);
		struct st_archive_file * f = st_hashtable_get(self->meta_worker->meta_files, hash).value.custom;
		free(hash);

		struct st_archive_files * to_file = last_volume->files + i;
		to_file->file = f;
		to_file->position = from_file->block_position;

		struct st_linked_list_files * next = from_file->next;
		free(from_file->path);
		free(from_file);

		from_file = next;
	}

	self->first_file = self->last_file = NULL;
	last_volume->nb_files = self->nb_files;
	self->nb_files = 0;
}

static int st_job_create_archive_single_worker_end_file(struct st_job_create_archive_data_worker * worker) {
	struct st_job_create_archive_single_worker_private * self = worker->data;
	ssize_t nb_write = self->writer->ops->end_of_file(self->writer);

	return nb_write >= 0 ? 0 : nb_write;
}

static void st_job_create_archive_single_worker_free(struct st_job_create_archive_data_worker * worker) {
	struct st_job_create_archive_single_worker_private * self = worker->data;

	if (self->drive != NULL)
		self->drive->lock->ops->unlock(self->drive->lock);

	self->writer->ops->free(self->writer);

	st_archive_free(self->archive);

	unsigned int i;
	for (i = 0; i < self->nb_checksums; i++)
		free(self->checksums[i]);
	free(self->checksums);

	free(worker->data);
	free(worker);
}

static int st_job_create_archive_single_worker_load_media(struct st_job_create_archive_data_worker * worker) {
	struct st_job_create_archive_single_worker_private * self = worker->data;

	if (st_job_create_archive_single_worker_select_media(self)) {
		self->writer = self->drive->ops->get_writer(self->drive, true, st_job_create_archive_single_worker_add_filter, self);

		int position = self->drive->ops->get_position(self->drive);
		st_archive_add_volume(self->archive, self->drive->slot->media, position);

		return self->writer == NULL;
	}

	return 1;
}

static int st_job_create_archive_single_schedule_auto_check_archive(struct st_job_create_archive_data_worker * worker) {
	struct st_job_create_archive_single_worker_private * self = worker->data;

	if (self->pool->auto_check == st_pool_autocheck_quick_mode || self->pool->auto_check == st_pool_autocheck_thorough_mode)
		return self->connect->ops->add_check_archive_job(self->connect, self->job, self->archive, time(NULL), self->pool->auto_check == st_pool_autocheck_quick_mode);

	return 0;
}

static int st_job_create_archive_single_worker_schedule_check_archive(struct st_job_create_archive_data_worker * worker, time_t start_time, bool quick_mode) {
	struct st_job_create_archive_single_worker_private * self = worker->data;
	return self->connect->ops->add_check_archive_job(self->connect, self->job, self->archive, start_time, quick_mode);
}

static bool st_job_create_archive_single_worker_select_media(struct st_job_create_archive_single_worker_private * self) {
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
	// ssize_t archive_size = 0;

	while (!stop) {
		switch (state) {
			case check_offline_free_size_left:
				total_size += self->connect->ops->get_available_size_of_offline_media_from_pool(self->connect, self->pool);

				if (self->archive_size - self->total_done > total_size) {
					// alert user
					if (!has_alerted_user)
						st_job_add_record(self->connect, st_log_level_warning, self->job, "Please, insert media which is a part of pool named %s", self->pool->name);
					has_alerted_user = true;

					self->job->sched_status = st_job_status_pause;
					sleep(10);
					self->job->sched_status = st_job_status_running;

					if (self->job->db_status == st_job_status_stopped)
						return false;

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
						if (self->archive_size - self->total_done < media->free_block * media->block_size)
							break;

						if (10 * media->free_block < media->total_block)
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
						drive = changer->ops->find_free_drive(changer, self->pool->format, false, true);

					if (drive == NULL) {
						slot->lock->ops->unlock(slot->lock);

						changer = NULL;
						slot = NULL;

						self->job->sched_status = st_job_status_pause;
						sleep(20);
						self->job->sched_status = st_job_status_running;

						if (self->job->db_status == st_job_status_stopped)
							return false;

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

				if (self->job->db_status == st_job_status_stopped)
					return false;

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

static int st_job_create_archive_single_worker_sync_db(struct st_job_create_archive_data_worker * worker) {
	struct st_job_create_archive_single_worker_private * self = worker->data;

	return self->connect->ops->sync_archive(self->connect, self->archive, self->checksums);
}

static ssize_t st_job_create_archive_single_worker_write(struct st_job_create_archive_data_worker * worker, void * buffer, ssize_t length) {
	struct st_job_create_archive_single_worker_private * self = worker->data;

	ssize_t available = self->writer->ops->get_available_size(self->writer);
	if (available == 0) {
		int failed = st_job_create_archive_single_worker_change_volume(self);
		if (failed)
			return failed;

		self->writer->ops->restart_file(self->writer, self->last_file->path, self->file_position);
	}

	ssize_t will_write = available > length ? length : available;

	ssize_t nb_write = self->writer->ops->write(self->writer, buffer, will_write);
	if (nb_write > 0) {
		self->total_done += nb_write;
		self->file_position += nb_write;
	}

	if (will_write < length) {
		if (st_job_create_archive_single_worker_change_volume(self))
			return 1;

		self->writer->ops->restart_file(self->writer, self->last_file->path, self->file_position);
	}

	return nb_write;
}

static int st_job_create_archive_single_worker_write_meta(struct st_job_create_archive_data_worker * worker) {
	struct st_job_create_archive_single_worker_private * self = worker->data;
	struct st_stream_writer * writer = self->drive->ops->get_raw_writer(self->drive, true);

	ssize_t nb_write = st_io_json_writer(writer, self->archive, self->checksums);

	writer->ops->close(writer);
	writer->ops->free(writer);

	return nb_write < 1;
}

