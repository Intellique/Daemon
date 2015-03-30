/****************************************************************************\
*                    ______           _      ____                            *
*                   / __/ /____  ____(_)__ _/ __ \___  ___                   *
*                  _\ \/ __/ _ \/ __/ / _ `/ /_/ / _ \/ -_)                  *
*                 /___/\__/\___/_/ /_/\_, /\____/_//_/\__/                   *
*                                      /_/                                   *
*  ------------------------------------------------------------------------  *
*  This file is a part of Storiq One                                         *
*                                                                            *
*  Storiq One is free software; you can redistribute it and/or modify        *
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
*  Copyright (C) 2013-2015, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#ifndef __STORIQONE_JOB_RESTORE_ARCHIVE_H__
#define __STORIQONE_JOB_RESTORE_ARCHIVE_H__

// bool
#include <stdbool.h>
// ssize_t
#include <sys/types.h>

#include <libstoriqone/job.h>

struct so_archive;
struct so_archive_file;
struct so_database_config;
struct so_media;

struct soj_restorearchive_data_worker {
	struct so_archive * archive;
	struct so_media * media;

	struct so_database_config * db_config;

	volatile ssize_t total_restored;
	volatile enum so_job_status status;
	volatile bool stop_request;

	unsigned int nb_warnings;
	unsigned int nb_errors;

	struct soj_restorearchive_data_worker * next;
};

void soj_restorearchive_check_worker_add(struct so_archive * archive, struct so_archive_file * file);
void soj_restorearchive_check_worker_start(struct so_database_config * db_config);
void soj_restorearchive_check_worker_stop(void);

struct soj_restorearchive_data_worker * soj_restorearchive_data_worker_new(struct so_archive * archive, struct so_media * media, struct so_database_config * db_config, struct soj_restorearchive_data_worker * previous_worker);
void soj_restorearchive_data_worker_start(struct soj_restorearchive_data_worker * first_worker, struct so_job * job, struct so_database_connection * db_connect);

const char * soj_restorearchive_path_get(const char * path, const char * parent, bool is_regular_file);
void soj_restorearchive_path_init(const char * root);

#endif
