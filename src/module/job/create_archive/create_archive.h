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
*  Last modified: Wed, 13 Feb 2013 11:10:18 +0100                         *
\*************************************************************************/

#ifndef __STONE_JOB_CREATEARCHIVE_H__
#define __STONE_JOB_CREATEARCHIVE_H__

// size_t
#include <sys/types.h>

#include <libstone/database.h>
#include <libstone/job.h>

struct st_hashtable;

struct st_job_create_archive_private {
	struct st_database_connection * connect;

	struct st_job_selected_path * selected_paths;
	unsigned int nb_selected_paths;

	ssize_t total_done;
	ssize_t total_size;

	struct st_job_create_archive_data_worker {
		struct st_job_create_archive_data_worker_ops {
			int (*add_file)(struct st_job_create_archive_data_worker * worker, struct st_job_selected_path * selected_path, const char * path);
			void (*close)(struct st_job_create_archive_data_worker * worker);
			int (*end_file)(struct st_job_create_archive_data_worker * worker);
			void (*free)(struct st_job_create_archive_data_worker * worker);
			int (*load_media)(struct st_job_create_archive_data_worker * worker);
			int (*schedule_auto_check_archive)(struct st_job_create_archive_data_worker * worker);
			int (*schedule_check_archive)(struct st_job_create_archive_data_worker * worker, time_t start_time, bool quick_mode);
			int (*sync_db)(struct st_job_create_archive_data_worker * worker);
			ssize_t (*write)(struct st_job_create_archive_data_worker * worker, void * buffer, ssize_t length);
			int (*write_meta)(struct st_job_create_archive_data_worker * worker);
		} * ops;
		void * data;
	} * worker;

	struct st_job_create_archive_meta_worker {
		struct st_job_create_archive_meta_worker_ops {
			void (*add_file)(struct st_job_create_archive_meta_worker * worker, struct st_job_selected_path * selected_path, const char * path, struct st_pool * pool);
			void (*free)(struct st_job_create_archive_meta_worker * worker);
			void (*wait)(struct st_job_create_archive_meta_worker * worker, bool stop);
		} * ops;
		void * data;

		struct st_hashtable * meta_files;
	} * meta;
};

ssize_t st_job_create_archive_compute_size(const char * path);

struct st_job_create_archive_data_worker * st_job_create_archive_single_worker(struct st_job * job, ssize_t archive_size, struct st_database_connection * connect, struct st_job_create_archive_meta_worker * meta_worker);

struct st_job_create_archive_meta_worker * st_job_create_archive_meta_worker_new(struct st_job * job, struct st_database_connection * connect);

#endif

