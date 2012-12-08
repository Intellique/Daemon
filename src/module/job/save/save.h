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
*  Last modified: Sat, 08 Dec 2012 12:57:04 +0100                         *
\*************************************************************************/

#ifndef __STONE_JOB_SAVE_H__
#define __STONE_JOB_SAVE_H__

// size_t
#include <sys/types.h>

#include <libstone/database.h>
#include <libstone/job.h>

struct st_job_save_private {
	struct st_database_connection * connect;

	struct st_job_selected_path * selected_paths;
	unsigned int nb_selected_paths;

	ssize_t total_done;
	ssize_t total_size;

	struct st_job_save_data_worker {
		struct st_job_save_data_worker_ops {
			int (*add_file)(struct st_job_save_data_worker * worker, const char * path);
			void (*close)(struct st_job_save_data_worker * worker);
			void (*free)(struct st_job_save_data_worker * worker);
			int (*load_media)(struct st_job_save_data_worker * worker);
			ssize_t (*write)(struct st_job_save_data_worker * worker, void * buffer, ssize_t length);
		} * ops;
		void * data;
	} * worker;
};

ssize_t st_job_save_compute_size(const char * path);

struct st_job_save_data_worker * st_job_save_single_worker(struct st_job * job, struct st_pool * pool, ssize_t archive_size, struct st_database_connection * connect);

#endif

