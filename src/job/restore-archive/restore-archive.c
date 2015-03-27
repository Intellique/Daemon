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
// strcmp
#include <string.h>

#include <libstoriqone/archive.h>
#include <libstoriqone/database.h>
#include <libstoriqone/host.h>
#include <libstoriqone/log.h>
#include <libstoriqone/media.h>
#include <libstoriqone/value.h>
#include <libstoriqone-job/job.h>
#include <libstoriqone-job/script.h>

#include "common.h"

#include <job_restore-archive.chcksum>

#include "config.h"

static struct so_archive * archive = NULL;
static char * restore_path = NULL;

static void soj_restorearchive_exit(struct so_job * job, struct so_database_connection * db_connect);
static void soj_restorearchive_init(void) __attribute__((constructor));
static int soj_restorearchive_run(struct so_job * job, struct so_database_connection * db_connect);
static int soj_restorearchive_simulate(struct so_job * job, struct so_database_connection * db_connect);
static void soj_restorearchive_script_on_error(struct so_job * job, struct so_database_connection * db_connect);
static void soj_restorearchive_script_post_run(struct so_job * job, struct so_database_connection * db_connect);
static bool soj_restorearchive_script_pre_run(struct so_job * job, struct so_database_connection * db_connect);

static struct so_job_driver soj_restorearchive = {
	.name = "restore-archive",

	.exit            = soj_restorearchive_exit,
	.run             = soj_restorearchive_run,
	.simulate        = soj_restorearchive_simulate,
	.script_on_error = soj_restorearchive_script_on_error,
	.script_post_run = soj_restorearchive_script_post_run,
	.script_pre_run  = soj_restorearchive_script_pre_run,
};


static void soj_restorearchive_exit(struct so_job * job, struct so_database_connection * db_connect) {
}

static void soj_restorearchive_init() {
	soj_job_register(&soj_restorearchive);

	bindtextdomain("storiqone-job-restore-archive", LOCALE_DIR);
}

static int soj_restorearchive_run(struct so_job * job, struct so_database_connection * db_connect) {
	struct so_restorearchive_worker * workers = NULL, * last_worker = NULL;
	unsigned int i;
	for (i = 0; i < archive->nb_volumes; i++) {
		struct so_archive_volume * vol = archive->volumes + i;
		bool found = false;

		struct so_restorearchive_worker * ptr;
		for (ptr = workers; !found && ptr != NULL; ptr = ptr->next)
			if (strcmp(ptr->media->medium_serial_number, vol->media->medium_serial_number) == 0)
				found = true;

		if (!found) {
			last_worker = so_restorearchive_worker_new(archive, vol->media, db_connect->config, last_worker);

			if (workers == NULL)
				workers = last_worker;
		}
	}

	so_restorearchive_worker_start(workers, job, db_connect);

	return 0;
}

static int soj_restorearchive_simulate(struct so_job * job, struct so_database_connection * db_connect) {
	archive = db_connect->ops->get_archive(db_connect, job);
	if (archive == NULL) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important, dgettext("storiqone-job-storiqone-job-restore-archive", "Archive not found"));
		return 1;
	}

	restore_path = db_connect->ops->get_restore_path(db_connect, job);
	soj_restorearchive_path_init(restore_path);

	return 0;
}

static void soj_restorearchive_script_on_error(struct so_job * job, struct so_database_connection * db_connect) {
}

static void soj_restorearchive_script_post_run(struct so_job * job, struct so_database_connection * db_connect) {
}

static bool soj_restorearchive_script_pre_run(struct so_job * job, struct so_database_connection * db_connect) {
	struct so_pool * pool = archive->volumes->media->pool;

	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_pre_job, pool) < 1)
		return true;

	struct so_value * json = so_value_pack("{sosososs}",
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"archive", so_archive_convert(archive),
		"restore path", restore_path
	);

	struct so_value * returned = soj_script_run(db_connect, job, so_script_type_pre_job, pool, json);
	so_value_free(json);

	bool should_run = false;
	so_value_unpack(returned, "{sb}", "should run", &should_run);

	so_value_free(returned);

	return should_run;
}

