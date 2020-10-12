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
// strcmp
#include <string.h>
// sleep
#include <unistd.h>

#include <libstoriqone/archive.h>
#include <libstoriqone/database.h>
#include <libstoriqone/json.h>
#include <libstoriqone/host.h>
#include <libstoriqone/log.h>
#include <libstoriqone/media.h>
#include <libstoriqone/value.h>
#include <libstoriqone-job/job.h>
#include <libstoriqone-job/script.h>

#include "common.h"

#include <job_restore-archive.chcksum>

#include "config.h"

static struct so_archive * archive = NULL;
static bool has_selected_path = false;
static char * restore_path = NULL;
static struct so_value * selected_path = NULL;
static int restore_min_version = -1, restore_max_version = -1;

static void soj_restorearchive_exit(struct so_job * job, struct so_database_connection * db_connect);
static void soj_restorearchive_init(void) __attribute__((constructor));
static int soj_restorearchive_run(struct so_job * job, struct so_database_connection * db_connect);
static void soj_restorearchive_script_on_error(struct so_job * job, struct so_database_connection * db_connect);
static void soj_restorearchive_script_post_run(struct so_job * job, struct so_database_connection * db_connect);
static bool soj_restorearchive_script_pre_run(struct so_job * job, struct so_database_connection * db_connect);
static int soj_restorearchive_warm_up(struct so_job * job, struct so_database_connection * db_connect);

static struct so_job_driver soj_restorearchive = {
	.name = "restore-archive",

	.exit            = soj_restorearchive_exit,
	.run             = soj_restorearchive_run,
	.script_on_error = soj_restorearchive_script_on_error,
	.script_post_run = soj_restorearchive_script_post_run,
	.script_pre_run  = soj_restorearchive_script_pre_run,
	.warm_up         = soj_restorearchive_warm_up,
};


static void soj_restorearchive_exit(struct so_job * job __attribute__((unused)), struct so_database_connection * db_connect __attribute__((unused))) {
	if (archive != NULL)
		so_archive_free(archive);
	free(restore_path);
	so_value_free(selected_path);
}

static void soj_restorearchive_init() {
	soj_job_register(&soj_restorearchive);

	bindtextdomain("storiqone-job-restore-archive", LOCALE_DIR);
}

static int soj_restorearchive_run(struct so_job * job, struct so_database_connection * db_connect) {
	struct soj_restorearchive_data_worker * workers = NULL, * last_worker = NULL;
	size_t total_size = 0;
	unsigned int i;

	struct so_value * last_files = so_value_new_hashtable2();
	for (i = 0; i < archive->nb_volumes; i++) {
		unsigned int j;
		struct so_archive_volume * vol = archive->volumes + i;
		for (j = 0; j < vol->nb_files; j++) {
			struct so_archive_files * ptr_file = vol->files + j;
			struct so_archive_file * file = ptr_file->file;

			file->skip_restore = restore_min_version > file->max_version || restore_max_version < file->min_version;
			if (file->skip_restore)
				continue;

			file->skip_restore = !soj_restorearchive_path_filter(file->path);
			if (file->skip_restore)
				continue;

			if (so_value_hashtable_has_key2(last_files, file->path)) {
				struct so_value * vfile = so_value_hashtable_get2(last_files, file->path, false, false);
				struct so_archive_file * previous_file = so_value_custom_get(vfile);

				if (previous_file->size != file->size || previous_file->modify_time < file->modify_time) {
					previous_file->skip_restore = true;
					so_value_hashtable_put2(last_files, file->path, so_value_new_custom(file, NULL), true);
					total_size += file->size - previous_file->size;
				}
			} else {
				so_value_hashtable_put2(last_files, file->path, so_value_new_custom(file, NULL), true);
				total_size += file->size;
			}
		}
	}
	so_value_free(last_files);
	last_files = NULL;

	for (i = 0; i < archive->nb_volumes; i++) {
		struct so_archive_volume * vol = archive->volumes + i;
		bool found = false;

		unsigned int j;
		for (j = 0; !found && j < vol->nb_files; j++) {
			struct so_archive_files * ptr_file = vol->files + j;
			struct so_archive_file * file = ptr_file->file;

			if (!file->skip_restore)
				found = true;
		}

		vol->skip_volume = !found;
		if (!found)
			continue;

		struct soj_restorearchive_data_worker * ptr;
		for (ptr = workers, found = false; !found && ptr != NULL; ptr = ptr->next)
			if (strcmp(ptr->media->medium_serial_number, vol->media->medium_serial_number) == 0) {
				soj_restorearchive_data_worker_add_files(ptr, vol);
				found = true;
			}

		if (!found) {
			last_worker = soj_restorearchive_data_worker_new(archive, vol, db_connect->config, last_worker);

			if (workers == NULL)
				workers = last_worker;
		}
	}

	job->done = 0.01;

	if (total_size == 0) {
		job->done = 1;
		return 0;
	}

	soj_restorearchive_check_worker_start(db_connect->config);
	soj_restorearchive_data_worker_start(workers, job, db_connect);

	unsigned int nb_errors = 0, nb_warnings = 0;
	bool finished = false;
	while (!finished) {
		size_t total_done = 0;
		finished = true;
		nb_errors = 0;
		nb_warnings = 0;

		struct soj_restorearchive_data_worker * ptr;
		for (ptr = workers; ptr != NULL; ptr = ptr->next) {
			total_done += ptr->total_restored;

			if (ptr->status == so_job_status_running)
				finished = false;

			nb_errors += ptr->nb_errors;
			nb_warnings += ptr->nb_warnings;
		}

		float done = total_done;
		job->done = 0.01 + 0.99 * done / total_size;

		if (!finished)
			sleep(5);
	}

	if (!job->stopped_by_user)
		job->done = 1;

	soj_restorearchive_check_worker_stop();

	if (nb_errors > 0)
		job->status = so_job_status_error;
	else if (nb_warnings)
		job->status = so_job_status_finished_with_warnings;
	else
		job->status = so_job_status_finished;

	struct so_value * report = so_value_pack("{sisososOsO}",
		"report version", 2,
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"archive", so_archive_convert(archive),
		"selected paths", selected_path
	);

	char * json = so_json_encode_to_string(report);
	db_connect->ops->add_report(db_connect, job, archive, NULL, json);

	free(json);
	so_value_free(report);

	return 0;
}

static void soj_restorearchive_script_on_error(struct so_job * job, struct so_database_connection * db_connect) {
	struct so_pool * pool = archive->volumes->media->pool;

	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_on_error, pool) < 1)
		return;

	struct so_value * json = so_value_pack("{sosososssO}",
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"archive", so_archive_convert(archive),
		"restore path", restore_path,
		"selected path", selected_path
	);

	struct so_value * returned = soj_script_run(db_connect, job, so_script_type_on_error, pool, json);
	so_value_free(json);
	so_value_free(returned);
}

static void soj_restorearchive_script_post_run(struct so_job * job, struct so_database_connection * db_connect) {
	struct so_pool * pool = archive->volumes->media->pool;

	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_post_job, pool) < 1)
		return;

	struct so_value * json = so_value_pack("{sosososssO}",
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"archive", so_archive_convert(archive),
		"restore path", restore_path,
		"selected path", selected_path
	);

	struct so_value * returned = soj_script_run(db_connect, job, so_script_type_post_job, pool, json);
	so_value_free(json);
	so_value_free(returned);
}

static bool soj_restorearchive_script_pre_run(struct so_job * job, struct so_database_connection * db_connect) {
	struct so_pool * pool = archive->volumes->media->pool;

	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_pre_job, pool) < 1)
		return true;

	struct so_value * json = so_value_pack("{sosososssO}",
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"archive", so_archive_convert(archive),
		"restore path", restore_path,
		"selected path", selected_path
	);

	struct so_value * returned = soj_script_run(db_connect, job, so_script_type_pre_job, pool, json);
	so_value_free(json);

	bool should_run = false;
	so_value_unpack(returned, "{sb}", "should run", &should_run);

	so_value_free(returned);

	return should_run;
}

static int soj_restorearchive_warm_up(struct so_job * job, struct so_database_connection * db_connect) {
	archive = db_connect->ops->get_archive_by_job(db_connect, job);
	if (archive == NULL) {
		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-storiqone-job-restore-archive", "Archive not found"));
		return 1;
	}
	if (archive->deleted) {
		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-storiqone-job-restore-archive", "No restoration for deleted archive '%s'"),
			archive->name);
		return 1;
	}

	restore_path = db_connect->ops->get_restore_path(db_connect, job);
	if (restore_path != NULL && restore_path[0] != '/') {
		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-storiqone-job-restore-archive", "Restore path should be absolute (starts with '/')"));
		return 2;
	}

	selected_path = db_connect->ops->get_selected_files_by_job(db_connect, job);
	if (selected_path == NULL)
		selected_path = so_value_new_linked_list();

	has_selected_path = so_value_list_get_length(selected_path) > 0;

	if (so_value_hashtable_has_key2(job->option, "restore_version") && (so_value_hashtable_has_key2(job->option, "restore_min_version") || so_value_hashtable_has_key2(job->option, "restore_max_version"))) {
		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-restore-archive", "You cannot have \"restore_version\" option with \"restore_min_version\" or \"restore_max_version\""));
		return 1;
	}

	if (so_value_unpack(job->option, "{si}", "restore_min_version", &restore_min_version) == 1 && restore_min_version < 1) {
		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-restore-archive", "job option \"restore_min_version\" should be greater or equal to 1"));
		return 1;
	}

	if (so_value_unpack(job->option, "{si}", "restore_max_version", &restore_max_version) == 1 && restore_max_version < 1) {
		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-restore-archive", "job option \"restore_max_version\" should be greater or equal to 1"));
		return 1;
	}

	if (restore_max_version > 0 && restore_max_version < restore_min_version) {
		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-restore-archive", "job option \"restore_max_version\" should be greater or equal to \"restore_min_version\""));
		return 1;
	}

	if (so_value_unpack(job->option, "{si}", "restore_version", &restore_min_version) == 1) {
		restore_max_version = restore_min_version;

		if (restore_min_version == -1 || restore_min_version == 0) {
			unsigned int i;
			for (i = 0; i < archive->nb_volumes; i++) {
				struct so_archive_volume * vol = archive->volumes + i;
				restore_min_version = vol->max_version;
			}

			soj_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_normal,
				dgettext("storiqone-job-restore-archive", "Restore version set to %d"), restore_min_version);
		} else if (restore_min_version > 0) {
			int max_version = 0;
			unsigned int i;
			for (i = 0; i < archive->nb_volumes; i++) {
				struct so_archive_volume * vol = archive->volumes + i;
				max_version = vol->max_version;
			}

			if (restore_min_version > max_version) {
				soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
					dgettext("storiqone-job-restore-archive", "Wrong restore version \"%d\", should be between 1 and %d"), restore_min_version, max_version);
				return 2;
			}
		} else {
			soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
				dgettext("storiqone-job-restore-archive", "Wrong restore version \"%d\", should be greater or equal than -1"), restore_min_version);
			return 2;
		}
	}

	if (restore_min_version == -1)
		restore_min_version = 1;

	if (restore_max_version == -1) {
		unsigned int i;
		for (i = 0; i < archive->nb_volumes; i++) {
			struct so_archive_volume * vol = archive->volumes + i;
			restore_max_version = vol->max_version;
		}
	}

	if (restore_min_version != restore_max_version)
		soj_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_normal,
			dgettext("storiqone-job-restore-archive", "Will restore from version \"%d\" to \"%d\""), restore_min_version, restore_max_version);
	else
		soj_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_normal,
			dgettext("storiqone-job-restore-archive", "Will restore version \"%d\""), restore_min_version);

	soj_restorearchive_path_init(restore_path, selected_path);

	return 0;
}
