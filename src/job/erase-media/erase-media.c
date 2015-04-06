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
// sleep
#include <unistd.h>

#include <libstoriqone/database.h>
#include <libstoriqone/host.h>
#include <libstoriqone/log.h>
#include <libstoriqone/media.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/value.h>
#include <libstoriqone-job/changer.h>
#include <libstoriqone-job/drive.h>
#include <libstoriqone-job/job.h>
#include <libstoriqone-job/script.h>

#include <job_erase-media.chcksum>

#include "config.h"

static void soj_erasemedia_exit(struct so_job * job, struct so_database_connection * db_connect);
static void soj_erasemedia_init(void) __attribute__((constructor));
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

static struct so_media * soj_erasemedia_media = NULL;


static void soj_erasemedia_exit(struct so_job * job __attribute__((unused)), struct so_database_connection * db_connect __attribute__((unused))) {
	if (soj_erasemedia_media != NULL)
		so_media_free(soj_erasemedia_media);
}

static void soj_erasemedia_init() {
	soj_job_register(&soj_erasemedia);

	bindtextdomain("storiqone-job-erase-media", LOCALE_DIR);
}

static int soj_erasemedia_run(struct so_job * job, struct so_database_connection * db_connect) {
	enum {
		alert_user,
		get_media,
		look_for_media,
		reserve_media,
	} state = look_for_media;
	bool stop = false, has_alert_user = false;
	struct so_slot * slot = NULL;
	ssize_t reserved_size = 0;
	struct so_drive * drive = NULL;

	while (!stop && !job->stopped_by_user) {
		switch (state) {
			case alert_user:
				job->status = so_job_status_waiting;
				if (!has_alert_user)
					so_job_add_record(job, db_connect, so_log_level_warning, so_job_record_notif_important, dgettext("storiqone-job-erase-media", "Media not found (named: %s)"), soj_erasemedia_media->name);
				has_alert_user = true;

				sleep(15);

				state = look_for_media;
				job->status = so_job_status_running;
				soj_changer_sync_all();
				break;

			case get_media:
				drive = slot->changer->ops->get_media(slot->changer, slot, false);
				if (drive == NULL) {
					job->status = so_job_status_waiting;

					sleep(15);

					state = look_for_media;
					job->status = so_job_status_running;
					soj_changer_sync_all();
				} else
					stop = true;
				break;

			case look_for_media:
				slot = soj_changer_find_media_by_job(job, db_connect);
				state = slot != NULL ? reserve_media : alert_user;
				break;

			case reserve_media:
				reserved_size = slot->changer->ops->reserve_media(slot->changer, slot, soj_erasemedia_media->format->block_size, so_pool_unbreakable_level_none);
				if (reserved_size < 0) {
					job->status = so_job_status_waiting;

					sleep(15);

					state = look_for_media;
					job->status = so_job_status_running;
					soj_changer_sync_all();
				} else
					state = get_media;
				break;
		}
	}

	job->status = so_job_status_running;
	job->done = 0.6;

	drive->ops->sync(drive);

	// check for write lock
	if (drive->slot->media->write_lock) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-erase-media", "Try to format a write protected media"));
		return 3;
	}

	if (job->stopped_by_user)
		return 1;

	bool quick_mode = true;
	so_value_unpack(job->option, "{sb}", "quick mode", &quick_mode);

	so_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_important,
		dgettext("storiqone-job-erase-media", "Erasing media in progress (mode: %s)"),
		quick_mode ? dgettext("storiqone-job-erase-media", "quick") : dgettext("storiqone-job-erase-media", "long"));

	int failed = drive->ops->erase_media(drive, true);
	if (failed == 0)
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-erase-media", "Erase media finished with sucess"));
	else
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-erase-media", "Failed to erase media"));

	return failed != 0 ? 2 : 0;
}

static int soj_erasemedia_simulate(struct so_job * job, struct so_database_connection * db_connect) {
	soj_erasemedia_media = db_connect->ops->get_media(db_connect, NULL, NULL, job);
	if (soj_erasemedia_media == NULL) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important, dgettext("storiqone-job-erase-media", "Critic error, no media linked to the current job"));
		return 1;
	}
	if (soj_erasemedia_media->type == so_media_type_cleaning) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important, dgettext("storiqone-job-erase-media", "Try to erase a cleaning media"));
		return 1;
	}
	if (soj_erasemedia_media->type == so_media_type_worm) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important, dgettext("storiqone-job-erase-media", "Try to erase a worm media"));
		return 1;
	}
	if (soj_erasemedia_media->status == so_media_status_error)
		so_job_add_record(job, db_connect, so_log_level_warning, so_job_record_notif_important, dgettext("storiqone-job-erase-media", "Try to erase a media with error status"));

	return 0;
}

static void soj_erasemedia_script_on_error(struct so_job * job, struct so_database_connection * db_connect) {
	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_on_error, soj_erasemedia_media->pool) < 1)
		return;

	struct so_value * json = so_value_pack("{sososo}",
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"media", so_media_convert(soj_erasemedia_media)
	);

	struct so_value * returned = soj_script_run(db_connect, job, so_script_type_on_error, soj_erasemedia_media->pool, json);
	so_value_free(json);
	so_value_free(returned);
}

static void soj_erasemedia_script_post_run(struct so_job * job, struct so_database_connection * db_connect) {
	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_post_job, soj_erasemedia_media->pool) < 1)
		return;

	struct so_value * json = so_value_pack("{sososo}",
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"media", so_media_convert(soj_erasemedia_media)
	);

	struct so_value * returned = soj_script_run(db_connect, job, so_script_type_post_job, soj_erasemedia_media->pool, json);
	so_value_free(json);
	so_value_free(returned);
}

static bool soj_erasemedia_script_pre_run(struct so_job * job, struct so_database_connection * db_connect) {
	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_pre_job, soj_erasemedia_media->pool) < 1)
		return true;

	struct so_value * json = so_value_pack("{sososo}",
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"media", so_media_convert(soj_erasemedia_media)
	);

	struct so_value * returned = soj_script_run(db_connect, job, so_script_type_pre_job, soj_erasemedia_media->pool, json);
	so_value_free(json);

	bool should_run = false;
	so_value_unpack(returned, "{sb}", "should run", &should_run);
	so_value_free(returned);

	return should_run;
}

