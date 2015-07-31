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

#include <libstoriqone/archive.h>
#include <libstoriqone/database.h>
#include <libstoriqone/host.h>
#include <libstoriqone/json.h>
#include <libstoriqone/log.h>
#include <libstoriqone/media.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/value.h>
#include <libstoriqone-job/changer.h>
#include <libstoriqone-job/job.h>
#include <libstoriqone-job/script.h>

#include <job_synchronize-archive.chcksum>

#include "config.h"

static struct so_archive * soj_synchronizearchive_src_archive = NULL;
static struct so_archive * soj_synchronizearchive_dest_archive = NULL;
static struct so_pool * soj_synchronizearchive_pool = NULL;
static struct so_value * soj_synchronizearchive_synchronized_archives = NULL;

static void soj_synchronizearchive_exit(struct so_job * job, struct so_database_connection * db_connect);
static void soj_synchronizearchive_init(void) __attribute__((constructor));
static int soj_synchronizearchive_run(struct so_job * job, struct so_database_connection * db_connect);
static int soj_synchronizearchive_simulate(struct so_job * job, struct so_database_connection * db_connect);
static void soj_synchronizearchive_script_on_error(struct so_job * job, struct so_database_connection * db_connect);
static void soj_synchronizearchive_script_post_run(struct so_job * job, struct so_database_connection * db_connect);
static bool soj_synchronizearchive_script_pre_run(struct so_job * job, struct so_database_connection * db_connect);

static struct so_job_driver soj_synchronizearchive = {
	.name = "synchronize-archive",

	.exit            = soj_synchronizearchive_exit,
	.run             = soj_synchronizearchive_run,
	.simulate        = soj_synchronizearchive_simulate,
	.script_on_error = soj_synchronizearchive_script_on_error,
	.script_post_run = soj_synchronizearchive_script_post_run,
	.script_pre_run  = soj_synchronizearchive_script_pre_run,
};


static void soj_synchronizearchive_exit(struct so_job * job __attribute__((unused)), struct so_database_connection * db_connect __attribute__((unused))) {
	so_archive_free(soj_synchronizearchive_src_archive);
	so_archive_free(soj_synchronizearchive_dest_archive);
	so_value_free(soj_synchronizearchive_synchronized_archives);
}

static void soj_synchronizearchive_init() {
	soj_job_register(&soj_synchronizearchive);

	bindtextdomain("storiqone-job-synchronize-archive", LOCALE_DIR);
}

static int soj_synchronizearchive_run(struct so_job * job, struct so_database_connection * db_connect) {
	return 0;
}

static int soj_synchronizearchive_simulate(struct so_job * job, struct so_database_connection * db_connect) {
	soj_synchronizearchive_dest_archive = db_connect->ops->get_archive_by_job(db_connect, job);

	if (soj_synchronizearchive_dest_archive->nb_volumes == 0) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-synchronize-archive", "Bug: archive '%s' do not contain files"),
			soj_synchronizearchive_dest_archive->name);

		return 1;
	}

	if (!soj_synchronizearchive_dest_archive->can_append) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-synchronize-archive", "Error, can't add file into an archive '%s' which forbid to add them"),
			soj_synchronizearchive_dest_archive->name);
		return 1;
	}

	if (db_connect->ops->is_archive_synchronized(db_connect, soj_synchronizearchive_dest_archive)) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-synchronize-archive", "Error, archive '%s' is already synchronized"),
			soj_synchronizearchive_dest_archive->name);
		return 1;
	}

	struct so_pool * pool = soj_synchronizearchive_pool = soj_synchronizearchive_dest_archive->volumes->media->pool;
	if (pool->deleted) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-synchronize-archive", "Try to create an archive '%s' into a pool '%s' which is deleted"),
			soj_synchronizearchive_dest_archive->name, pool->name);
		return 1;
	}

	soj_synchronizearchive_synchronized_archives = db_connect->ops->get_synchronized_archive(db_connect, soj_synchronizearchive_dest_archive);

	struct so_archive_volume * last_vol = soj_synchronizearchive_dest_archive->volumes + (soj_synchronizearchive_dest_archive->nb_volumes - 1);
	ssize_t new_volume_size = 0;

	struct so_value_iterator * iter = so_value_list_get_iterator(soj_synchronizearchive_synchronized_archives);
	while (so_value_iterator_has_next(iter)) {
		struct so_value * varchive = so_value_iterator_get_value(iter, false);
		soj_synchronizearchive_src_archive = so_value_custom_get(varchive);

		struct so_archive_volume * first_vol;
		unsigned int i;
		for (i = 0; i < soj_synchronizearchive_src_archive->nb_volumes; i++) {
			first_vol = soj_synchronizearchive_src_archive->volumes + i;

			if (first_vol->start_time > last_vol->end_time)
				break;
		}

		if (first_vol->start_time < last_vol->end_time)
			continue;

		bool ok = true;
		unsigned int j = i;
		new_volume_size = 0;
		for (; i < soj_synchronizearchive_src_archive->nb_volumes; i++) {
			struct so_archive_volume * vol = soj_synchronizearchive_src_archive->volumes + i;
			new_volume_size += vol->size;

			struct so_slot * sl = soj_changer_find_slot(vol->media);
			if (sl == NULL) {
				ok = false;
				break;
			}

			sl->changer->ops->reserve_media(sl->changer, sl, 0, so_pool_unbreakable_level_none);
		}

		if (!ok) {
			for (; j < i; j++) {
				struct so_archive_volume * vol = soj_synchronizearchive_src_archive->volumes + i;
				struct so_slot * sl = soj_changer_find_slot(vol->media);
				sl->changer->ops->release_media(sl->changer, sl);
			}
			soj_synchronizearchive_src_archive = NULL;
			continue;
		}
	}
	so_value_iterator_free(iter);

	return 0;
}

static void soj_synchronizearchive_script_on_error(struct so_job * job, struct so_database_connection * db_connect) {
	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_on_error, soj_synchronizearchive_pool) < 1)
		return;

	struct so_value * json = so_value_pack("{sosososO}",
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"archive", so_archive_convert(soj_synchronizearchive_dest_archive),
		"candidate archives", soj_synchronizearchive_synchronized_archives
	);

	struct so_value * returned = soj_script_run(db_connect, job, so_script_type_on_error, soj_synchronizearchive_pool, json);
	so_value_free(json);
	so_value_free(returned);
}

static void soj_synchronizearchive_script_post_run(struct so_job * job, struct so_database_connection * db_connect) {
	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_post_job, soj_synchronizearchive_pool) < 1)
		return;

	struct so_value * json = so_value_pack("{sosososO}",
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"archive", so_archive_convert(soj_synchronizearchive_dest_archive),
		"candidate archives", soj_synchronizearchive_synchronized_archives
	);

	struct so_value * returned = soj_script_run(db_connect, job, so_script_type_post_job, soj_synchronizearchive_pool, json);
	so_value_free(json);
	so_value_free(returned);
}

static bool soj_synchronizearchive_script_pre_run(struct so_job * job, struct so_database_connection * db_connect) {
	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_pre_job, soj_synchronizearchive_pool) < 1)
		return true;

	struct so_value * json = so_value_pack("{sosososO}",
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"archive", so_archive_convert(soj_synchronizearchive_dest_archive),
		"candidate archives", soj_synchronizearchive_synchronized_archives
	);

	struct so_value * returned = soj_script_run(db_connect, job, so_script_type_pre_job, soj_synchronizearchive_pool, json);
	so_value_free(json);

	bool should_run = false;
	so_value_unpack(returned, "{sb}", "should run", &should_run);
	so_value_free(returned);

	return should_run;
}

