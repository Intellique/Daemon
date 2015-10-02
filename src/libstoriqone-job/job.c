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

#define _GNU_SOURCE
// va_end, va_start
#include <stdarg.h>
// vasprintf
#include <stdio.h>
// free
#include <stdlib.h>

#include <libstoriqone/database.h>
#include <libstoriqone/log.h>
#include <libstoriqone-job/job.h>

#include "job.h"

static struct so_job * soj_current_job = NULL;
static struct so_job_driver * soj_driver = NULL;


int soj_job_add_record(struct so_job * job, struct so_database_connection * db_connect, enum so_log_level level, enum so_job_record_notif notif, const char * format, ...) {
	char * message = NULL;

	va_list va;
	va_start(va, format);
	int size = vasprintf(&message, format, va);
	va_end(va);

	if (size < 0)
		return -1;

	so_log_write(level, "%s", message);
	int failed = db_connect->ops->add_job_record(db_connect, job, level, notif, message);

	free(message);

	return failed;
}

struct so_job * soj_job_get() {
	return soj_current_job;
}

struct so_job_driver * soj_job_get_driver() {
	return soj_driver;
}

void soj_job_register(struct so_job_driver * driver) {
	soj_driver = driver;
}

void soj_job_set(struct so_job * current_job) {
	soj_current_job = current_job;
}

