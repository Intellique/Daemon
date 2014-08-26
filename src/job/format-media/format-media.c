/****************************************************************************\
*                             __________                                     *
*                            / __/_  __/__  ___  ___                         *
*                           _\ \  / / / _ \/ _ \/ -_)                        *
*                          /___/ /_/  \___/_//_/\__/                         *
*                                                                            *
*  ------------------------------------------------------------------------  *
*  This file is a part of STone                                              *
*                                                                            *
*  STone is free software; you can redistribute it and/or modify             *
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
*  Copyright (C) 2014, Clercin guillaume <gclercin@intellique.com>           *
\****************************************************************************/

// free
#include <stdlib.h>
// strcpy
#include <string.h>
// uuid_generate, uuid_unparse_lower
#include <uuid/uuid.h>
// sleep
#include <unistd.h>

#include <libstone/database.h>
#include <libstone/host.h>
#include <libstone/log.h>
#include <libstone/media.h>
#include <libstone/slot.h>
#include <libstone/value.h>
#include <libstone-job/changer.h>
#include <libstone-job/drive.h>
#include <libstone-job/job.h>
#include <libstone-job/script.h>

#include <job_format-media.chcksum>

static struct st_pool * formatmedia_pool = NULL;
static struct st_slot * formatmedia_slot = NULL;

static void formatmedia_exit(struct st_job * job, struct st_database_connection * db_connect);
static void formatmedia_init(void) __attribute__((constructor));
static int formatmedia_run(struct st_job * job, struct st_database_connection * db_connect);
static int formatmedia_simulate(struct st_job * job, struct st_database_connection * db_connect);
static void formatmedia_script_on_error(struct st_job * job, struct st_database_connection * db_connect);
static void formatmedia_script_post_run(struct st_job * job, struct st_database_connection * db_connect);
static bool formatmedia_script_pre_run(struct st_job * job, struct st_database_connection * db_connect);

static struct st_job_driver formatmedia = {
	.name = "format-media",

	.exit            = formatmedia_exit,
	.run             = formatmedia_run,
	.simulate        = formatmedia_simulate,
	.script_on_error = formatmedia_script_on_error,
	.script_post_run = formatmedia_script_post_run,
	.script_pre_run  = formatmedia_script_pre_run,
};


static void formatmedia_exit(struct st_job * job __attribute__((unused)), struct st_database_connection * db_connect __attribute__((unused))) {}

static void formatmedia_init() {
	stj_job_register(&formatmedia);
}

static int formatmedia_run(struct st_job * job, struct st_database_connection * db_connect) {
	enum {
		changer_has_free_drive,
		check_write_support,
		drive_is_free,
		look_for_media,
		media_in_drive,
		reserve_media
	} state = reserve_media;

	char * cookie = NULL;
	struct st_changer * changer = formatmedia_slot->changer;
	struct st_drive * drive = NULL;

	int failed;
	bool stop = false;
	while (!stop && !job->stopped_by_user) {
		switch (state) {
			case changer_has_free_drive:
				drive = changer->ops->find_free_drive(changer, formatmedia_slot->media->format, true);
				if (drive != NULL)
					state = drive_is_free;
				else {
					changer->ops->release_all_media(changer);
					changer = NULL;
					drive = NULL;

					sleep(20);

					state = look_for_media;
				}
				break;

			case check_write_support:
				if (drive->ops->check_support(drive, formatmedia_slot->media->format, true)) {
					state = drive_is_free;
				} else {
					st_job_add_record(job, db_connect, st_log_level_warning, st_job_record_notif_important, "Unload media because drive can not write into this support");

					failed = changer->ops->unload(changer, drive);

					if (failed != 0) {
						st_job_add_record(job, db_connect, st_log_level_error, st_job_record_notif_important, "Error while unloading media (%s)", drive->slot->media->label);
						return 2;
					}

					changer = NULL;
					drive = NULL;

					state = look_for_media;
				}
				break;

			case drive_is_free:
				cookie = drive->ops->lock(drive);
				if (cookie == NULL) {
					changer->ops->release_all_media(changer);
					changer = NULL;
					drive = NULL;

					sleep(20);

					state = look_for_media;
				} else
					stop = true;
				break;

			case look_for_media:
				stj_changer_sync_all();
				formatmedia_slot = stj_changer_find_media_by_job(job, db_connect);
				if (formatmedia_slot != NULL) {
					changer = formatmedia_slot->changer;
					state = reserve_media;
				} else {
					sleep(20);
				}
				break;

			case media_in_drive:
				drive = formatmedia_slot->drive;
				state = drive != NULL ? drive_is_free : changer_has_free_drive;
				break;

			case reserve_media:
				failed = changer->ops->reserve_media(changer, formatmedia_slot);
				if (failed == 0)
					state = media_in_drive;
				else {
					sleep(20);

					changer = NULL;
					drive = NULL;
					state = look_for_media;
				}
				break;
		}
	}

	if (job->stopped_by_user)
		return 1;

	job->status = st_job_status_running;
	job->done = 0.2;

	if (formatmedia_slot->drive == NULL && drive->slot->media != NULL) {
		failed = changer->ops->unload(changer, drive);

		if (failed != 0) {
			st_job_add_record(job, db_connect, st_log_level_error, st_job_record_notif_important, "Error while unloading media (%s)", drive->slot->media->label);
			return 2;
		}
	}

	if (job->stopped_by_user)
		return 1;

	job->status = st_job_status_running;
	job->done = 0.4;

	if (formatmedia_slot->drive == NULL) {
		failed = changer->ops->load(changer, formatmedia_slot, drive);

		if (failed != 0) {
			st_job_add_record(job, db_connect, st_log_level_error, st_job_record_notif_important, "Error while loading media (%s)", formatmedia_slot->media->label);
			return 2;
		}
	}

	if (job->stopped_by_user)
		return 1;

	job->status = st_job_status_running;
	job->done = 0.6;

	drive->ops->sync(drive);

	// check for write lock
	if (drive->slot->media->write_lock) {
		st_job_add_record(job, db_connect, st_log_level_error, st_job_record_notif_important, "Try to format a write protected media");
		return 3;
	}

	// find best block size
	ssize_t block_size = drive->ops->find_best_block_size(drive);

	if (job->stopped_by_user)
		return 1;

	st_job_add_record(job, db_connect, st_log_level_info, st_job_record_notif_important, "Formatting media in progress (block size used: %zd)", block_size);

	// write header
	failed = drive->ops->format_media(drive, formatmedia_pool);
	if (failed != 0) {
		st_job_add_record(job, db_connect, st_log_level_error, st_job_record_notif_important, "Failed to format media");
		return 4;
	}

	// check header
	st_job_add_record(job, db_connect, st_log_level_info, st_job_record_notif_important, "Checking media header in progress");
	if (drive->ops->check_header(drive)) {
		st_job_add_record(job, db_connect, st_log_level_info, st_job_record_notif_important, "Checking media header: success");
		return 0;
	} else {
		st_job_add_record(job, db_connect, st_log_level_error, st_job_record_notif_important, "Checking media header: failed");
		return 5;
	}
}

static int formatmedia_simulate(struct st_job * job, struct st_database_connection * db_connect) {
	st_job_add_record(job, db_connect, st_log_level_info, st_job_record_notif_important, "Start (simulation) format media job (job name: %s), key: %s, num runs %ld", job->name, job->key, job->num_runs);

	formatmedia_pool = db_connect->ops->get_pool(db_connect, NULL, job);
	formatmedia_slot = stj_changer_find_media_by_job(job, db_connect);

	if (formatmedia_pool == NULL) {
		st_job_add_record(job, db_connect, st_log_level_error, st_job_record_notif_important, "Pool not found");
		return 1;
	}
	if (formatmedia_pool->deleted) {
		st_job_add_record(job, db_connect, st_log_level_error, st_job_record_notif_important, "Try to format to a pool which is deleted");
		return 1;
	}

	if (formatmedia_slot == NULL) {
		st_job_add_record(job, db_connect, st_log_level_error, st_job_record_notif_important, "Media not found");
		return 1;
	}
	if (formatmedia_slot->media == NULL) {
		st_job_add_record(job, db_connect, st_log_level_error, st_job_record_notif_important, "BUG: slot should not be empty");
		return 1;
	}

	struct st_media * media = formatmedia_slot->media;
	if (media->type == st_media_type_cleaning) {
		st_job_add_record(job, db_connect, st_log_level_error, st_job_record_notif_important, "Try to format a cleaning media");
		return 1;
	}
	if (media->type == st_media_type_worm && media->nb_volumes > 0) {
		st_job_add_record(job, db_connect, st_log_level_error, st_job_record_notif_important, "Try to format a worm media with data");
		return 1;
	}

	if (media->status == st_media_status_error)
		st_job_add_record(job, db_connect, st_log_level_warning, st_job_record_notif_important, "Try to format a media with error status");

	if (st_media_format_cmp(media->format, formatmedia_pool->format) != 0) {
		st_job_add_record(job, db_connect, st_log_level_error, st_job_record_notif_important, "Try to format a media whose type does not match the format of pool");
		return 1;
	}

	return 0;
}

static void formatmedia_script_on_error(struct st_job * job, struct st_database_connection * db_connect) {
}

static void formatmedia_script_post_run(struct st_job * job, struct st_database_connection * db_connect) {
}

static bool formatmedia_script_pre_run(struct st_job * job, struct st_database_connection * db_connect) {
	if (db_connect->ops->get_nb_scripts(db_connect, job->type, st_script_type_pre_job, formatmedia_pool) < 1)
		return true;

	struct st_value * json = st_value_pack("{sosososo}",
		"job", st_job_convert(job),
		"host", st_host_get_info2(),
		"media", st_media_convert(formatmedia_slot->media),
		"pool", st_pool_convert(formatmedia_pool)
	);

	struct st_value * returned = stj_script_run(db_connect, job, st_script_type_pre_job, formatmedia_pool, json);
	st_value_free(json);

	bool should_run = false;
	st_value_unpack(returned, "{sb}", "should run", &should_run);
	if (should_run) {
		struct st_value * datas = NULL;
		st_value_unpack(returned, "{so}", "datas", &datas);

		struct st_value_iterator * iter = st_value_list_get_iterator(datas);
		while (st_value_iterator_has_next(iter)) {
			struct st_value * data = st_value_iterator_get_value(iter, false);

			char * uuid = NULL;
			st_value_unpack(data, "{s{ss}}", "media", "uuid", &uuid);
			if (uuid != NULL) {
				uuid_t tmp;
				if (uuid_parse(uuid, tmp) != 0) {
					st_job_add_record(job, db_connect, st_log_level_warning, st_job_record_notif_normal, "Invalid uuid provide by script, ignoring it");
				} else {
					st_job_add_record(job, db_connect, st_log_level_warning, st_job_record_notif_normal, "Script request to format media with this uuid (%s) instead of (%s)", uuid, formatmedia_slot->media->uuid);
					strcpy(formatmedia_slot->media->uuid, uuid);
				}
				free(uuid);
			}

			char * name = NULL;
			st_value_unpack(data, "{s{ss}}", "media", "name", &name);
			if (name != NULL) {
				st_job_add_record(job, db_connect, st_log_level_warning, st_job_record_notif_normal, "Script request to format media with this name (%s) instead of (%s)", name, formatmedia_slot->media->name);

				free(formatmedia_slot->media->name);
				formatmedia_slot->media->name = name;
			}
		}
		st_value_iterator_free(iter);
	}

	st_value_free(returned);

	return should_run;
}

