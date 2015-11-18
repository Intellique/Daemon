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
// bindtextdomain, dgettext
#include <libintl.h>
// bool
#include <stdbool.h>
// asprintf
#include <stdio.h>
// free
#include <stdlib.h>
// sleep
#include <unistd.h>

#include <libstoriqone/database.h>
#include <libstoriqone/host.h>
#include <libstoriqone/log.h>
#include <libstoriqone/media.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/thread_pool.h>
#include <libstoriqone/value.h>
#include <libstoriqone-job/backup.h>
#include <libstoriqone-job/changer.h>
#include <libstoriqone-job/job.h>
#include <libstoriqone-job/script.h>

#include <job_check-backup-db.chcksum>

#include "config.h"
#include "common.h"

static struct so_backup * soj_checkbackupdb_backup = NULL;
static size_t soj_checkbackupdb_backup_size = 0;
static struct so_pool * soj_checkbackupdb_pool = NULL;

static void soj_checkbackupdb_exit(struct so_job * job, struct so_database_connection * db_connect);
static void soj_checkbackupdb_init(void) __attribute__((constructor));
static int soj_checkbackupdb_run(struct so_job * job, struct so_database_connection * db_connect);
static int soj_checkbackupdb_simulate(struct so_job * job, struct so_database_connection * db_connect);
static void soj_checkbackupdb_script_on_error(struct so_job * job, struct so_database_connection * db_connect);
static void soj_checkbackupdb_script_post_run(struct so_job * job, struct so_database_connection * db_connect);
static bool soj_checkbackupdb_script_pre_run(struct so_job * job, struct so_database_connection * db_connect);

static struct so_job_driver checkbackupdb = {
	.name = "check-backup-db",

	.exit            = soj_checkbackupdb_exit,
	.run             = soj_checkbackupdb_run,
	.simulate        = soj_checkbackupdb_simulate,
	.script_on_error = soj_checkbackupdb_script_on_error,
	.script_post_run = soj_checkbackupdb_script_post_run,
	.script_pre_run  = soj_checkbackupdb_script_pre_run,
};


static void soj_checkbackupdb_exit(struct so_job * job __attribute__((unused)), struct so_database_connection * db_connect __attribute__((unused))) {
	soj_backup_free(soj_checkbackupdb_backup);
}

static void soj_checkbackupdb_init() {
	soj_job_register(&checkbackupdb);

	bindtextdomain("storiqone-job-check-backup-db", LOCALE_DIR);
}

static int soj_checkbackupdb_run(struct so_job * job, struct so_database_connection * db_connect) {
	unsigned int i;
	struct soj_checkbackupdb_worker * worker = NULL, * ptr = NULL;
	for (i = 0; i < soj_checkbackupdb_backup->nb_volumes; i++) {
		struct so_backup_volume * vol = soj_checkbackupdb_backup->volumes + i;

		ptr = soj_checkbackupdb_worker_new(soj_checkbackupdb_backup, vol, soj_checkbackupdb_backup_size, db_connect->config, ptr);
		if (worker == NULL)
			worker = ptr;
	}

	for (ptr = worker, i = 0; ptr != NULL; ptr = ptr->next, i++) {
		char * name = NULL;
		int size = asprintf(&name, "worker: vol #%u", i);

		if (size < 0) {
			so_log_write(so_log_level_error,
				dgettext("storiqone-job-check-backup-db", "Failed while setting worker thread name"));

			free(name);
			name = "worker";
		}

		so_thread_pool_run(name, soj_checkbackupdb_worker_do, ptr);

		if (size >= 0)
			free(name);
	}

	job->done = 0.01;

	sleep(1);

	bool stop = false;
	while (!stop) {
		bool is_running = false;
		float done = 0;

		for (ptr = worker; ptr != NULL; ptr = ptr->next) {
			is_running |= ptr->working;
			done += ((float) ptr->position) / ptr->size;
		}

		job->done = 0.01 + done * 0.98;
		stop = !is_running;

		if (is_running)
			sleep(1);
	}

	job->done = 0.99;

	if (!job->stopped_by_user) {
		unsigned int i;
		bool checksum_ok = true;
		for (ptr = worker, i = 0; ptr != NULL; ptr = ptr->next, i++) {
			if (ptr->volume->checksum_ok)
				soj_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_important,
					dgettext("storiqone-job-check-backup-db", "Checking backup volume #%u: success"), i);
			else {
				soj_job_add_record(job, db_connect, so_log_level_warning, so_job_record_notif_important,
					dgettext("storiqone-job-check-backup-db", "Checking backup volume #%u: failed"), i);
				checksum_ok = false;
			}

			db_connect->ops->mark_backup_volume_checked(db_connect, ptr->volume);
		}

		if (checksum_ok)
			soj_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_important,
				dgettext("storiqone-job-check-backup-db", "Checking backup: success"));
		else
			soj_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_important,
				dgettext("storiqone-job-check-backup-db", "Checking backup: failed"));
	}

	soj_checkbackupdb_worker_free(worker);

	job->done = 1;

	return 0;
}

static int soj_checkbackupdb_simulate(struct so_job * job, struct so_database_connection * db_connect) {
	soj_checkbackupdb_backup = db_connect->ops->get_backup(db_connect, job);
	if (soj_checkbackupdb_backup == NULL) {
		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-check-backup-db", "Backup not found"));
		return 1;
	}

	unsigned int i;
	for (i = 0; i < soj_checkbackupdb_backup->nb_volumes; i++) {
		struct so_backup_volume * vol = soj_checkbackupdb_backup->volumes + i;
		soj_checkbackupdb_backup_size += vol->size;

		if (soj_checkbackupdb_pool == NULL)
			soj_checkbackupdb_pool = vol->media->pool;

		if (vol->media == NULL) {
			soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
				dgettext("storiqone-job-check-backup-db", "BUG: media should not be empty"));
			return 1;
		}
		if (vol->media->status == so_media_status_error)
			soj_job_add_record(job, db_connect, so_log_level_warning, so_job_record_notif_important,
				dgettext("storiqone-job-check-backup-db", "Try to read a media with error status"));
	}

	return 0;
}

static void soj_checkbackupdb_script_on_error(struct so_job * job, struct so_database_connection * db_connect) {
	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_on_error, soj_checkbackupdb_pool) < 1)
		return;

	struct so_value * json = so_value_pack("{sosososo}",
		"backup", soj_backup_convert(soj_checkbackupdb_backup),
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"pool", so_pool_convert(soj_checkbackupdb_pool)
	);

	struct so_value * returned = soj_script_run(db_connect, job, so_script_type_on_error, soj_checkbackupdb_pool, json);
	so_value_free(json);
	so_value_free(returned);
}

static void soj_checkbackupdb_script_post_run(struct so_job * job, struct so_database_connection * db_connect) {
	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_on_error, soj_checkbackupdb_pool) < 1)
		return;

	struct so_value * json = so_value_pack("{sosososo}",
		"backup", soj_backup_convert(soj_checkbackupdb_backup),
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"pool", so_pool_convert(soj_checkbackupdb_pool)
	);

	struct so_value * returned = soj_script_run(db_connect, job, so_script_type_post_job, soj_checkbackupdb_pool, json);
	so_value_free(json);
	so_value_free(returned);
}

static bool soj_checkbackupdb_script_pre_run(struct so_job * job, struct so_database_connection * db_connect) {
	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_pre_job, soj_checkbackupdb_pool) < 1)
		return true;

	struct so_value * json = so_value_pack("{sosososo}",
		"backup", soj_backup_convert(soj_checkbackupdb_backup),
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"pool", so_pool_convert(soj_checkbackupdb_pool)
	);

	struct so_value * returned = soj_script_run(db_connect, job, so_script_type_pre_job, soj_checkbackupdb_pool, json);
	so_value_free(json);

	bool should_run = false;
	so_value_unpack(returned, "{sb}", "should run", &should_run);
	so_value_free(returned);

	return true;
}

