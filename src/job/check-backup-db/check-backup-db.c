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
*  Copyright (C) 2015, Guillaume Clercin <gclercin@intellique.com>           *
\****************************************************************************/

// bindtextdomain, dgettext
#include <libintl.h>
// free
#include <stdlib.h>

#include <libstoriqone/database.h>
#include <libstoriqone/log.h>
#include <libstoriqone/media.h>
#include <libstoriqone/slot.h>
#include <libstoriqone-job/backup.h>
#include <libstoriqone-job/changer.h>
#include <libstoriqone-job/job.h>
#include <libstoriqone-job/script.h>

#include <job_check-backup-db.chcksum>

#include "config.h"

struct so_backup * checkbackupdb_backup = NULL;
size_t checkbackupdb_backup_size = 0;
struct so_pool * checkbackupdb_pool = NULL;

static void checkbackupdb_exit(struct so_job * job, struct so_database_connection * db_connect);
static void checkbackupdb_init(void) __attribute__((constructor));
static int checkbackupdb_run(struct so_job * job, struct so_database_connection * db_connect);
static int checkbackupdb_simulate(struct so_job * job, struct so_database_connection * db_connect);
static void checkbackupdb_script_on_error(struct so_job * job, struct so_database_connection * db_connect);
static void checkbackupdb_script_post_run(struct so_job * job, struct so_database_connection * db_connect);
static bool checkbackupdb_script_pre_run(struct so_job * job, struct so_database_connection * db_connect);

static struct so_job_driver checkbackupdb = {
	.name = "check-backup-db",

	.exit            = checkbackupdb_exit,
	.run             = checkbackupdb_run,
	.simulate        = checkbackupdb_simulate,
	.script_on_error = checkbackupdb_script_on_error,
	.script_post_run = checkbackupdb_script_post_run,
	.script_pre_run  = checkbackupdb_script_pre_run,
};


static void checkbackupdb_exit(struct so_job * job __attribute__((unused)), struct so_database_connection * db_connect __attribute__((unused))) {
	soj_backup_free(checkbackupdb_backup);
}

static void checkbackupdb_init() {
	soj_job_register(&checkbackupdb);

	bindtextdomain("storiqone-job-check-backup-db", LOCALE_DIR);
}

static int checkbackupdb_run(struct so_job * job, struct so_database_connection * db_connect) {
	unsigned int i;
	for (i = 0; i < checkbackupdb_backup->nb_volumes; i++) {
		struct so_backup_volume * vol = checkbackupdb_backup->volumes + i;
		struct so_slot * sl = soj_changer_find_slot(vol->media);

		bool reserved = sl->changer->ops->reserve_media(sl->changer, sl);
		if (!reserved) {
		}
	}

	return 0;
}

static int checkbackupdb_simulate(struct so_job * job, struct so_database_connection * db_connect) {
	checkbackupdb_backup = db_connect->ops->backup_get(db_connect, job);
	if (checkbackupdb_backup == NULL) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important, dgettext("storiqone-job-check-backup-db", "Backup not found"));
		return 1;
	}

	unsigned int i;
	for (i = 0; i < checkbackupdb_backup->nb_volumes; i++) {
		struct so_backup_volume * vol = checkbackupdb_backup->volumes + i;
		checkbackupdb_backup_size += vol->size;

		if (checkbackupdb_pool == 0)
			checkbackupdb_pool = vol->media->pool;

		if (vol->media == NULL) {
			so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important, dgettext("storiqone-job-check-backup-db", "BUG: media should not be empty"));
			return 1;
		}
		if (vol->media->status == so_media_status_error)
			so_job_add_record(job, db_connect, so_log_level_warning, so_job_record_notif_important, dgettext("storiqone-job-check-backup-db", "Try to read a media with error status"));

		struct so_slot * sl = soj_changer_find_slot(vol->media);
		if (sl == NULL) {
			so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important, dgettext("storiqone-job-check-backup-db", "Media (%s) not found"), vol->media->name);
			return 1;
		}
	}

	return 0;
}

static void checkbackupdb_script_on_error(struct so_job * job, struct so_database_connection * db_connect) {
	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_on_error, checkbackupdb_pool) < 1)
		return;
}

static void checkbackupdb_script_post_run(struct so_job * job, struct so_database_connection * db_connect) {
	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_on_error, checkbackupdb_pool) < 1)
		return;
}

static bool checkbackupdb_script_pre_run(struct so_job * job, struct so_database_connection * db_connect) {
	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_pre_job, checkbackupdb_pool) < 1)
		return true;

	return true;
}

