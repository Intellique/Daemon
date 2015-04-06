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
// strcpy
#include <string.h>
// uuid_generate, uuid_unparse_lower
#include <uuid/uuid.h>
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

#include <job_format-media.chcksum>

#include "config.h"

static struct so_media * soj_formatmedia_media = NULL;
static struct so_pool * soj_formatmedia_pool = NULL;

static void soj_formatmedia_exit(struct so_job * job, struct so_database_connection * db_connect);
static void soj_formatmedia_init(void) __attribute__((constructor));
static int soj_formatmedia_run(struct so_job * job, struct so_database_connection * db_connect);
static int soj_formatmedia_simulate(struct so_job * job, struct so_database_connection * db_connect);
static void soj_formatmedia_script_on_error(struct so_job * job, struct so_database_connection * db_connect);
static void soj_formatmedia_script_post_run(struct so_job * job, struct so_database_connection * db_connect);
static bool soj_formatmedia_script_pre_run(struct so_job * job, struct so_database_connection * db_connect);

static struct so_job_driver soj_formatmedia = {
	.name = "format-media",

	.exit            = soj_formatmedia_exit,
	.run             = soj_formatmedia_run,
	.simulate        = soj_formatmedia_simulate,
	.script_on_error = soj_formatmedia_script_on_error,
	.script_post_run = soj_formatmedia_script_post_run,
	.script_pre_run  = soj_formatmedia_script_pre_run,
};


static void soj_formatmedia_exit(struct so_job * job __attribute__((unused)), struct so_database_connection * db_connect __attribute__((unused))) {
	so_pool_free(soj_formatmedia_pool);
	soj_formatmedia_pool = NULL;
}

static void soj_formatmedia_init() {
	soj_job_register(&soj_formatmedia);

	bindtextdomain("storiqone-job-format-media", LOCALE_DIR);
}

static int soj_formatmedia_run(struct so_job * job, struct so_database_connection * db_connect) {
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
					so_job_add_record(job, db_connect, so_log_level_warning, so_job_record_notif_important, dgettext("storiqone-job-format-media", "Media not found (named: %s)"), soj_formatmedia_media->name);
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
				reserved_size = slot->changer->ops->reserve_media(slot->changer, slot, soj_formatmedia_media->format->block_size, so_pool_unbreakable_level_none);
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
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important, dgettext("storiqone-job-format-media", "Try to format a write protected media"));
		return 3;
	}

	// find best block size
	ssize_t block_size = drive->ops->find_best_block_size(drive);

	if (job->stopped_by_user)
		return 1;

	so_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_important, dgettext("storiqone-job-format-media", "Formatting media in progress (block size used: %zd)"), block_size);

	// write header
	int failed = drive->ops->format_media(drive, soj_formatmedia_pool);
	if (failed != 0) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important, dgettext("storiqone-job-format-media", "Failed to format media"));
		return 4;
	}

	job->status = so_job_status_running;
	job->done = 0.8;

	// check header
	so_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_important, dgettext("storiqone-job-format-media", "Checking media header in progress"));
	if (drive->ops->check_header(drive)) {
		so_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_important, dgettext("storiqone-job-format-media", "Checking media header: success"));

		job->status = so_job_status_running;
		job->done = 1;
		return 0;
	} else {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important, dgettext("storiqone-job-format-media", "Checking media header: failed"));

		job->status = so_job_status_error;
		return 5;
	}
}

static int soj_formatmedia_simulate(struct so_job * job, struct so_database_connection * db_connect) {
	soj_formatmedia_pool = db_connect->ops->get_pool(db_connect, NULL, job);

	if (soj_formatmedia_pool == NULL) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important, dgettext("storiqone-job-format-media", "Pool not found"));
		return 1;
	}
	if (soj_formatmedia_pool->deleted) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important, dgettext("storiqone-job-format-media", "Try to format to a pool which is deleted"));
		return 1;
	}

	soj_formatmedia_media = db_connect->ops->get_media(db_connect, NULL, NULL, job);
	if (soj_formatmedia_media == NULL) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important, dgettext("storiqone-job-format-media", "Critic error, no media linked to the current job"));
		return 1;
	}
	if (soj_formatmedia_media->type == so_media_type_cleaning) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important, dgettext("storiqone-job-format-media", "Try to format a cleaning media"));
		return 1;
	}
	if (soj_formatmedia_media->type == so_media_type_worm && soj_formatmedia_media->nb_volumes > 0) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important, dgettext("storiqone-job-format-media", "Try to format a worm media with data"));
		return 1;
	}
	if (soj_formatmedia_media->status == so_media_status_error)
		so_job_add_record(job, db_connect, so_log_level_warning, so_job_record_notif_important, dgettext("storiqone-job-format-media", "Try to format a media with error status"));

	if (so_media_format_cmp(soj_formatmedia_media->format, soj_formatmedia_pool->format) != 0) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important, dgettext("storiqone-job-format-media", "Try to format a media whose type does not match the format of pool"));
		return 1;
	}

	if (!soj_changer_has_apt_drive(soj_formatmedia_media->format, true)) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important, dgettext("storiqone-job-format-media", "Failed to find suitable drive to format media"));
		return 1;
	}

	return 0;
}

static void soj_formatmedia_script_on_error(struct so_job * job, struct so_database_connection * db_connect) {
	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_on_error, soj_formatmedia_pool) < 1)
		return;

	struct so_value * json = so_value_pack("{sosososo}",
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"media", so_media_convert(soj_formatmedia_media),
		"pool", so_pool_convert(soj_formatmedia_pool)
	);

	struct so_value * returned = soj_script_run(db_connect, job, so_script_type_on_error, soj_formatmedia_pool, json);
	so_value_free(json);
	so_value_free(returned);
}

static void soj_formatmedia_script_post_run(struct so_job * job, struct so_database_connection * db_connect) {
	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_post_job, soj_formatmedia_pool) < 1)
		return;

	struct so_value * json = so_value_pack("{sosososo}",
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"media", so_media_convert(soj_formatmedia_media),
		"pool", so_pool_convert(soj_formatmedia_pool)
	);

	struct so_value * returned = soj_script_run(db_connect, job, so_script_type_post_job, soj_formatmedia_pool, json);
	so_value_free(json);
	so_value_free(returned);
}

static bool soj_formatmedia_script_pre_run(struct so_job * job, struct so_database_connection * db_connect) {
	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_pre_job, soj_formatmedia_pool) < 1)
		return true;

	struct so_value * json = so_value_pack("{sosososo}",
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"media", so_media_convert(soj_formatmedia_media),
		"pool", so_pool_convert(soj_formatmedia_pool)
	);

	struct so_value * returned = soj_script_run(db_connect, job, so_script_type_pre_job, soj_formatmedia_pool, json);
	so_value_free(json);

	bool should_run = false;
	so_value_unpack(returned, "{sb}", "should run", &should_run);
	if (should_run) {
		struct so_value * datas = NULL;
		so_value_unpack(returned, "{so}", "datas", &datas);

		struct so_value_iterator * iter = so_value_list_get_iterator(datas);
		while (so_value_iterator_has_next(iter)) {
			struct so_value * data = so_value_iterator_get_value(iter, false);

			char * uuid = NULL;
			so_value_unpack(data, "{s{ss}}", "media", "uuid", &uuid);
			if (uuid != NULL) {
				uuid_t tmp;
				if (uuid_parse(uuid, tmp) != 0) {
					so_job_add_record(job, db_connect, so_log_level_warning, so_job_record_notif_normal, dgettext("storiqone-job-format-media", "Invalid uuid provide by script, ignoring it"));
				} else {
					so_job_add_record(job, db_connect, so_log_level_warning, so_job_record_notif_normal, dgettext("storiqone-job-format-media", "Script request to format media with this uuid (%s) instead of (%s)"), uuid, soj_formatmedia_media->uuid);
					strcpy(soj_formatmedia_media->uuid, uuid);
				}
				free(uuid);
			}

			char * name = NULL;
			so_value_unpack(data, "{s{ss}}", "media", "name", &name);
			if (name != NULL) {
				so_job_add_record(job, db_connect, so_log_level_warning, so_job_record_notif_normal, dgettext("storiqone-job-format-media", "Script request to format media with this name (%s) instead of (%s)"), name, soj_formatmedia_media->name);

				free(soj_formatmedia_media->name);
				soj_formatmedia_media->name = name;
			}
		}
		so_value_iterator_free(iter);
	}

	so_value_free(returned);

	return should_run;
}

