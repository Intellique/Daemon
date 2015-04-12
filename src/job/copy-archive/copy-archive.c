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

#include <libstoriqone/archive.h>
#include <libstoriqone/database.h>
#include <libstoriqone/host.h>
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

	.copy_archive   = NULL,
	.pool           = NULL,
	.media_iterator = NULL,
	.media          = NULL,
	.dest_drive     = NULL,
};

static void soj_copyarchive_exit(struct so_job * job, struct so_database_connection * db_connect);
static void soj_copyarchive_init(void) __attribute__((constructor));
static int soj_copyarchive_run(struct so_job * job, struct so_database_connection * db_connect);
static int soj_copyarchive_simulate(struct so_job * job, struct so_database_connection * db_connect);
static void soj_copyarchive_script_on_error(struct so_job * job, struct so_database_connection * db_connect);
static void soj_copyarchive_script_post_run(struct so_job * job, struct so_database_connection * db_connect);
static bool soj_copyarchive_script_pre_run(struct so_job * job, struct so_database_connection * db_connect);

static struct so_job_driver soj_copyarchive = {
	.name = "copy-archive",

	.exit            = soj_copyarchive_exit,
	.run             = soj_copyarchive_run,
	.simulate        = soj_copyarchive_simulate,
	.script_on_error = soj_copyarchive_script_on_error,
	.script_post_run = soj_copyarchive_script_post_run,
	.script_pre_run  = soj_copyarchive_script_pre_run,
};


static void soj_copyarchive_exit(struct so_job * job __attribute__((unused)), struct so_database_connection * db_connect __attribute__((unused))) {
	if (data.src_archive != NULL)
		so_archive_free(data.src_archive);
	if (data.pool != NULL)
		so_pool_free(data.pool);
}

static void soj_copyarchive_init() {
	soj_job_register(&soj_copyarchive);

	bindtextdomain("storiqone-job-copy-archive", LOCALE_DIR);
}

static int soj_copyarchive_run(struct so_job * job, struct so_database_connection * db_connect) {
	job->done = 0.01;

	struct so_media * media = data.src_archive->volumes[0].media;

	data.src_drive = soj_media_find_and_load(media, false, 0, db_connect);
	if (data.src_drive == NULL) {
		so_job_add_record(job, db_connect, so_log_level_error,
			so_job_record_notif_important, dgettext("storiqone-job-format-media", "Failed to load media '%s'"),
			media->name);
		return 2;
	}

	data.media_iterator = soj_media_get_iterator(data.pool);
	struct so_value * vmedia = so_value_iterator_get_value(data.media_iterator, false);
	data.media = so_value_custom_get(vmedia);
	data.dest_drive = soj_media_load(data.media, true);

	if (data.dest_drive == NULL) {
	} else {
		// TODO: direct copy
		soj_copyarchive_indirect_copy(job, db_connect, &data);
	}

	return 0;
}

static int soj_copyarchive_simulate(struct so_job * job, struct so_database_connection * db_connect) {
	data.src_archive = db_connect->ops->get_archive_by_job(db_connect, job);
	if (data.src_archive == NULL) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-copy-archive", "Archive not found"));
		return 1;
	}
	if (data.src_archive->deleted) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-copy-archive", "Try to copy an deleted archive '%s'"),
			data.src_archive->name);
		return 1;
	}

	data.pool = db_connect->ops->get_pool(db_connect, NULL, job);
	if (data.pool == NULL) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-copy-archive", "Pool not found"));
		return 1;
	}
	if (data.pool->deleted) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-copy-archive", "Try to copy an archive '%s' into a pool '%s' which is deleted"),
			data.src_archive->name, data.pool->name);
		return 1;
	}

	if (!soj_changer_has_apt_drive(data.pool->format, true)) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-copy-archive", "Failed to find suitable drive to copy archive '%s'"),
			data.src_archive->name);
		return 1;
	}

	ssize_t reserved = soj_media_prepare(data.pool, data.src_archive->size, db_connect);
	if (reserved < data.src_archive->size) {
		reserved += soj_media_prepare_unformatted(data.pool, true, db_connect);

		if (reserved < data.src_archive->size)
			reserved += soj_media_prepare_unformatted(data.pool, false, db_connect);
	}

	if (reserved < data.src_archive->size) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-copy-archive", "Error: not enought space available into pool '%s'"),
			data.pool->name);
		return 1;
	}

	return 0;
}

static void soj_copyarchive_script_on_error(struct so_job * job, struct so_database_connection * db_connect) {
	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_on_error, data.pool) < 1)
		return;

	struct so_value * json = so_value_pack("{sosososo}",
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"archive", so_archive_convert(data.src_archive),
		"pool", so_pool_convert(data.pool)
	);

	struct so_value * returned = soj_script_run(db_connect, job, so_script_type_on_error, data.pool, json);
	so_value_free(json);
	so_value_free(returned);
}

static void soj_copyarchive_script_post_run(struct so_job * job, struct so_database_connection * db_connect) {
	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_post_job, data.pool) < 1)
		return;

	struct so_value * json = so_value_pack("{sosososo}",
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"archive", so_archive_convert(data.src_archive),
		"pool", so_pool_convert(data.pool)
	);

	struct so_value * returned = soj_script_run(db_connect, job, so_script_type_post_job, data.pool, json);
	so_value_free(json);
	so_value_free(returned);
}

static bool soj_copyarchive_script_pre_run(struct so_job * job, struct so_database_connection * db_connect) {
	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_pre_job, data.pool) < 1)
		return true;

	struct so_value * json = so_value_pack("{sosososo}",
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"archive", so_archive_convert(data.src_archive),
		"pool", so_pool_convert(data.pool)
	);

	struct so_value * returned = soj_script_run(db_connect, job, so_script_type_pre_job, data.pool, json);
	so_value_free(json);

	bool should_run = false;
	so_value_unpack(returned, "{sb}", "should run", &should_run);
	so_value_free(returned);

	return should_run;
}
