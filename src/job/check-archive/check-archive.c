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

// bindtextdomain, dgettext
#include <libintl.h>
// free
#include <stdlib.h>

#include <libstoriqone/archive.h>
#include <libstoriqone/database.h>
#include <libstoriqone/host.h>
#include <libstoriqone/json.h>
#include <libstoriqone/log.h>
#include <libstoriqone/media.h>
#include <libstoriqone/value.h>
#include <libstoriqone-job/job.h>
#include <libstoriqone-job/script.h>

#include <job_check-archive.chcksum>

#include "common.h"
#include "config.h"

static struct so_archive * archive = NULL;

static void soj_checkarchive_exit(struct so_job * job, struct so_database_connection * db_connect);
static void soj_checkarchive_init(void) __attribute__((constructor));
static int soj_checkarchive_run(struct so_job * job, struct so_database_connection * db_connect);
static int soj_checkarchive_simulate(struct so_job * job, struct so_database_connection * db_connect);
static void soj_checkarchive_script_on_error(struct so_job * job, struct so_database_connection * db_connect);
static void soj_checkarchive_script_post_run(struct so_job * job, struct so_database_connection * db_connect);
static bool soj_checkarchive_script_pre_run(struct so_job * job, struct so_database_connection * db_connect);

static struct so_job_driver soj_checkarchive = {
	.name = "check-archive",

	.exit            = soj_checkarchive_exit,
	.run             = soj_checkarchive_run,
	.simulate        = soj_checkarchive_simulate,
	.script_on_error = soj_checkarchive_script_on_error,
	.script_post_run = soj_checkarchive_script_post_run,
	.script_pre_run  = soj_checkarchive_script_pre_run,
};


static void soj_checkarchive_exit(struct so_job * job __attribute__((unused)), struct so_database_connection * db_connect __attribute__((unused))) {
	if (archive != NULL)
		so_archive_free(archive);
}

static void soj_checkarchive_init() {
	soj_job_register(&soj_checkarchive);

	bindtextdomain("storiqone-job-check-archive", LOCALE_DIR);
}

static int soj_checkarchive_run(struct so_job * job, struct so_database_connection * db_connect) {
	bool quick_mode = false;
	so_value_unpack(job->option, "{sb}", "quick_mode", &quick_mode);

	int failed;
	if (quick_mode)
		failed = soj_checkarchive_quick_mode(job, archive, db_connect);
	else
		failed = soj_checkarchive_thorough_mode(job, archive, db_connect);

	struct so_value * report = so_value_pack("{sososo}",
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"archive", so_archive_convert(archive)
	);
	char * json = so_json_encode_to_string(report);
	db_connect->ops->add_report(db_connect, job, archive, json);
	free(json);

	return failed;
}

static int soj_checkarchive_simulate(struct so_job * job, struct so_database_connection * db_connect) {
	archive = db_connect->ops->get_archive(db_connect, job);
	if (archive == NULL) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important, dgettext("storiqone-job-check-archive", "Archive not found"));
		return 1;
	}

	return 0;
}

static void soj_checkarchive_script_on_error(struct so_job * job, struct so_database_connection * db_connect) {
	struct so_pool * pool = archive->volumes->media->pool;

	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_on_error, pool) < 1)
		return;

	struct so_value * json = so_value_pack("{sososo}",
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"archive", so_archive_convert(archive)
	);

	struct so_value * returned = soj_script_run(db_connect, job, so_script_type_on_error, pool, json);
	so_value_free(json);
	so_value_free(returned);
}

static void soj_checkarchive_script_post_run(struct so_job * job, struct so_database_connection * db_connect) {
	struct so_pool * pool = archive->volumes->media->pool;

	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_post_job, pool) < 1)
		return;

	struct so_value * json = so_value_pack("{sososo}",
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"archive", so_archive_convert(archive)
	);

	struct so_value * returned = soj_script_run(db_connect, job, so_script_type_post_job, pool, json);
	so_value_free(json);
	so_value_free(returned);
}

static bool soj_checkarchive_script_pre_run(struct so_job * job, struct so_database_connection * db_connect) {
	struct so_pool * pool = archive->volumes->media->pool;

	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_pre_job, pool) < 1)
		return true;

	struct so_value * json = so_value_pack("{sososo}",
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"archive", so_archive_convert(archive)
	);

	struct so_value * returned = soj_script_run(db_connect, job, so_script_type_pre_job, pool, json);
	so_value_free(json);

	bool should_run = false;
	so_value_unpack(returned, "{sb}", "should run", &should_run);

	so_value_free(returned);

	return should_run;
}

