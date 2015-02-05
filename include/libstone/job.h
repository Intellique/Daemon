/****************************************************************************\
*                             __________                                     *
*                            / __/_  __/__  ___  ___                         *
*                           _\ \  / / / _ \/ _ \/ -_)                        *
*                          /___/ /_/  \___/_//_/\__/                         *
*                                                                            *
*  ------------------------------------------------------------------------  *
*  This file is a part of STone                                              *
*                                                                            *
*  STone is free software; you can redistribute it and/or modify             *
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
*  Copyright (C) 2013-2015, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Thu, 23 Jan 2014 12:58:04 +0100                            *
\****************************************************************************/

#ifndef __STONE_JOB_H__
#define __STONE_JOB_H__

// bool
#include <stdbool.h>
// time_t
#include <sys/time.h>

#include "plugin.h"

struct st_database_config;
struct st_database_connection;
struct st_job_driver;
enum st_log_level;
struct st_user;

enum st_job_status {
	st_job_status_disable,
	st_job_status_error,
	st_job_status_finished,
	st_job_status_pause,
	st_job_status_running,
	st_job_status_scheduled,
	st_job_status_stopped,
	st_job_status_waiting,

	st_job_status_unknown,
};

enum st_job_record_notif {
	st_job_record_notif_normal,
	st_job_record_notif_important,
	st_job_record_notif_read,

	st_job_record_notif_unknown,
};

struct st_job {
	char * name;

	time_t next_start;
	long interval;
	long repetition;
	long num_runs;

	float done;
	volatile enum st_job_status db_status;
	volatile enum st_job_status sched_status;
	bool stoped_by_user;
	time_t updated;

	struct st_user * user;

	char * meta;
	struct st_hashtable * option;

	struct st_job_ops {
		bool (*check)(struct st_job * j);
		void (*free)(struct st_job * j);
		void (*on_error)(struct st_job * j);
		void (*post_run)(struct st_job * j);
		bool (*pre_run)(struct st_job *j);
		int (*run)(struct st_job * j);
	} * ops;
	void * data;

	struct st_database_config * db_config;
	struct st_database_connection * db_connect;
	void * db_data;

	struct st_job_driver * driver;
};

struct st_job_driver {
	const char * name;
	void (*new_job)(struct st_job * job);
	void * cookie;
	const struct st_plugin api_level;
	const char * src_checksum;
};

struct st_job_selected_path {
	char * path;
	void * db_data;
};

#define STONE_JOB_API_LEVEL 1

void st_job_add_record(struct st_database_connection * connect, enum st_log_level level, struct st_job * job, enum st_job_record_notif notif, const char * message, ...) __attribute__ ((format (printf, 5, 6)));
struct st_job_driver * st_job_get_driver(const char * driver);
void st_job_register_driver(struct st_job_driver * driver);
const char * st_job_status_to_string(enum st_job_status status);
enum st_job_status st_job_string_to_status(const char * status);
void st_job_sync_plugins(struct st_database_connection * connection);

#endif

