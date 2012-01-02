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
*  Copyright (C) 2011, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Mon, 02 Jan 2012 23:21:45 +0100                         *
\*************************************************************************/

#ifndef __STONE_JOB_H__
#define __STONE_JOB_H__

// time_t
#include <sys/time.h>

#include "database.h"

struct st_job_driver;

enum st_job_status {
	st_job_status_disable,
	st_job_status_error,
	st_job_status_idle,
	st_job_status_pause,
	st_job_status_running,

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
	struct st_scheduler_ops {
		int (*update_status)(struct st_job * j);
	} * db_ops;
	void * scheduler_private;

	// job
	float done;
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

#endif

