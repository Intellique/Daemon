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
*  Last modified: Sat, 29 Dec 2012 12:44:54 +0100                         *
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
		unsigned int nb_warnings;
		unsigned int nb_errors;

		ssize_t total_restored;

		struct st_drive * drive;
		struct st_slot * slot;
		struct st_media * media;

		pthread_mutex_t lock;
		pthread_cond_t wait;
		volatile bool running;

		struct st_job_restore_archive_data_worker * next;
	} * first_worker, * last_worker;

	struct st_job_restore_archive_path {
		struct st_hashtable * paths;
		pthread_mutex_t lock;
	} * restore_path;

	struct st_job_restore_archive_checks_worker {
		struct st_job_restore_archive_private * jp;
		unsigned int nb_warnings;
		unsigned int nb_errors;

		struct st_job_restore_archive_checks_files {
			struct st_archive_file * file;
			bool checked;
			unsigned int nb_volume_restored;
			unsigned int nb_volumes;
			struct st_job_restore_archive_checks_files * next;
		} * first_file, * last_file;

		pthread_mutex_t lock;
		pthread_cond_t wait;

		volatile bool running;
		struct st_database_connection * connect;
	} * checks;
};

void st_job_restore_archive_checks_worker_add_file(struct st_job_restore_archive_checks_worker * check, struct st_archive_file * file);
void st_job_restore_archive_checks_worker_free(struct st_job_restore_archive_checks_worker * check);
struct st_job_restore_archive_checks_worker * st_job_restore_archive_checks_worker_new(struct st_job_restore_archive_private * jp);

void st_job_restore_archive_data_worker_free(struct st_job_restore_archive_data_worker * worker);
struct st_job_restore_archive_data_worker * st_job_restore_archive_data_worker_new(struct st_job_restore_archive_private * self, struct st_drive * drive, struct st_slot * slot);
bool st_job_restore_archive_data_worker_wait(struct st_job_restore_archive_data_worker * worker);

void st_job_restore_archive_path_free(struct st_job_restore_archive_path * restore_path);
char * st_job_restore_archive_path_get(struct st_job_restore_archive_path * restore_path, struct st_database_connection * connect, struct st_job * job, struct st_archive_file * file, bool has_restore_to);
struct st_job_restore_archive_path * st_job_restore_archive_path_new(void);

#endif

