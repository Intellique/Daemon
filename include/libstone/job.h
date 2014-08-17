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
*  Copyright (C) 2014, Clercin guillaume <gclercin@intellique.com>           *
\****************************************************************************/

#ifndef __LIBSTONE_JOB_H__
#define __LIBSTONE_JOB_H__

// bool
#include <stdbool.h>
// time_t
#include <sys/time.h>

struct st_database_connection;
enum st_log_level;

enum st_job_status {
	st_job_status_disable = 0x1,
	st_job_status_error = 0x2,
	st_job_status_finished = 0x3,
	st_job_status_pause = 0x4,
	st_job_status_running = 0x5,
	st_job_status_scheduled = 0x6,
	st_job_status_stopped = 0x7,
	st_job_status_waiting = 0x8,

	st_job_status_unknown = 0x0,
};

enum st_job_record_notif {
	st_job_record_notif_normal = 0x1,
	st_job_record_notif_important = 0x2,
	st_job_record_notif_read = 0x3,

	st_job_record_notif_unknown = 0x0,
};

struct st_job {
	char * key;
	char * name;
	char * type;

	time_t next_start;
	long interval;
	long repetition;

	long num_runs;
	volatile float done;
	volatile enum st_job_status status;

	int exit_code;
	volatile bool stopped_by_user;

	struct st_value * meta;
	struct st_value * option;

	void * data;
	struct st_value * db_data;
};

int st_job_add_record(struct st_job * job, struct st_database_connection * db_connect, enum st_log_level level, enum st_job_record_notif notif, const char * format, ...) __attribute__ ((nonnull(1,2),format(printf, 5, 6)));
struct st_value * st_job_convert(struct st_job * job) __attribute__((nonnull,warn_unused_result));
void st_job_free(struct st_job * job);
void st_job_free2(void * job);
const char * st_job_report_notif_to_string(enum st_job_record_notif notif);
const char * st_job_status_to_string(enum st_job_status status);
enum st_job_record_notif st_job_string_to_record_notif(const char * notif) __attribute__((nonnull));
enum st_job_status st_job_string_to_status(const char * status) __attribute__((nonnull));
void st_job_sync(struct st_job * job, struct st_value * new_job) __attribute__((nonnull));

#endif

