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
// NULL
#include <stddef.h>

#include <libstoriqone/archive.h>
#include <libstoriqone/database.h>
#include <libstoriqone-job/job.h>

#include <job_check-archive.chcksum>

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


static void soj_checkarchive_exit(struct so_job * job, struct so_database_connection * db_connect) {
}

static void soj_checkarchive_init() {
	soj_job_register(&soj_checkarchive);

	bindtextdomain("storiqone-job-check-archive", LOCALE_DIR);
}

static int soj_checkarchive_run(struct so_job * job, struct so_database_connection * db_connect) {
	return 0;
}

static int soj_checkarchive_simulate(struct so_job * job, struct so_database_connection * db_connect) {
	archive = db_connect->ops->get_archive(db_connect, job);
	if (archive == NULL) {
		return 1;
	}
}

static void soj_checkarchive_script_on_error(struct so_job * job, struct so_database_connection * db_connect) {
}

static void soj_checkarchive_script_post_run(struct so_job * job, struct so_database_connection * db_connect) {
}

static bool soj_checkarchive_script_pre_run(struct so_job * job, struct so_database_connection * db_connect) {
	return true;
}

