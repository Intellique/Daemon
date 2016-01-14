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
*  Copyright (C) 2013-2016, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#ifndef __LIBSTORIQONE_JOB_JOB_H__
#define __LIBSTORIQONE_JOB_JOB_H__

#include <libstoriqone/job.h>

struct so_database_connection;
struct so_job;

struct so_job_driver {
	char * name;

	void (*exit)(struct so_job * job, struct so_database_connection * db_connect);
	int (*run)(struct so_job * job, struct so_database_connection * db_connect);
	int (*simulate)(struct so_job * job, struct so_database_connection * db_connect);
	void (*script_on_error)(struct so_job * job, struct so_database_connection * db_connect);
	void (*script_post_run)(struct so_job * job, struct so_database_connection * db_connect);
	bool (*script_pre_run)(struct so_job * job, struct so_database_connection * db_connect);
};

int soj_job_add_record(struct so_job * job, struct so_database_connection * db_connect, enum so_log_level level, enum so_job_record_notif notif, const char * format, ...) __attribute__ ((format(printf, 5, 6)));
struct so_job * soj_job_get(void);
void soj_job_register(struct so_job_driver * driver);

#endif

