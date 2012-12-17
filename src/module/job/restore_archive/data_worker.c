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
*  Last modified: Sun, 16 Dec 2012 18:54:17 +0100                         *
\*************************************************************************/

// free, malloc
#include <stdlib.h>

#include <libstone/library/changer.h>
#include <libstone/library/drive.h>
#include <libstone/library/slot.h>
#include <libstone/thread_pool.h>

#include "restore_archive.h"

static void st_job_restore_archive_data_worker_work(void * arg);

void st_job_restore_archive_data_worker_free(struct st_job_restore_archive_data_worker * worker) {
	free(worker);
}

struct st_job_restore_archive_data_worker * st_job_restore_archive_data_worker_new(struct st_job_restore_archive_private * self, struct st_drive * drive, struct st_slot * slot) {
	struct st_job_restore_archive_data_worker * worker = malloc(sizeof(struct st_job_restore_archive_data_worker));
	worker->jp = self;

	worker->total_restored = 0;

	worker->drive = drive;
	worker->slot = slot;
	worker->media = slot->media;

	pthread_mutex_init(&worker->lock, NULL);
	pthread_cond_init(&worker->wait, NULL);

	worker->running = true;
	worker->next = NULL;

	st_thread_pool_run(st_job_restore_archive_data_worker_work, worker);

	return worker;
}

void st_job_restore_archive_data_worker_wait(struct st_job_restore_archive_data_worker * worker) {
	struct st_job_restore_archive_data_worker * self = worker;

	if (!self->running)
		return;

	pthread_mutex_lock(&worker->lock);
	pthread_cond_wait(&worker->wait, &worker->lock);
	pthread_mutex_unlock(&worker->lock);
}

static void st_job_restore_archive_data_worker_work(void * arg) {
	struct st_job_restore_archive_data_worker * self = arg;
	struct st_database_connection * connect = self->jp->connect->config->ops->connect(self->jp->connect->config);

	self->running = true;

	struct st_drive * dr = self->drive;
	struct st_slot * sl = self->slot;

	if (dr->slot->media != NULL && dr->slot != sl) {
		dr->changer->ops->unload(dr->changer, dr);
	}

	if (dr->slot->media == NULL) {
		dr->changer->ops->load_slot(dr->changer, sl, dr);
	}

	struct st_archive * archive = self->jp->archive;

	unsigned int i;
	for (i = 0; i < archive->nb_volumes; i++) {
		struct st_archive_volume * vol = archive->volumes + i;

		if (vol->media != self->media)
			continue;

		connect->ops->get_archive_files_by_job_and_archive_volume(connect, self->jp->job, vol);

		unsigned int j;
		for (j = 0; j < vol->nb_files; j++) {
		}
	}
}

