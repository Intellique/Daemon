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
*  Copyright (C) 2013-2018, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#ifndef __STORIQONE_JOB_CREATE_ARCHIVE_WORKER_H__
#define __STORIQONE_JOB_CREATE_ARCHIVE_WORKER_H__

// bool
#include <stdbool.h>
// ssize_t
#include <sys/types.h>

struct so_archive;
struct so_database_connection;
struct so_format_file;
struct so_pool;
struct so_value;

struct so_value * soj_create_archive_worker_archives(void);
enum so_format_writer_status soj_create_archive_worker_add_file(struct so_format_file * file, const char * selected_path, struct so_database_connection * db_connect);
int soj_create_archive_worker_close(struct so_database_connection * db_connect);
int soj_create_archive_worker_create_check_archive(struct so_value * option, struct so_database_connection * db_connect);
ssize_t soj_create_archive_worker_end_of_file(void);
bool soj_create_archive_worker_finished(void);
void soj_create_archive_worker_generate_report(struct so_value * selected_path, struct so_database_connection * db_connect);
void soj_create_archive_worker_init_archive(struct so_job * job, struct so_archive * primary_archive, struct so_value * mirrors);
void soj_create_archive_worker_init_pool(struct so_job * job, struct so_pool * primary_pool, struct so_value * mirrors);
void soj_create_archive_worker_prepare_medias(struct so_database_connection * db_connect);
void soj_create_archive_worker_prepare_medias2(struct so_database_connection * db_connect);
float soj_create_archive_progress(void);
void soj_create_archive_worker_reserve_medias(ssize_t archive_size, struct so_database_connection * db_connect);
int soj_create_archive_worker_sync_archives(bool first_synchro, bool close_archive, struct so_archive * archive, struct so_database_connection * db_connect);
ssize_t soj_create_archive_worker_write(struct so_format_file * file, const char * buffer, ssize_t length, struct so_database_connection * db_connect);

#endif
