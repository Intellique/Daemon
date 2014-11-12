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
*  Copyright (C) 2014, Clercin guillaume <gclercin@intellique.com>           *
\****************************************************************************/

// free
#include <stdlib.h>

#include <libstoriqone/json.h>
#include <libstoriqone/database.h>
#include <libstoriqone/log.h>
#include <libstoriqone/process.h>
#include <libstoriqone/value.h>
#include <libstoriqone-job/script.h>

#include "job.h"

struct so_value * soj_script_run(struct so_database_connection * db_connect, struct so_job * job, enum so_script_type type, struct so_pool * pool, struct so_value * data) {
	int nb_scripts = db_connect->ops->get_nb_scripts(db_connect, job->type, type, pool);
	so_log_write(so_log_level_info, "script found: %d", nb_scripts);
	if (nb_scripts < 0)
		return NULL;

	struct so_value * datas = so_value_new_array(nb_scripts);

	int i, status = 0;
	bool should_run = false;
	for (i = 0; i < nb_scripts && status == 0; i++) {
		char * path = db_connect->ops->get_script(db_connect, job->type, i, type, pool);
		if (path == NULL) {
			so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important, "script has not found into database, %s, %d/%d", so_script_type_to_string(type, false), i, nb_scripts);
			status = 1;
			break;
		}

		so_job_add_record(job, db_connect, so_log_level_debug, so_job_record_notif_normal, "running %s script: %s", so_script_type_to_string(type, false), path);

		// run command & wait result
		struct so_process process;
		const char * params[] = { so_script_type_to_string(type, false) };
		so_process_new(&process, path, params, 1);

		int fd_write = so_process_pipe_to(&process);
		int fd_read = so_process_pipe_from(&process, so_process_stdout);

		so_process_start(&process, 1);
		so_json_encode_to_fd(data, fd_write, true);

		struct so_value * returned = so_json_parse_fd(fd_read, -1);

		so_process_wait(&process, 1);
		status = process.exited_code;
		so_process_free(&process, 1);

		if (status != 0)
			so_job_add_record(job, db_connect, so_log_level_warning, so_job_record_notif_important, "script %s has exited with code %d", path, status);
		else
			so_job_add_record(job, db_connect, so_log_level_notice, so_job_record_notif_important, "script %s has exited with code OK", path);

		if (returned == NULL) {
			so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important, "job '%s' has not returned data", path);
			status = 1;
			free(path);
			break;
		}

		if (type == so_script_type_pre_job) {
			so_value_unpack(returned, "{sb}", "should run", &should_run);

			if (!should_run) {
				so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important, "job '%s' has not returned data 'should run'", path);
				status = 1;
				free(path);
				break;
			}
		}

		struct so_value * data = NULL;
		so_value_unpack(returned, "{sO}", "data", &data);
		if (data != NULL)
			so_value_list_push(datas, data, true);

		char * message = NULL;
		so_value_unpack(returned, "{ss}", "message", &message);
		if (message != NULL) {
			so_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_important, "script '%s' return message: %s", path, message);
			free(message);
		}

		if (type == so_script_type_pre_job && !should_run)
			status = 1;

		free(path);
	}

	return so_value_pack("{sisosb}", "status", (long int) status, "datas", datas, "should run", should_run);
}
