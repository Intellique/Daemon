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
// sleep
#include <unistd.h>

#include <libstoriqone/archive.h>
#include <libstoriqone/database.h>
#include <libstoriqone/json.h>
#include <libstoriqone/host.h>
#include <libstoriqone/log.h>
#include <libstoriqone/media.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/value.h>
#include <libstoriqone-job/changer.h>
#include <libstoriqone-job/drive.h>
#include <libstoriqone-job/job.h>
#include <libstoriqone-job/media.h>
#include <libstoriqone-job/script.h>

#include <job_erase-media.chcksum>

#include "config.h"

static struct so_value * soj_erasemedia_convert_archives(void);
static void soj_erasemedia_exit(struct so_job * job, struct so_database_connection * db_connect);
static void soj_erasemedia_init(void) __attribute__((constructor));
static void soj_erasemedia_make_report(struct so_job * job, struct so_database_connection * db_connect);
static int soj_erasemedia_run(struct so_job * job, struct so_database_connection * db_connect);
static int soj_erasemedia_simulate(struct so_job * job, struct so_database_connection * db_connect);
static void soj_erasemedia_script_on_error(struct so_job * job, struct so_database_connection * db_connect);
static void soj_erasemedia_script_post_run(struct so_job * job, struct so_database_connection * db_connect);
static bool soj_erasemedia_script_pre_run(struct so_job * job, struct so_database_connection * db_connect);

static struct so_job_driver soj_erasemedia = {
	.name = "erase-media",

	.exit            = soj_erasemedia_exit,
	.run             = soj_erasemedia_run,
	.simulate        = soj_erasemedia_simulate,
	.script_on_error = soj_erasemedia_script_on_error,
	.script_post_run = soj_erasemedia_script_post_run,
	.script_pre_run  = soj_erasemedia_script_pre_run,
};

static struct so_value * soj_erasemedia_archives = NULL;
static struct so_media * soj_erasemedia_media = NULL;


static struct so_value * soj_erasemedia_convert_archives() {
	struct so_value * archives = so_value_new_linked_list();
	struct so_value_iterator * iter = so_value_list_get_iterator(soj_erasemedia_archives);
	while (so_value_iterator_has_next(iter)) {
		struct so_archive * archive = so_value_custom_get(so_value_iterator_get_value(iter, false));
		so_value_list_push(archives, so_archive_convert(archive), true);
	}
	so_value_iterator_free(iter);
	return archives;
}

static void soj_erasemedia_exit(struct so_job * job __attribute__((unused)), struct so_database_connection * db_connect __attribute__((unused))) {
	if (soj_erasemedia_media != NULL)
		so_media_free(soj_erasemedia_media);
	so_value_free(soj_erasemedia_archives);
}

static void soj_erasemedia_init() {
	soj_job_register(&soj_erasemedia);

	bindtextdomain("storiqone-job-erase-media", LOCALE_DIR);
}

static void soj_erasemedia_make_report(struct so_job * job, struct so_database_connection * db_connect) {
	struct so_value * report = so_value_pack("{sosososo}",
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"media", so_media_convert(soj_erasemedia_media),
		"archives", soj_erasemedia_convert_archives()
	);

	char * json = so_json_encode_to_string(report);
	db_connect->ops->add_report(db_connect, job, NULL, soj_erasemedia_media, json);

	free(json);
	so_value_free(report);
}

static int soj_erasemedia_run(struct so_job * job, struct so_database_connection * db_connect) {
	job->done = 0.25;

	struct so_drive * drive = soj_media_find_and_load(soj_erasemedia_media, false, 0, db_connect);
	if (drive == NULL) {
		so_job_add_record(job, db_connect, so_log_level_error,
			so_job_record_notif_important, dgettext("storiqone-job-erase-media", "Failed to load media '%s'"),
			soj_erasemedia_media->name);
		return 2;
	}

	job->status = so_job_status_running;
	job->done = 0.5;

	drive->ops->sync(drive);

	// check for write lock
	if (drive->slot->media->write_lock) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-erase-media", "Try to erase a media '%s' with a write protected media"),
			soj_erasemedia_media->name);
		return 3;
	}

	if (job->stopped_by_user)
		return 1;

	bool quick_mode = false;
	so_value_unpack(job->option, "{sb}", "quick mode", &quick_mode);

	so_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_important,
		dgettext("storiqone-job-erase-media", "Erasing media '%s' in progress (mode: %s)"),
		soj_erasemedia_media->name,
		quick_mode ? dgettext("storiqone-job-erase-media", "quick") : dgettext("storiqone-job-erase-media", "long"));

	job->done = 0.75;

	int failed = drive->ops->erase_media(drive, true);

	job->done = 1;

	if (failed == 0) {
		db_connect->ops->mark_archive_as_purged(db_connect, soj_erasemedia_media, job);
		soj_erasemedia_make_report(job, db_connect);

		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-erase-media", "Erase media '%s' finished with sucess"),
			soj_erasemedia_media->name);
	} else
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-erase-media", "Failed to erase media '%s'"),
			soj_erasemedia_media->name);

	return failed != 0 ? 2 : 0;
}

static int soj_erasemedia_simulate(struct so_job * job, struct so_database_connection * db_connect) {
	soj_erasemedia_media = db_connect->ops->get_media(db_connect, NULL, NULL, job);
	if (soj_erasemedia_media == NULL) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-erase-media", "Critic error, no media linked to the current job"));
		return 1;
	}
	if (soj_erasemedia_media->type == so_media_type_cleaning) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-erase-media", "Try to erase a cleaning media"));
		return 1;
	}
	if (soj_erasemedia_media->type == so_media_type_worm) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-erase-media", "Try to erase a worm media"));
		return 1;
	}
	if (soj_erasemedia_media->status == so_media_status_error)
		so_job_add_record(job, db_connect, so_log_level_warning, so_job_record_notif_important,
			dgettext("storiqone-job-erase-media", "Try to erase a media with error status"));

	soj_erasemedia_archives = db_connect->ops->get_archive_by_media(db_connect, soj_erasemedia_media);

	struct so_value_iterator * iter = so_value_list_get_iterator(soj_erasemedia_archives);
	while (so_value_iterator_has_next(iter)) {
		struct so_archive * archive = so_value_custom_get(so_value_iterator_get_value(iter, false));
		if (!archive->deleted)
			so_job_add_record(job, db_connect, so_log_level_warning, so_job_record_notif_important,
				dgettext("storiqone-job-erase-media", "Try to erase a media witch contains an archive not deleted (named: %s)"),
				archive->name);
	}
	so_value_iterator_free(iter);

	return 0;
}

static void soj_erasemedia_script_on_error(struct so_job * job, struct so_database_connection * db_connect) {
	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_on_error, soj_erasemedia_media->pool) < 1)
		return;

	struct so_value * json = so_value_pack("{sosososo}",
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"media", so_media_convert(soj_erasemedia_media),
		"archives", soj_erasemedia_convert_archives()
	);

	struct so_value * returned = soj_script_run(db_connect, job, so_script_type_on_error, soj_erasemedia_media->pool, json);
	so_value_free(json);
	so_value_free(returned);
}

static void soj_erasemedia_script_post_run(struct so_job * job, struct so_database_connection * db_connect) {
	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_post_job, soj_erasemedia_media->pool) < 1)
		return;

	struct so_value * json = so_value_pack("{sosososo}",
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"media", so_media_convert(soj_erasemedia_media),
		"archives", soj_erasemedia_convert_archives()
	);

	struct so_value * returned = soj_script_run(db_connect, job, so_script_type_post_job, soj_erasemedia_media->pool, json);
	so_value_free(json);
	so_value_free(returned);
}

static bool soj_erasemedia_script_pre_run(struct so_job * job, struct so_database_connection * db_connect) {
	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_pre_job, soj_erasemedia_media->pool) < 1)
		return true;

	struct so_value * json = so_value_pack("{sosososo}",
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"media", so_media_convert(soj_erasemedia_media),
		"archives", soj_erasemedia_convert_archives()
	);

	struct so_value * returned = soj_script_run(db_connect, job, so_script_type_pre_job, soj_erasemedia_media->pool, json);
	so_value_free(json);

	bool should_run = false;
	so_value_unpack(returned, "{sb}", "should run", &should_run);
	so_value_free(returned);

	return should_run;
}

