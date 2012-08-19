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
*  Last modified: Sun, 19 Aug 2012 12:46:40 +0200                         *
\*************************************************************************/

#ifndef __STONE_JOB_H__
#define __STONE_JOB_H__

// time_t
#include <sys/time.h>
// ssize_t
#include <sys/types.h>

struct st_database_connection;
struct st_job_driver;
enum st_log_level;
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
	char * name;
	time_t next_start;
	long interval;
	long repetition;
	long num_runs;

	float done;
	volatile enum st_job_status db_status;
	enum st_job_status sched_status;
	time_t updated;

	struct st_user * user;

	struct st_hashtable * meta;
	struct st_hashtable * option;

	struct st_job_ops {
		short (*check)(struct st_job * j);
		void (*free)(struct st_job * j);
		int (*run)(struct st_job * j);
	} * ops;
	void * data;

	void * db_data;

	struct st_job_driver * driver;
};

struct st_job_driver {
	char * name;
	void (*new_job)(struct st_job * job, struct st_database_connection * db);
	void * cookie;
	int api_version;
};

#define STONE_JOB_API_LEVEL 1

void st_job_add_record(struct st_database_connection * connect, enum st_log_level level, struct st_job * job, const char * message, ...) __attribute__ ((format (printf, 4, 5)));
struct st_job_driver * st_job_get_driver(const char * driver);
void st_job_register_driver(struct st_job_driver * driver);
const char * st_job_status_to_string(enum st_job_status status);
enum st_job_status st_job_string_to_status(const char * status);
void st_job_sync_plugins(struct st_database_connection * connection);

#endif

