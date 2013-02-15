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
*  Last modified: Fri, 15 Feb 2013 13:48:54 +0100                         *
\*************************************************************************/

// time
#include <time.h>
// sleep
#include <unistd.h>

#include <libstone/database.h>
#include <libstone/format.h>
#include <libstone/io.h>
#include <libstone/library/archive.h>
#include <libstone/library/drive.h>
#include <libstone/library/ressource.h>
#include <libstone/log.h>
#include <stoned/library/changer.h>
#include <stoned/library/slot.h>

#include "copy_archive.h"

struct st_stream_writer * st_job_copy_archive_add_filter(struct st_stream_writer * writer, void * param) {
	struct st_job_copy_archive_private * self = param;

	if (self->nb_checksums > 0)
		return self->checksum_writer = st_checksum_writer_new(writer, self->checksums, self->nb_checksums, true);

	return writer;
}

bool st_job_copy_archive_change_ouput_media(struct st_job_copy_archive_private * self) {
	self->writer->ops->close(self->writer);

	self->current_volume->end_time = self->copy->end_time = time(NULL);
	self->current_volume->size = self->writer->ops->position(self->writer);

	self->current_volume->digests = st_checksum_writer_get_checksums(self->checksum_writer);
	self->current_volume->nb_digests = self->nb_checksums;

	self->writer->ops->free(self->writer);
	self->writer = NULL;

	if (!st_job_copy_archive_select_output_media(self, true))
		return false;

	self->writer = self->drive_output->ops->get_writer(self->drive_output, true, st_job_copy_archive_add_filter, self);
	if (self->writer == NULL)
		return false;

	int position = self->drive_output->ops->get_position(self->drive_output);

	st_archive_add_volume(self->copy, self->drive_output->slot->media, position);
	self->current_volume = self->copy->volumes + self->copy->nb_volumes;

	return true;
}

bool st_job_copy_archive_select_input_media(struct st_job_copy_archive_private * self, struct st_media * media) {
	struct st_slot * slot =	st_changer_find_slot_by_media(media);
	if (slot == NULL)
		return false;

	struct st_changer * changer = slot->changer;
	struct st_drive * drive = slot->drive;

	if (drive != NULL) {
		if (drive->lock->ops->timed_lock(drive->lock, 5000))
			return false;
	} else {
		if (slot->lock->ops->timed_lock(slot->lock, 5000))
			return false;

		drive = changer->ops->find_free_drive(changer, slot->media->format, true, false);
		if (drive == NULL)
			return false;
	}

	self->drive_input = drive;
	self->slot_input = slot;

	return true;
}

bool st_job_copy_archive_select_output_media(struct st_job_copy_archive_private * self, bool load_media) {
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
				if (self->drive_output != NULL) {
					if (self->drive_output->changer != slot->changer || (drive != NULL && drive != self->drive_output)) {
						self->drive_output->lock->ops->unlock(self->drive_output->lock);
						self->drive_output = NULL;
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

	if (!load_media) {
		self->drive_output = drive;
		self->slot_output = slot;
		return ok;
	}

	if (ok) {
		if (self->drive_output == NULL)
			self->drive_output = drive;

		if (self->drive_output->slot->media != NULL && self->drive_output->slot != slot) {
			struct st_media * media = self->drive_output->slot->media;

			st_job_add_record(self->connect, st_log_level_info, self->job, "Unloading media (%s) from drive #%td of changer [ %s | %s ]", media->name, self->drive_output - changer->drives, changer->vendor, changer->model);

			int failed = changer->ops->unload(changer, self->drive_output);
			if (failed) {
				st_job_add_record(self->connect, st_log_level_error, self->job, "Unloading media (%s) from drive #%td of changer [ %s | %s ] finished with code = %d", media->name, self->drive_output - changer->drives, changer->vendor, changer->model, failed);
				ok = false;
			} else
				st_job_add_record(self->connect, st_log_level_info, self->job, "Unloading media (%s) from drive #%td of changer [ %s | %s ] finished with code = OK", media->name, self->drive_output - changer->drives, changer->vendor, changer->model);
		}
	}

	if (ok) {
		if (self->drive_output->slot->media == NULL) {
			struct st_media * media = slot->media;

			st_job_add_record(self->connect, st_log_level_info, self->job, "Loading media (%s) from slot #%td to drive #%td of changer [ %s | %s ]", media->name, slot - changer->slots, self->drive_output - changer->drives, changer->vendor, changer->model);

			int failed = changer->ops->load_slot(changer, slot, self->drive_output);
			if (failed) {
				st_job_add_record(self->connect, st_log_level_error, self->job, "Loading media (%s) from slot #%td to drive #%td of changer [ %s | %s ] finished with code = %d", media->name, slot - changer->slots, self->drive_output - changer->drives, changer->vendor, changer->model, failed);
				ok = false;
			} else
				st_job_add_record(self->connect, st_log_level_info, self->job, "Loading media (%s) from slot #%td to drive #%td of changer [ %s | %s ] finished with code = OK", media->name, slot - changer->slots, self->drive_output - changer->drives, changer->vendor, changer->model);
		}
	}

	if (ok) {
		if (self->drive_output->slot->media->status == st_media_status_new) {
			struct st_media * media = self->drive_output->slot->media;

			st_job_add_record(self->connect, st_log_level_info, self->job, "Formatting new media (%s) from drive #%td of changer [ %s | %s ]", media->name, changer->drives - self->drive_output, changer->vendor, changer->model);

			int failed = st_media_write_header(self->drive_output, self->pool);
			if (failed) {
				st_job_add_record(self->connect, st_log_level_error, self->job, "Formatting new media (%s) from drive #%td of changer [ %s | %s ] finished with code = %d", media->name, changer->drives - self->drive_output, changer->vendor, changer->model, failed);
				ok = false;
			} else
				st_job_add_record(self->connect, st_log_level_info, self->job, "Formatting new media (%s) from drive #%td of changer [ %s | %s ] finished with code = OK", media->name, changer->drives - self->drive_output, changer->vendor, changer->model);
		}
	}

	return ok;
}

