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
*  Last modified: Thu, 05 Apr 2012 17:03:02 +0200                         *
\*************************************************************************/

#ifndef __STONE_JOB_SAVE_COMMON_H__
#define __STONE_JOB_SAVE_COMMON_H__

#include <pthread.h>
// ssize_t
#include <sys/types.h>

struct st_job;

struct st_job_save_private {
	struct st_tar_out * tar;
	char * buffer;
	ssize_t block_size;

	struct st_job_save_savepoint {
		struct st_changer * changer;
		struct st_drive * drive;
		struct st_slot * slot;
	} * savepoints;
	unsigned int nb_savepoints;

	struct st_drive * current_drive;
	struct st_archive_volume * current_volume;

	struct st_stream_writer * current_tape_writer;
	struct st_stream_writer * current_checksum_writer;

	struct st_tape ** tapes;
	unsigned int nb_tapes;

	ssize_t total_size_done;
	ssize_t total_size;

	struct st_io_json * json;

	struct st_database_connection * db_con;

    // for slave
    volatile short run;
    struct st_archive_files {
        struct st_archive_file * file;
        struct st_archive_files * next;
    } * files_first, * files_last;
    pthread_mutex_t lock;
    pthread_cond_t wait;
};

void st_job_save_add_file(struct st_job_save_private * jp, struct st_archive_file * file);
void st_job_save_start_slave(struct st_job * job);

#endif

