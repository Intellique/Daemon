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
*  Last modified: Sun, 16 Dec 2012 18:53:53 +0100                         *
\*************************************************************************/

#ifndef __STONE_JOB_RESTOREARCHIVE_H__
#define __STONE_JOB_RESTOREARCHIVE_H__

// pthread_cond_t
#include <pthread.h>
// size_t
#include <sys/types.h>

#include <libstone/database.h>
#include <libstone/library/archive.h>

struct st_slot;

struct st_job_restore_archive_private {
	struct st_job * job;
	struct st_database_connection * connect;

	struct st_archive * archive;

	struct st_job_restore_archive_data_worker {
		struct st_job_restore_archive_private * jp;

		ssize_t total_restored;

		struct st_drive * drive;
		struct st_slot * slot;
		struct st_media * media;

		pthread_mutex_t lock;
		pthread_cond_t wait;
		volatile bool running;

		struct st_job_restore_archive_data_worker * next;
	} * first_worker, * last_worker;

	struct st_job_restore_archive_checks_worker {
		struct st_job_restore_archive_checks_worker_ops {
		} * ops;
		void * data;
	} * checks;
};

void st_job_restore_archive_data_worker_free(struct st_job_restore_archive_data_worker * worker);
struct st_job_restore_archive_data_worker * st_job_restore_archive_data_worker_new(struct st_job_restore_archive_private * self, struct st_drive * drive, struct st_slot * slot);
void st_job_restore_archive_data_worker_wait(struct st_job_restore_archive_data_worker * worker);

#endif

