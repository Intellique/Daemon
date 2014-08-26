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

// free
#include <stdlib.h>

#include <libstone/json.h>
#include <libstone/database.h>
#include <libstone/log.h>
#include <libstone/process.h>
#include <libstone/value.h>

#include "job.h"
#include "script.h"

__asm__(".symver stj_script_run_v1, stj_script_run@@LIBSTONE_JOB_1.2");
struct st_value * stj_script_run_v1(struct st_database_connection * db_connect, struct st_job * job, enum st_script_type type, struct st_pool * pool, struct st_value * data) {
	int nb_scripts = db_connect->ops->get_nb_scripts(db_connect, job->type, type, pool);
	st_log_write(st_log_level_info, "script found: %d", nb_scripts);
	if (nb_scripts < 0)
		return NULL;

	struct st_value * datas = st_value_new_array(nb_scripts);

	int i, status = 0;
	bool should_run = false;
	for (i = 0; i < nb_scripts && status == 0; i++) {
		char * path = db_connect->ops->get_script(db_connect, job->type, i, type, pool);
		if (path == NULL) {
			st_job_add_record(job, db_connect, st_log_level_error, st_job_record_notif_important, "script has not found into database, %s, %d/%d", st_script_type_to_string(type), i, nb_scripts);
			status = 1;
			break;
		}

		st_job_add_record(job, db_connect, st_log_level_debug, st_job_record_notif_normal, "running %s script: %s", st_script_type_to_string(type), path);

		// run command & wait result
		struct st_process process;
		const char * params[] = { st_script_type_to_string(type) };
		st_process_new(&process, path, params, 1);

		int fd_write = st_process_pipe_to(&process);
		int fd_read = st_process_pipe_from(&process, st_process_stdout);

		st_process_start(&process, 1);
		st_json_encode_to_fd(data, fd_write, true);

		struct st_value * returned = st_json_parse_fd(fd_read, -1);

		st_process_wait(&process, 1);
		status = process.exited_code;
		st_process_free(&process, 1);

		if (status != 0)
			st_job_add_record(job, db_connect, st_log_level_warning, st_job_record_notif_important, "script %s has exited with code %d", path, status);
		else
			st_job_add_record(job, db_connect, st_log_level_notice, st_job_record_notif_important, "script %s has exited with code OK", path);

		if (returned == NULL) {
			st_job_add_record(job, db_connect, st_log_level_error, st_job_record_notif_important, "job '%s' has not returned data", path);
			status = 1;
			free(path);
			break;
		}

		if (type == st_script_type_pre_job) {
			st_value_unpack(returned, "{sb}", "should run", &should_run);

			if (!should_run) {
				st_job_add_record(job, db_connect, st_log_level_error, st_job_record_notif_important, "job '%s' has not returned data 'should run'", path);
				status = 1;
				free(path);
				break;
			}
		}

		struct st_value * data = NULL;
		st_value_unpack(returned, "{sO}", "data", &data);
		if (data != NULL)
			st_value_list_push(datas, data, true);

		char * message = NULL;
		st_value_unpack(returned, "{ss}", "message", &message);
		if (message != NULL) {
			st_job_add_record(job, db_connect, st_log_level_info, st_job_record_notif_important, "script '%s' return message: %s", path, message);
			free(message);
		}

		if (type == st_script_type_pre_job && !should_run)
			status = 1;

		free(path);
	}

	return st_value_pack("{sisosb}", "status", (long int) status, "datas", datas, "should run", should_run);
}

