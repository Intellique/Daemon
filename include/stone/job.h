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
*  Last modified: Thu, 16 Aug 2012 13:18:26 +0200                         *
\*************************************************************************/

#ifndef __STONE_JOB_H__
#define __STONE_JOB_H__

// time_t
#include <sys/time.h>
// ssize_t
#include <sys/types.h>

#include "database.h"

struct st_archive;
struct st_backupdb;
struct st_job_driver;
enum st_log_level;
struct st_pool;
struct st_tape;
struct st_user;

enum st_job_status {
	st_job_status_disable,
	st_job_status_error,
	st_job_status_idle,
	st_job_status_pause,
	st_job_status_running,
	st_job_status_stopped,
	st_job_status_waiting,

	st_job_status_unknown,
};

struct st_job {
	// database
	long id;
	char * name;
	enum st_job_status db_status;
	time_t updated;

	// scheduler
	enum st_job_status sched_status;
	time_t start;
	long interval;
	long repetition;
	long num_runs;
	struct st_scheduler_ops {
		int (*add_record)(struct st_job * j, enum st_log_level level, const char * format, ...) __attribute__ ((format (printf, 3, 4)));
		int (*update_status)(struct st_job * j);
	} * db_ops;
	void * scheduler_private;

	// job
	float done;
	struct st_archive * archive;
	struct st_backupdb * backup;
	struct st_tape * tape;
	struct st_pool * pool;

	char ** paths;
	unsigned int nb_paths;

	char ** checksums;
	long * checksum_ids;
	unsigned int nb_checksums;

	struct st_job_block_number {
		long sequence;
		struct st_tape * tape;
		long tape_position;
		ssize_t block_number;
		ssize_t size;
	} * block_numbers;
	unsigned int nb_block_numbers;

	struct st_job_restore_to {
		char * path;
		int nb_trunc_path;
	} * restore_to;

	struct st_user * user;

	struct st_hashtable * job_meta;
	struct st_hashtable * job_option;

	struct st_job_ops {
		void (*free)(struct st_job * j);
		int (*run)(struct st_job * j);
		int (*stop)(struct st_job * j);
	} * job_ops;
	void * data;

	struct st_job_driver * driver;
};

struct st_job_driver {
	char * name;
	void (*new_job)(struct st_database_connection * db, struct st_job * job);
	void * cookie;
	int api_version;
};

#define STONE_JOB_APIVERSION 1

struct st_job_driver * st_job_get_driver(const char * driver);
void st_job_register_driver(struct st_job_driver * driver);
const char * st_job_status_to_string(enum st_job_status status);
enum st_job_status st_job_string_to_status(const char * status);
void st_job_sync_plugins(void);

#endif

