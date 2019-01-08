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
*  Copyright (C) 2013-2019, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#ifndef __LIBSTORIQONE_JOB_H__
#define __LIBSTORIQONE_JOB_H__

// bool
#include <stdbool.h>
// time_t
#include <sys/time.h>

struct so_database_connection;
enum so_log_level;

enum so_job_status {
	so_job_status_disable = 0x1,
	so_job_status_error = 0x2,
	so_job_status_finished = 0x3,
	so_job_status_pause = 0x4,
	so_job_status_running = 0x5,
	so_job_status_scheduled = 0x6,
	so_job_status_stopped = 0x7,
	so_job_status_waiting = 0x8,

	so_job_status_unknown = 0x0,
};

enum so_job_record_notif {
	so_job_record_notif_normal = 0x1,
	so_job_record_notif_important = 0x2,
	so_job_record_notif_read = 0x3,

	so_job_record_notif_unknown = 0x0,
};

enum so_job_run_step {
	so_job_run_step_job = 0x1,
	so_job_run_step_on_error = 0x2,
	so_job_run_step_post_job = 0x3,
	so_job_run_step_pre_job = 0x4,
	so_job_run_step_warm_up = 0x5,

	so_job_run_step_unknown = 0x0,
};

struct so_job {
	char * id;
	char * name;
	char * type;

	time_t next_start;
	long interval;
	long repetition;

	long num_runs;
	enum so_job_run_step step;
	volatile float done;
	volatile enum so_job_status status;

	int exit_code;
	volatile bool stopped_by_user;

	// login
	char * user;
	struct so_value * meta;
	struct so_value * option;

	void * data;
	struct so_value * db_data;
};

struct so_value * so_job_convert(struct so_job * job) __attribute__((warn_unused_result));
void so_job_free(struct so_job * job);
void so_job_free2(void * job);
const char * so_job_report_notif_to_string(enum so_job_record_notif notif, bool translate);
const char * so_job_run_step_to_string(enum so_job_run_step step, bool translate);
enum so_job_run_step so_job_run_string_to_step(const char * step, bool translate);
const char * so_job_status_to_string(enum so_job_status status, bool translate);
enum so_job_record_notif so_job_string_to_record_notif(const char * notif, bool translate);
enum so_job_status so_job_string_to_status(const char * status, bool translate);
void so_job_sync(struct so_job * job, struct so_value * new_job);

#endif
