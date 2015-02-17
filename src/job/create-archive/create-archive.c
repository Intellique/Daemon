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

#include <stddef.h>

// bindtextdomain, dgettext
#include <libintl.h>

#include <libstoriqone/database.h>
#include <libstoriqone/file.h>
#include <libstoriqone/log.h>
#include <libstoriqone/media.h>
#include <libstoriqone/value.h>
#include <libstoriqone-job/job.h>

#include <job_create-archive.chcksum>

#include "config.h"


static struct so_media * media_source = NULL;
static struct so_value * selected_path = NULL;
static struct so_pool * primary_pool = NULL;

static void soj_create_archive_exit(struct so_job * job, struct so_database_connection * db_connect);
static void soj_create_archive_init(void) __attribute__((constructor));
static int soj_create_archive_run(struct so_job * job, struct so_database_connection * db_connect);
static int soj_create_archive_simulate(struct so_job * job, struct so_database_connection * db_connect);
static void soj_create_archive_script_on_error(struct so_job * job, struct so_database_connection * db_connect);
static void soj_create_archive_script_post_run(struct so_job * job, struct so_database_connection * db_connect);
static bool soj_create_archive_script_pre_run(struct so_job * job, struct so_database_connection * db_connect);

static struct so_job_driver create_archive = {
	.name = "backup-db",

	.exit            = soj_create_archive_exit,
	.run             = soj_create_archive_run,
	.simulate        = soj_create_archive_simulate,
	.script_on_error = soj_create_archive_script_on_error,
	.script_post_run = soj_create_archive_script_post_run,
	.script_pre_run  = soj_create_archive_script_pre_run,
};


static void soj_create_archive_exit(struct so_job * job, struct so_database_connection * db_connect) {
	if (media_source != NULL)
		so_media_free(media_source);
	so_value_free(selected_path);
	if (primary_pool != NULL)
		so_pool_free(primary_pool);
}

static void soj_create_archive_init() {
	soj_job_register(&create_archive);

	bindtextdomain("storiqone-job-create-archive", LOCALE_DIR);
}

static int soj_create_archive_run(struct so_job * job, struct so_database_connection * db_connect) {
}

static int soj_create_archive_simulate(struct so_job * job, struct so_database_connection * db_connect) {
	/**
	 * reserved for future (see ltfs)
	 * media_source = db_connect->ops->get_media(db_connect, NULL, NULL, job);
	**/

	selected_path = db_connect->ops->get_selected_files_by_job(db_connect, job);
	if (selected_path == NULL || so_value_list_get_length(selected_path) == 0) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important, dgettext("storiqone-job-create-archive", "There is no select files to create new archive or to add into an archive"));
		return 1;
	}

	ssize_t total_size = 0;
	struct so_value_iterator * iter = so_value_list_get_iterator(selected_path);
	while (so_value_iterator_has_next(iter)) {
		struct so_value * elt = so_value_iterator_get_value(iter, false);
		ssize_t sub_total = so_file_compute_size(so_value_string_get(elt), true);
		if (sub_total < 0)
			break;
		else
			total_size += sub_total;
	}
	so_value_iterator_free(iter);


	primary_pool = db_connect->ops->get_pool(db_connect, NULL, job);
	// TODO: primary_pool IS NULL -> adding file to archive
	
	if (primary_pool->deleted) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important, dgettext("storiqone-job-create-archive", "Try to create an archive into a pool which is deleted"));
		return 1;
	}

	struct so_value * pool_mirrors = db_connect->ops->get_pool_by_pool_mirror(db_connect, primary_pool);
}

static void soj_create_archive_script_on_error(struct so_job * job, struct so_database_connection * db_connect) {
}

static void soj_create_archive_script_post_run(struct so_job * job, struct so_database_connection * db_connect) {
}

static bool soj_create_archive_script_pre_run(struct so_job * job, struct so_database_connection * db_connect) {
}

