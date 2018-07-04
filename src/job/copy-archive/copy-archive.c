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
*  Copyright (C) 2013-2016, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// bindtextdomain, dgettext
#include <libintl.h>
// free
#include <stdlib.h>
// strdup
#include <string.h>
// sleep
#include <unistd.h>
// uuid
#include <uuid/uuid.h>

#include <libstoriqone/archive.h>
#include <libstoriqone/database.h>
#include <libstoriqone/format.h>
#include <libstoriqone/host.h>
#include <libstoriqone/json.h>
#include <libstoriqone/log.h>
#include <libstoriqone/media.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/value.h>
#include <libstoriqone-job/changer.h>
#include <libstoriqone-job/job.h>
#include <libstoriqone-job/media.h>
#include <libstoriqone-job/script.h>

#include <job_copy-archive.chcksum>

#include "common.h"
#include "config.h"

static struct soj_copyarchive_private data = {
	.src_archive = NULL,
	.src_drive   = NULL,

	.copy_archive = NULL,
	.dest_drive   = NULL,
	.writer       = NULL,

	.first_files = NULL,
	.last_files  = NULL,
	.nb_files    = 0,
};

static void soj_copyarchive_exit(struct so_job * job, struct so_database_connection * db_connect);
static void soj_copyarchive_init(void) __attribute__((constructor));
static int soj_copyarchive_run(struct so_job * job, struct so_database_connection * db_connect);
static void soj_copyarchive_script_on_error(struct so_job * job, struct so_database_connection * db_connect);
static void soj_copyarchive_script_post_run(struct so_job * job, struct so_database_connection * db_connect);
static bool soj_copyarchive_script_pre_run(struct so_job * job, struct so_database_connection * db_connect);
static int soj_copyarchive_warm_up(struct so_job * job, struct so_database_connection * db_connect);

static struct so_job_driver soj_copyarchive = {
	.name = "copy-archive",

	.exit            = soj_copyarchive_exit,
	.run             = soj_copyarchive_run,
	.script_on_error = soj_copyarchive_script_on_error,
	.script_post_run = soj_copyarchive_script_post_run,
	.script_pre_run  = soj_copyarchive_script_pre_run,
	.warm_up         = soj_copyarchive_warm_up,
};


static void soj_copyarchive_exit(struct so_job * job __attribute__((unused)), struct so_database_connection * db_connect __attribute__((unused))) {
	if (data.src_archive != NULL)
		so_archive_free(data.src_archive);

	if (data.copy_archive != NULL)
		so_archive_free(data.copy_archive);
}

static void soj_copyarchive_init() {
	soj_job_register(&soj_copyarchive);

	bindtextdomain("storiqone-job-copy-archive", LOCALE_DIR);
}

static int soj_copyarchive_run(struct so_job * job, struct so_database_connection * db_connect) {
	job->done = 0.01;

	struct so_media * media = data.src_archive->volumes[0].media;

	data.src_drive = soj_media_find_and_load(media, false, 0, false, NULL, NULL, db_connect);
	if (data.src_drive == NULL) {
		soj_job_add_record(job, db_connect, so_log_level_error,
			so_job_record_notif_important, dgettext("storiqone-job-format-media", "Failed to load media '%s'"),
			media->name);
		return 2;
	}

	soj_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_normal,
		dgettext("storiqone-job-copy-archive", "Synchronizing archive with database"));
	int failed = soj_copyarchive_util_sync_archive(job, data.copy_archive, db_connect);
	if (failed != 0) {
		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-copy-archive", "Error while synchronizing archive with database"));
		return failed;
	} else
		soj_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_normal,
			dgettext("storiqone-job-copy-archive", "Archive synchronized with database"));

	bool error = false;
	data.dest_drive = soj_media_find_and_load_next(data.copy_archive->pool, true, &error, db_connect);

	if (error) {
		job->status = so_job_status_error;
		return 1;
	}

	if (data.dest_drive == NULL)
		failed = soj_copyarchive_indirect_copy(job, db_connect, &data);
	else
		failed = soj_copyarchive_direct_copy(job, db_connect, &data);

	if (failed == 0) {
		job->done = 0.99;

		soj_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_normal,
			dgettext("storiqone-job-copy-archive", "Synchronizing archive with database"));
		failed = soj_copyarchive_util_sync_archive(job, data.copy_archive, db_connect);
		if (failed != 0) {
			soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-copy-archive", "Error while synchronizing archive with database"));
			return failed;
		} else
			soj_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_normal,
				dgettext("storiqone-job-copy-archive", "Archive synchronized with database"));

		soj_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_important,
			dgettext("storiqone-job-copy-archive", "Writing metadata of copy archive '%s'"),
			data.src_archive->name);

		failed = soj_copyarchive_util_write_meta(&data);

		data.writer->ops->free(data.writer);
		data.writer = NULL;

		if (failed != 0)
			soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
				dgettext("storiqone-job-copy-archive", "Error while writing metadata of copy archive '%s'"),
				data.src_archive->name);
		else
			soj_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_important,
				dgettext("storiqone-job-copy-archive", "Wrote sucessfully metadata of copy archive '%s'"),
				data.src_archive->name);

		if (failed == 0) {
			data.copy_archive->status = so_archive_status_complete;

			soj_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_important,
				dgettext("storiqone-job-copy-archive", "Synchronizing archive '%s' into database"),
				data.src_archive->name);

			struct so_value * report = so_value_pack("{sisosososO}",
				"report version", 2,
				"job", so_job_convert(job),
				"host", so_host_get_info2(),
				"pool", so_pool_convert(data.copy_archive->volumes->media->pool),
				"archive", so_archive_convert(data.copy_archive)
			);
			char * json = so_json_encode_to_string(report);

			failed = db_connect->ops->start_transaction(db_connect);
			if (failed != 0) {
				soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
					dgettext("storiqone-job-copy-archive", "Failed to start database transaction"));
			} else {
				failed = db_connect->ops->sync_archive(db_connect, data.copy_archive, data.src_archive);
				if (failed == 0)
					failed = db_connect->ops->link_archives(db_connect, job, data.src_archive, data.copy_archive, data.copy_archive->pool);

				db_connect->ops->add_report(db_connect, job, data.copy_archive, NULL, json);

				if (failed != 0)
					db_connect->ops->cancel_transaction(db_connect);
				else
					db_connect->ops->finish_transaction(db_connect);
			}

			if (failed != 0)
				soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
					dgettext("storiqone-job-copy-archive", "Failed to synchronize archive '%s' into database"),
					data.copy_archive->name);
			else {
				soj_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_important,
					dgettext("storiqone-job-copy-archive", "Archive '%s' synchronized into database"),
					data.src_archive->name);
				job->done = 1;
			}

			free(json);
			so_value_free(report);
		}

		if (failed == 0) {
			bool check_archive = data.copy_archive->pool->auto_check == so_pool_autocheck_quick_mode || data.copy_archive->pool->auto_check == so_pool_autocheck_thorough_mode;
			so_value_unpack(job->option, "{sb}", "check_archive", &check_archive);

			if (check_archive) {
				char * mode = NULL;
				so_value_unpack(job->option, "{ss}", "check_archive_mode", &mode);

				bool quick_mode = data.copy_archive->pool->auto_check == so_pool_autocheck_quick_mode;
				if (mode != NULL) {
					quick_mode = strcmp(mode, "quick_mode") == 0;
					free(mode);
				}

				failed = db_connect->ops->create_check_archive_job(db_connect, job, data.copy_archive, quick_mode);
			}
		}
	}

	return failed;
}

static void soj_copyarchive_script_on_error(struct so_job * job, struct so_database_connection * db_connect) {
	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_on_error, data.copy_archive->pool) < 1)
		return;

	struct so_value * json = so_value_pack("{sosos{so}so}",
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"archive",
			"source", so_archive_convert(data.src_archive),
		"pool", so_pool_convert(data.copy_archive->pool)
	);

	struct so_value * returned = soj_script_run(db_connect, job, so_script_type_on_error, data.copy_archive->pool, json);
	so_value_free(json);
	so_value_free(returned);
}

static void soj_copyarchive_script_post_run(struct so_job * job, struct so_database_connection * db_connect) {
	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_post_job, data.copy_archive->pool) < 1)
		return;

	struct so_value * json = so_value_pack("{sosos{soso}so}",
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"archive",
			"source", so_archive_convert(data.src_archive),
			"copy", so_archive_convert(data.copy_archive),
		"pool", so_pool_convert(data.copy_archive->pool)
	);

	struct so_value * returned = soj_script_run(db_connect, job, so_script_type_post_job, data.copy_archive->pool, json);
	so_value_free(json);
	so_value_free(returned);
}

static bool soj_copyarchive_script_pre_run(struct so_job * job, struct so_database_connection * db_connect) {
	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_pre_job, data.copy_archive->pool) < 1)
		return true;

	struct so_value * json = so_value_pack("{sosos{so}so}",
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"archive",
			"source", so_archive_convert(data.src_archive),
		"pool", so_pool_convert(data.copy_archive->pool)
	);

	struct so_value * returned = soj_script_run(db_connect, job, so_script_type_pre_job, data.copy_archive->pool, json);
	so_value_free(json);

	bool should_run = false;
	so_value_unpack(returned, "{sb}", "should run", &should_run);
	so_value_free(returned);

	return should_run;
}

static int soj_copyarchive_warm_up(struct so_job * job, struct so_database_connection * db_connect) {
	data.src_archive = db_connect->ops->get_archive_by_job(db_connect, job);
	if (data.src_archive == NULL) {
		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-copy-archive", "Archive not found"));
		return 1;
	}
	if (data.src_archive->deleted) {
		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-copy-archive", "Trying to copy a deleted archive '%s'"),
			data.src_archive->name);
		return 1;
	}

	data.copy_archive = so_archive_new();
	data.copy_archive->name = strdup(data.src_archive->name);
	if (data.src_archive->metadata != NULL)
		data.copy_archive->metadata = so_value_share(data.src_archive->metadata);

	uuid_t uuid;
	uuid_generate(uuid);
	uuid_unparse_lower(uuid, data.copy_archive->uuid);

	data.copy_archive->creator = strdup(job->user);
	data.copy_archive->owner = strdup(job->user);

	data.copy_archive->pool = db_connect->ops->get_pool(db_connect, NULL, job);
	if (data.copy_archive->pool == NULL) {
		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-copy-archive", "Pool not found"));
		return 1;
	}
	if (data.copy_archive->pool->deleted) {
		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-copy-archive", "Trying to copy an archive '%s' to a deleted pool '%s'"),
			data.src_archive->name, data.copy_archive->pool->name);
		return 1;
	}

	if (!soj_changer_has_apt_drive(data.copy_archive->pool->media_format, true)) {
		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-copy-archive", "Failed to find a suitable drive to copy archive '%s'"),
			data.src_archive->name);
		return 1;
	}

	ssize_t reserved = soj_media_prepare(data.copy_archive->pool, data.src_archive->size, true, NULL, db_connect);
	if (reserved < data.src_archive->size) {
		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-copy-archive", "Error: not enought space available in pool '%s'"),
			data.copy_archive->pool->name);
		return 1;
	}

	return 0;
}
