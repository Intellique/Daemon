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
#include <libstoriqone/host.h>
#include <libstoriqone/log.h>
#include <libstoriqone/media.h>
#include <libstoriqone/value.h>
#include <libstoriqone-job/changer.h>
#include <libstoriqone-job/drive.h>
#include <libstoriqone-job/job.h>
#include <libstoriqone-job/media.h>
#include <libstoriqone-job/script.h>

#include <job_import-archives.chcksum>

#include "config.h"

static struct so_value * soj_importarchives_archives = NULL;
static struct so_media * soj_importarchives_media = NULL;
static struct so_pool * soj_importarchives_pool = NULL;

static void soj_importarchives_exit(struct so_job * job, struct so_database_connection * db_connect);
static void soj_importarchives_init(void) __attribute__((constructor));
static int soj_importarchives_run(struct so_job * job, struct so_database_connection * db_connect);
static int soj_importarchives_simulate(struct so_job * job, struct so_database_connection * db_connect);
static void soj_importarchives_script_on_error(struct so_job * job, struct so_database_connection * db_connect);
static void soj_importarchives_script_post_run(struct so_job * job, struct so_database_connection * db_connect);
static bool soj_importarchives_script_pre_run(struct so_job * job, struct so_database_connection * db_connect);

static struct so_job_driver soj_importarchives = {
	.name = "import-archives",

	.exit            = soj_importarchives_exit,
	.run             = soj_importarchives_run,
	.simulate        = soj_importarchives_simulate,
	.script_on_error = soj_importarchives_script_on_error,
	.script_post_run = soj_importarchives_script_post_run,
	.script_pre_run  = soj_importarchives_script_pre_run,
};


static void soj_importarchives_exit(struct so_job * job __attribute__((unused)), struct so_database_connection * db_connect __attribute__((unused))) {
	if (soj_importarchives_archives != NULL)
		so_value_free(soj_importarchives_archives);
	soj_importarchives_archives = NULL;

	if (soj_importarchives_media != NULL)
		so_media_free(soj_importarchives_media);
	soj_importarchives_media = NULL;

	if (soj_importarchives_pool != NULL)
		so_pool_free(soj_importarchives_pool);
	soj_importarchives_pool = NULL;
}

static void soj_importarchives_init() {
	soj_job_register(&soj_importarchives);

	bindtextdomain("storiqone-job-import-archives", LOCALE_DIR);
}

static int soj_importarchives_run(struct so_job * job, struct so_database_connection * db_connect) {
	job->done = 0;

	struct so_drive * drive = soj_media_find_and_load(soj_importarchives_media, false, 0, db_connect);
	if (drive == NULL) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-import-archives", "Failed to load media '%s'"),
			soj_importarchives_media->name);
		return 2;
	}

	job->status = so_job_status_running;

	drive->ops->sync(drive);

	job->done = 0.33;
	if (job->stopped_by_user)
		return 1;

	so_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_important,
		dgettext("storiqone-job-import-archives", "Find first archive"));

	soj_importarchives_archives = so_value_new_linked_list();

	/*
	struct so_archive * archive = drive->ops->find_first_archive(drive);
	while (archive != NULL) {
		so_value_list_push(soj_importarchives_archives, so_value_new_custom(archive, so_archive_free2), true);

		if (job->stopped_by_user)
			break;

		archive = drive->ops->find_next_archive(drive);

		unsigned int i;
		for (i = 0; i < archive->nb_volumes; i++) {
			struct so_archive_volume * vol = archive->volumes + i;
			vol->media->pool = soj_importarchives_pool;
		}
	}
	*/

	job->done = 0.5;
	if (job->stopped_by_user)
		return 1;

	struct so_value_iterator * iter = so_value_list_get_iterator(soj_importarchives_archives);
	while (so_value_iterator_has_next(iter)) {
		struct so_value * varchive = so_value_iterator_get_value(iter, false);
		struct so_archive * archive = so_value_custom_get(varchive);

		db_connect->ops->sync_archive(db_connect, archive, NULL);
	}
	so_value_iterator_free(iter);

	job->done = 1;

	return 0;
}

static int soj_importarchives_simulate(struct so_job * job, struct so_database_connection * db_connect) {
	soj_importarchives_media = db_connect->ops->get_media(db_connect, NULL, NULL, job);
	if (soj_importarchives_media == NULL) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-import-archives", "Critic error, no media linked to the current job"));
		return 1;
	}
	if (soj_importarchives_media->type == so_media_type_cleaning) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-import-archives", "Try to import a cleaning media '%s'"),
			soj_importarchives_media->name);
		return 1;
	}
	if (soj_importarchives_media->pool != NULL) {
		so_job_add_record(job, db_connect, so_log_level_warning, so_job_record_notif_important,
			dgettext("storiqone-job-import-archives", "Try to import a media '%s' which is a member of pool '%s'"),
			soj_importarchives_media->name, soj_importarchives_media->pool->name);
		return 1;
	}
	if (soj_importarchives_media->status == so_media_status_error)
		so_job_add_record(job, db_connect, so_log_level_warning, so_job_record_notif_important,
			dgettext("storiqone-job-import-archives", "Try to import a media '%s' with error status"),
			soj_importarchives_media->name);

	soj_importarchives_pool = db_connect->ops->get_pool(db_connect, NULL, job);
	if (soj_importarchives_pool == NULL) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-import-archives", "Pool not found"));
		return 1;
	}
	if (soj_importarchives_pool->deleted) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-import-archives", "Try to import into a pool '%s' which is deleted"),
			soj_importarchives_pool->name);
		return 1;
	}

	if (so_media_format_cmp(soj_importarchives_media->media_format, soj_importarchives_pool->media_format) != 0) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-import-archives", "Try to import archives from media '%s' (media format: %s) whose type does not match the format of pool '%s' (fomat: %s)"),
			soj_importarchives_media->name, soj_importarchives_media->media_format->name, soj_importarchives_pool->name, soj_importarchives_pool->media_format->name);
		return 1;
	}

	if (!soj_changer_has_apt_drive(soj_importarchives_media->media_format, false)) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-import-archives", "Failed to find suitable drive to format media '%s'"),
			soj_importarchives_media->name);
		return 1;
	}

	return 0;
}

static void soj_importarchives_script_on_error(struct so_job * job, struct so_database_connection * db_connect) {
	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_on_error, soj_importarchives_pool) < 1)
		return;

	struct so_value * archives = so_value_new_linked_list();
	if (soj_importarchives_archives != NULL) {
		struct so_value_iterator * iter = so_value_list_get_iterator(soj_importarchives_archives);
		while (so_value_iterator_has_next(iter)) {
			struct so_value * varchive = so_value_iterator_get_value(iter, false);
			struct so_archive * archive = so_value_custom_get(varchive);

			so_value_list_push(archives, so_archive_convert(archive), true);
		}
		so_value_iterator_free(iter);
	}

	struct so_value * json = so_value_pack("{sososososo}",
		"archives", archives,
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"media", so_media_convert(soj_importarchives_media),
		"pool", so_pool_convert(soj_importarchives_pool)
	);

	struct so_value * returned = soj_script_run(db_connect, job, so_script_type_on_error, soj_importarchives_pool, json);
	so_value_free(json);
	so_value_free(returned);
}

static void soj_importarchives_script_post_run(struct so_job * job, struct so_database_connection * db_connect) {
	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_post_job, soj_importarchives_pool) < 1)
		return;

	struct so_value * archives = so_value_new_linked_list();
	if (soj_importarchives_archives != NULL) {
		struct so_value_iterator * iter = so_value_list_get_iterator(soj_importarchives_archives);
		while (so_value_iterator_has_next(iter)) {
			struct so_value * varchive = so_value_iterator_get_value(iter, false);
			struct so_archive * archive = so_value_custom_get(varchive);

			so_value_list_push(archives, so_archive_convert(archive), true);
		}
		so_value_iterator_free(iter);
	}

	struct so_value * json = so_value_pack("{sosososo}",
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"media", so_media_convert(soj_importarchives_media),
		"pool", so_pool_convert(soj_importarchives_pool)
	);

	struct so_value * returned = soj_script_run(db_connect, job, so_script_type_post_job, soj_importarchives_pool, json);
	so_value_free(json);
	so_value_free(returned);
}

static bool soj_importarchives_script_pre_run(struct so_job * job, struct so_database_connection * db_connect) {
	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_pre_job, soj_importarchives_pool) < 1)
		return true;

	struct so_value * json = so_value_pack("{sosososo}",
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"media", so_media_convert(soj_importarchives_media),
		"pool", so_pool_convert(soj_importarchives_pool)
	);

	struct so_value * returned = soj_script_run(db_connect, job, so_script_type_pre_job, soj_importarchives_pool, json);
	so_value_free(json);

	bool should_run = false;
	so_value_unpack(returned, "{sb}", "should run", &should_run);
	so_value_free(returned);

	return should_run;
}

