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
// strcmp, strcpy
#include <string.h>
// uuid_generate, uuid_unparse_lower
#include <uuid/uuid.h>

#include <libstoriqone/archive.h>
#include <libstoriqone/database.h>
#include <libstoriqone/file.h>
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

#include <job_format-media.chcksum>

#include "config.h"

static struct so_media * soj_formatmedia_media = NULL;
static struct so_pool * soj_formatmedia_pool = NULL;
static struct so_value * soj_formatmedia_option = NULL;

static bool soj_formatmedia_check_block_size(struct so_job * job, ssize_t block_size, struct so_database_connection * db_connect);
static bool soj_formatmedia_check_partition_size(struct so_job * job, ssize_t partition_size, struct so_database_connection * db_connect);
static void soj_formatmedia_exit(struct so_job * job, struct so_database_connection * db_connect);
static void soj_formatmedia_init(void) __attribute__((constructor));
static int soj_formatmedia_run(struct so_job * job, struct so_database_connection * db_connect);
static void soj_formatmedia_script_on_error(struct so_job * job, struct so_database_connection * db_connect);
static void soj_formatmedia_script_post_run(struct so_job * job, struct so_database_connection * db_connect);
static bool soj_formatmedia_script_pre_run(struct so_job * job, struct so_database_connection * db_connect);
static int soj_formatmedia_warm_up(struct so_job * job, struct so_database_connection * db_connect);

static struct so_job_driver soj_formatmedia = {
	.name = "format-media",

	.exit            = soj_formatmedia_exit,
	.run             = soj_formatmedia_run,
	.script_on_error = soj_formatmedia_script_on_error,
	.script_post_run = soj_formatmedia_script_post_run,
	.script_pre_run  = soj_formatmedia_script_pre_run,
	.warm_up         = soj_formatmedia_warm_up,
};


static bool soj_formatmedia_check_block_size(struct so_job * job, ssize_t block_size, struct so_database_connection * db_connect) {
	if (block_size < 1024 || block_size > 1048576) {
		soj_job_add_record(job, db_connect, so_log_level_critical, so_job_record_notif_normal,
			dgettext("storiqone-job-format-media", "parameter 'block size' should be a positive integer (not %zd) and between 1 KB and 1 MB"),
			block_size);

		return false;
	}

	// check if block_size is a power of two
	if ((block_size & (block_size - 1)) != 0) {
		soj_job_add_record(job, db_connect, so_log_level_critical, so_job_record_notif_normal,
			dgettext("storiqone-job-format-media", "parameter 'block size' should be a two of two, not %zd"),
			block_size);

		return false;
	}

	return true;
}

static bool soj_formatmedia_check_partition_size(struct so_job * job, ssize_t partition_size, struct so_database_connection * db_connect) {
	if (partition_size < soj_formatmedia_media->media_format->capacity * 3 / 4) {
		float ratio = partition_size;
		ratio /= soj_formatmedia_media->media_format->capacity;

		char buf_partition_size[16], buf_media_capacity[16];
		so_file_convert_size_to_string(partition_size, buf_partition_size, 16);
		so_file_convert_size_to_string(soj_formatmedia_media->media_format->capacity, buf_media_capacity, 16);

		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-format-media", "Partition size should be greater than 75%% of media capacity (%s / %s = %.0f)"),
			buf_partition_size, buf_media_capacity, ratio);

		return false;
	}

	return true;
}

static void soj_formatmedia_exit(struct so_job * job __attribute__((unused)), struct so_database_connection * db_connect __attribute__((unused))) {
	so_media_free(soj_formatmedia_media);
	soj_formatmedia_media = NULL;
	so_pool_free(soj_formatmedia_pool);
	soj_formatmedia_pool = NULL;
}

static void soj_formatmedia_init() {
	soj_job_register(&soj_formatmedia);

	bindtextdomain("storiqone-job-format-media", LOCALE_DIR);
}

static int soj_formatmedia_run(struct so_job * job, struct so_database_connection * db_connect) {
	job->done = 0.25;

	struct so_drive * drive = soj_media_find_and_load2(soj_formatmedia_media, soj_formatmedia_pool, false, soj_formatmedia_media->media_format->block_size, false, NULL, NULL, db_connect);
	if (drive == NULL) {
		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-format-media", "Failed to load media '%s'"),
			soj_formatmedia_media->name);
		return 2;
	}

	job->status = so_job_status_running;
	job->done = 0.5;

	drive->ops->sync(drive);

	if (job->stopped_by_user)
		return 1;

	// check for write lock
	if (drive->slot->media->write_lock) {
		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-format-media", "Trying to format a write protected media '%s'"),
			drive->slot->media->name);
		return 3;
	}

	soj_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_important,
		dgettext("storiqone-job-format-media", "Formatting media '%s' in progress"),
		soj_formatmedia_media->name);

	// write header
	int failed = drive->ops->format_media(drive, soj_formatmedia_pool, soj_formatmedia_option);
	if (failed != 0) {
		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-format-media", "Failed to format media"));
		return 4;
	}

	job->status = so_job_status_running;
	job->done = 0.75;

	// check header
	soj_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_important,
		dgettext("storiqone-job-format-media", "Checking media header '%s' in progress"),
		soj_formatmedia_media->name);

	if (drive->ops->check_header(drive)) {
		soj_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_important,
			dgettext("storiqone-job-format-media", "Checking media header '%s' was successful"),
			soj_formatmedia_media->name);

		job->status = so_job_status_running;
		job->done = 1;
		return 0;
	} else {
		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-format-media", "Checking media header '%s': failed"),
			soj_formatmedia_media->name);

		job->status = so_job_status_error;
		return 5;
	}
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

			const char * uuid = NULL;
			so_value_unpack(data, "{s{sS}}", "media", "uuid", &uuid);
			if (uuid != NULL) {
				uuid_t tmp;
				if (uuid_parse(uuid, tmp) != 0) {
					soj_job_add_record(job, db_connect, so_log_level_warning, so_job_record_notif_normal,
						dgettext("storiqone-job-format-media", "Invalid UUID provided by script, ignoring it"));
				} else {
					soj_job_add_record(job, db_connect, so_log_level_warning, so_job_record_notif_normal,
						dgettext("storiqone-job-format-media", "Script request to format media with this UUID (%s) instead of (%s)"),
						uuid, soj_formatmedia_media->uuid);
					strcpy(soj_formatmedia_media->uuid, uuid);
				}
			}

			char * name = NULL;
			so_value_unpack(data, "{s{ss}}", "media", "name", &name);
			if (name != NULL) {
				soj_job_add_record(job, db_connect, so_log_level_warning, so_job_record_notif_normal,
					dgettext("storiqone-job-format-media", "Script request to format media with this name (%s) instead of (%s)"),
					name, soj_formatmedia_media->name);

				free(soj_formatmedia_media->name);
				soj_formatmedia_media->name = name;
			}
		}
		so_value_iterator_free(iter);
	}

	so_value_free(returned);

	return should_run;
}

static int soj_formatmedia_warm_up(struct so_job * job, struct so_database_connection * db_connect) {
	soj_formatmedia_media = db_connect->ops->get_media(db_connect, NULL, NULL, job);
	if (soj_formatmedia_media == NULL) {
		soj_job_add_record(job, db_connect, so_log_level_critical, so_job_record_notif_important,
			dgettext("storiqone-job-format-media", "No media linked to the current job"));
		return 1;
	}
	if (soj_formatmedia_media->type == so_media_type_cleaning) {
		soj_job_add_record(job, db_connect, so_log_level_critical, so_job_record_notif_important,
			dgettext("storiqone-job-format-media", "Trying to format a cleaning media '%s'"),
			soj_formatmedia_media->name);
		return 1;
	}
	if (soj_formatmedia_media->type == so_media_type_worm && soj_formatmedia_media->nb_volumes > 0) {
		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-format-media", "Trying to format a worm media '%s' containing data"),
			soj_formatmedia_media->name);
		return 1;
	}
	if (soj_formatmedia_media->pool != NULL) {
		soj_job_add_record(job, db_connect, so_log_level_warning, so_job_record_notif_important,
			dgettext("storiqone-job-format-media", "Trying to format a media '%s' which is member of pool '%s'"),
			soj_formatmedia_media->name, soj_formatmedia_media->pool->name);
		return 1;
	}
	if (soj_formatmedia_media->status == so_media_status_error)
		soj_job_add_record(job, db_connect, so_log_level_warning, so_job_record_notif_important,
			dgettext("storiqone-job-format-media", "Trying to format a media '%s' with error status"),
			soj_formatmedia_media->name);

	soj_formatmedia_pool = db_connect->ops->get_pool(db_connect, NULL, job);
	if (soj_formatmedia_pool == NULL) {
		soj_job_add_record(job, db_connect, so_log_level_critical, so_job_record_notif_important,
			dgettext("storiqone-job-format-media", "No pool related to this job"));
		return 1;
	}
	if (soj_formatmedia_pool->deleted) {
		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-format-media", "Trying to format a media '%s' to a deleted pool '%s'"),
			soj_formatmedia_media->name, soj_formatmedia_pool->name);
		return 1;
	}

	if (strcmp(soj_formatmedia_pool->archive_format->name, "LTFS") == 0 && !soj_formatmedia_media->media_format->support_partition) {
		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-format-media", "Trying to format a LTFS media '%s' which does not support partition (LTFS requirement)"),
			soj_formatmedia_media->name);
		return 1;
	}
	if (!soj_formatmedia_pool->archive_format->writable) {
		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-format-media", "No write support for format '%s'"),
			soj_formatmedia_pool->archive_format->name);
		return 1;
	}

	if (so_media_format_cmp(soj_formatmedia_media->media_format, soj_formatmedia_pool->media_format) != 0) {
		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-format-media", "Trying to format a media '%s' (media format: %s) not matching pool '%s' (pool format: %s)"),
			soj_formatmedia_media->name, soj_formatmedia_media->media_format->name, soj_formatmedia_pool->name, soj_formatmedia_pool->media_format->name);
		return 1;
	}

	if (!soj_changer_has_apt_drive(soj_formatmedia_media->media_format, true)) {
		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-format-media", "Failed to find a suitable drive to format media '%s'"),
			soj_formatmedia_media->name);
		return 1;
	}

	soj_formatmedia_option = so_value_new_hashtable2();
	ssize_t block_size = 0;

	if (so_value_valid(job->option, "{ss}", "block size")) {
		const char * action = NULL;
		so_value_unpack(job->option, "{sS}", "block size", &action);

		struct so_media_format * format = soj_formatmedia_media->media_format;
		char buf_size[16];

		if (strcmp(action, "default") == 0) {
			block_size = format->block_size;

			so_file_convert_size_to_string(block_size, buf_size, 15);

			soj_job_add_record(job, db_connect, so_log_level_notice, so_job_record_notif_normal,
				dgettext("storiqone-job-format-media", "Will use default block size '%s' defined by media format '%s'"),
				buf_size, format->name);
		} else if (strcmp(action, "auto") == 0) {
			if (soj_formatmedia_media->type == so_media_type_worm) {
				block_size = format->block_size;

				so_file_convert_size_to_string(block_size, buf_size, 15);

				soj_job_add_record(job, db_connect, so_log_level_warning, so_job_record_notif_normal,
					dgettext("storiqone-job-format-media", "Will use default block size '%s' defined by media format '%s' because type of media '%s' is WORM"),
					buf_size, format->name, soj_formatmedia_media->name);
			} else {
				block_size = 0;

				soj_job_add_record(job, db_connect, so_log_level_notice, so_job_record_notif_normal,
					dgettext("storiqone-job-format-media", "Will look for the best block size to use with media '%s'"),
					soj_formatmedia_media->name);
			}
		} else {
			block_size = so_file_parse_size(action);

			if (!soj_formatmedia_check_block_size(job, block_size, db_connect)) {
				soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
					dgettext("storiqone-job-format-media", "Invalid block size '%s'"),
					action);

				return 1;
			}
		}

		so_value_hashtable_put2(soj_formatmedia_option, "block size", so_value_new_integer(block_size), true);
	} else if (so_value_valid(job->option, "{sz}", "block size")) {
		so_value_unpack(job->option, "{sz}", "block size", &block_size);
		if (block_size < 0) {
			soj_job_add_record(job, db_connect, so_log_level_critical, so_job_record_notif_normal,
				dgettext("storiqone-job-format-media", "parameter 'block size' should be a positive integer (not %zd)"),
				block_size);

			return 1;
		}

		if (soj_formatmedia_check_block_size(job, block_size, db_connect))
			so_value_hashtable_put2(soj_formatmedia_option, "block size", so_value_new_integer(block_size), true);
		else
			return 1;


	}

	if (so_value_valid(job->option, "{ss}", "partition size")) {
		char * str_partition_size = NULL;
		so_value_unpack(job->option, "{ss}", "partition size", &str_partition_size);

		if (str_partition_size != NULL) {
			ssize_t partition_size = so_file_parse_size(str_partition_size);
			free(str_partition_size);

			if (soj_formatmedia_check_partition_size(job, partition_size, db_connect))
				so_value_hashtable_put2(soj_formatmedia_option, "partition size", so_value_new_integer(partition_size), true);
			else
				return 1;
		}
	} else if (so_value_valid(job->option, "{sz}", "partition size")) {
		ssize_t partition_size = 0;
		so_value_unpack(job->option, "{sz}", "partition size", &partition_size);

		if (soj_formatmedia_check_partition_size(job, partition_size, db_connect))
			so_value_hashtable_put2(soj_formatmedia_option, "partition size", so_value_new_integer(partition_size), true);
		else
			return 1;
	}

	return 0;
}
