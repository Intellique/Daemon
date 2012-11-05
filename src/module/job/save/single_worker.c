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
*  Last modified: Sun, 04 Nov 2012 20:44:50 +0100                         *
\*************************************************************************/

// bool
#include <stdbool.h>
// free, malloc
#include <stdlib.h>
// sleep
#include <unistd.h>

#include <libstone/library/drive.h>
#include <libstone/library/ressource.h>
#include <libstone/library/slot.h>
#include <stoned/library/changer.h>

#include "save.h"

struct st_job_save_single_worker_private {
	struct st_pool * pool;

	ssize_t total_done;
	ssize_t archive_size;

	struct st_drive * drive;

	struct st_media ** medias_used;
	unsigned int nb_medias_used;
};

static void st_job_save_single_worker_add_media(struct st_job_save_single_worker_private * self, struct st_media * media);
static void st_job_save_single_worker_free(struct st_job_save_data_worker * worker);
static int st_job_save_single_worker_load_media(struct st_job_save_data_worker * worker);
static bool st_job_save_single_worker_select_media(struct st_job_save_single_worker_private * self);
static ssize_t st_job_save_single_worker_write(struct st_job_save_data_worker * worker, void * buffer, ssize_t length);

static struct st_job_save_data_worker_ops st_job_save_single_worker_ops = {
	.free       = st_job_save_single_worker_free,
	.load_media = st_job_save_single_worker_load_media,
	.write      = st_job_save_single_worker_write,
};


struct st_job_save_data_worker * st_job_save_single_worker(struct st_pool * pool, ssize_t archive_size) {
	struct st_job_save_single_worker_private * self = malloc(sizeof(struct st_job_save_single_worker_private));
	self->pool = pool;

	self->total_done = 0;
	self->archive_size = archive_size;

	self->drive = NULL;

	self->medias_used = NULL;
	self->nb_medias_used = 0;

	struct st_job_save_data_worker * worker = malloc(sizeof(struct st_job_save_data_worker));
	worker->ops = &st_job_save_single_worker_ops;
	worker->data = self;

	return worker;
}

static void st_job_save_single_worker_add_media(struct st_job_save_single_worker_private * self, struct st_media * media) {
	void * new_addr = realloc(self->medias_used, (self->nb_medias_used + 1) * sizeof(struct st_media *));
	if (new_addr == NULL) {
		// add warning
		return;
	}

	self->medias_used = new_addr;
	self->medias_used[self->nb_medias_used] = media;
	self->nb_medias_used++;
}

static void st_job_save_single_worker_free(struct st_job_save_data_worker * worker) {
	struct st_job_save_single_worker_private * self = worker->data;
	free(self->medias_used);

	free(self);
	free(worker);
}

static int st_job_save_single_worker_load_media(struct st_job_save_data_worker * worker) {
}

static bool st_job_save_single_worker_select_media(struct st_job_save_single_worker_private * self) {
	bool stop = false;
	enum state {
		check_offline_free_size_left,
		check_online_free_size_left,
		check_media_free_space,
		check_media_in_drive,
		find_free_drive,
		is_pool_growable1,
		is_pool_growable2,
	} state = find_free_drive;

	struct st_changer * changer = NULL;
	struct st_drive * drive = NULL;
	struct st_slot * slot = NULL;

	ssize_t total_size = 0;

	while (!stop) {
		switch (state) {
			case check_online_free_size_left:
				total_size = st_changer_get_online_size(self->pool);

				if (self->archive_size - self->total_done < total_size) {
					if (slot == drive->slot) {
						// media is already loaded
						stop = true;
						break;
					}

					if (slot != NULL) {
						ssize_t media_available = st_media_get_available_size(slot->media);

						if (self->archive_size - self->total_done < media_available) {
							stop = true;
							break;
						}

						if (10 * media_available < slot->media->format->capacity) {
							// release lock
							if (slot == drive->slot)
								drive->lock->ops->unlock(drive->lock);
							else
								slot->lock->ops->unlock(slot->lock);

							st_job_save_single_worker_add_media(self, slot->media);

							changer = NULL;
							drive = NULL;
							slot = NULL;

							state = find_free_drive;
							break;
						}
					}

					stop = true;
					break;
				}

				state = is_pool_growable1;

				break;

			case check_media_free_space:
				if (st_media_get_available_size(drive->slot->media) >= drive->slot->media->format->capacity / 10)
					stop = true;
				else
					state = is_pool_growable1;

				break;

			case check_media_in_drive:
				if (drive->slot != slot && drive->slot->media != NULL)
					changer->ops->unload(changer, drive);

				state = check_online_free_size_left;
				// check remain archive size
				if (drive->slot->media != NULL && st_media_get_available_size(drive->slot->media) > (self->archive_size - self->total_done))
					state = check_media_free_space;

				break;

			case find_free_drive:
				slot = st_changer_find_media_by_pool(self->pool, self->medias_used, self->nb_medias_used);

				if (slot == NULL) {
					sleep(20);
					break;
				}

				changer = slot->changer;
				drive = slot->drive;

				if (drive == NULL) {
					drive = changer->ops->find_free_drive(changer);

					if (drive == NULL) {
						slot->lock->ops->unlock(slot->lock);

						changer = NULL;
						slot = NULL;

						sleep(20);
						break;
					}
				}

				state = check_media_in_drive;
				break;

			case is_pool_growable1:
				if (self->pool->growable) {
				}

				state = check_offline_free_size_left;
				break;
		}
	}

	return true;
}

static ssize_t st_job_save_single_worker_write(struct st_job_save_data_worker * worker, void * buffer, ssize_t length) {
}

