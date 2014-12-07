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
*  Copyright (C) 2014, Clercin guillaume <gclercin@intellique.com>           *
\****************************************************************************/

// bindtextdomain, dgettext
#include <libintl.h>
// free
#include <stdlib.h>
// sleep
#include <unistd.h>

#include <libstoriqone/database.h>
#include <libstoriqone/file.h>
#include <libstoriqone/log.h>
#include <libstoriqone/media.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/value.h>
#include <libstoriqone-job/changer.h>
#include <libstoriqone-job/drive.h>
#include <libstoriqone-job/io.h>
#include <libstoriqone-job/job.h>
#include <libstoriqone-job/media.h>
#include <libstoriqone-job/script.h>

#include <job_backup-db.chcksum>

#include "config.h"

static struct so_value * backupdb_checksums = NULL;
static struct so_value * backupdb_medias = NULL;
static struct so_pool * backupdb_pool = NULL;

static void backupdb_exit(struct so_job * job, struct so_database_connection * db_connect);
static void backupdb_init(void) __attribute__((constructor));
static int backupdb_run(struct so_job * job, struct so_database_connection * db_connect);
static int backupdb_simulate(struct so_job * job, struct so_database_connection * db_connect);
static void backupdb_script_on_error(struct so_job * job, struct so_database_connection * db_connect);
static void backupdb_script_post_run(struct so_job * job, struct so_database_connection * db_connect);
static bool backupdb_script_pre_run(struct so_job * job, struct so_database_connection * db_connect);

static struct so_job_driver backupdb = {
	.name = "backup-db",

	.exit            = backupdb_exit,
	.run             = backupdb_run,
	.simulate        = backupdb_simulate,
	.script_on_error = backupdb_script_on_error,
	.script_post_run = backupdb_script_post_run,
	.script_pre_run  = backupdb_script_pre_run,
};


static void backupdb_exit(struct so_job * job __attribute__((unused)), struct so_database_connection * db_connect __attribute__((unused))) {
	so_value_free(backupdb_checksums);
	backupdb_checksums = NULL;
	so_value_free(backupdb_medias);
	backupdb_medias = NULL;
	so_pool_free(backupdb_pool);
	backupdb_pool = NULL;
}

static void backupdb_init() {
	soj_job_register(&backupdb);

	bindtextdomain("storiqone-job-backup-db", LOCALE_DIR);
}

static int backupdb_run(struct so_job * job, struct so_database_connection * db_connect) {
	struct so_stream_reader * db_reader = db_connect->config->ops->backup_db(db_connect->config);
	if (db_reader == NULL) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important, dgettext("storiqone-job-backup-db", "Failed to get database hander"));
		return 1;
	}

	struct so_stream_writer * tmp_writer = so_io_tmp_writer();
	if (tmp_writer == NULL) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important, dgettext("storiqone-job-backup-db", "Failed to create temporary file"));
		goto tmp_writer;
	}

	if (job->stopped_by_user)
		goto tmp_writer;

	job->status = so_job_status_running;
	job->done = 0.01;

	// write backup into temporary file
	static char buffer[16384];
	ssize_t nb_read, nb_total_read = 0;
	while (nb_read = db_reader->ops->read(db_reader, buffer, 16384), nb_read > 0) {
		nb_total_read += nb_read;

		ssize_t nb_total_write = 0;
		while (nb_total_write < nb_read) {
			ssize_t nb_write = tmp_writer->ops->write(tmp_writer, buffer + nb_total_write, nb_read - nb_total_write);
			if (nb_write < 0) {
				so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important, dgettext("storiqone-job-backup-db", "Error while writting into temporary file"));
				goto tmp_writer;
			} else {
				nb_total_write += nb_write;
			}
		}
	}

	if (nb_read < 0) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important, dgettext("storiqone-job-backup-db", "Error while reading from database"));
		goto tmp_writer;
	}

	db_reader->ops->close(db_reader);
	db_reader->ops->free(db_reader);
	db_reader = NULL;

	job->done = 0.02;

	ssize_t size_available = soj_media_prepare(backupdb_pool, db_connect);
	if (size_available < nb_total_read) {
		size_available += soj_media_prepare_unformatted(backupdb_pool, true, db_connect);

		if (size_available < nb_total_read)
			size_available += soj_media_prepare_unformatted(backupdb_pool, false, db_connect);

		if (size_available < nb_total_read)
			goto tmp_writer;
	}

	struct so_stream_reader * tmp_reader = tmp_writer->ops->reopen(tmp_writer);
	tmp_writer->ops->free(tmp_writer);

	struct so_value * reserved_medias = soj_media_reserve(backupdb_pool, nb_total_read, so_pool_unbreakable_level_none);
	struct so_value_iterator * iter = so_value_list_get_iterator(reserved_medias);
	while (so_value_iterator_has_next(iter)) {
		struct so_value * vmedia = so_value_iterator_get_value(iter, false);
		struct so_media * media = so_value_custom_get(vmedia);

		struct so_drive * drive = soj_media_load(media);
		// TODO: when drive is null

		const char * cookie = drive->ops->lock(drive);
		struct so_stream_writer * dr_writer = drive->ops->get_raw_writer(drive, cookie);
		struct so_stream_writer * cksum_writer = dr_writer;

		if (so_value_list_get_length(backupdb_checksums) > 0)
			cksum_writer = soj_checksum_writer_new(dr_writer, backupdb_checksums, true);

		off_t position = 0;
		while (nb_read = tmp_reader->ops->read(tmp_reader, buffer, 16384), nb_read > 0) {
			ssize_t nb_total_write = 0;
			while (nb_total_write < nb_read) {
				ssize_t nb_write = cksum_writer->ops->write(cksum_writer, buffer, nb_read);
				if (nb_write < 0) {
				} else {
					nb_total_write += nb_write;
					position += nb_write;
				}
			}

			job->done = 0.02 + 0.98 * ((float) (position)) / nb_total_read;
		}

		if (nb_read < 0) {
		}

		cksum_writer->ops->close(cksum_writer);

		if (dr_writer != cksum_writer) {
			struct so_value * checksums = soj_checksum_writer_get_checksums(cksum_writer);
			so_value_free(checksums);
		}

		cksum_writer->ops->free(cksum_writer);
	}
	so_value_iterator_free(iter);

	tmp_reader->ops->close(tmp_reader);
	tmp_reader->ops->free(tmp_reader);

	job->done = 1;

	return 0;

tmp_writer:
	if (tmp_writer != NULL) {
		tmp_writer->ops->close(tmp_writer);
		tmp_writer->ops->free(tmp_writer);
	}
	if (db_reader != NULL) {
		db_reader->ops->close(db_reader);
		db_reader->ops->free(db_reader);
	}
	return 1;
}

static int backupdb_simulate(struct so_job * job, struct so_database_connection * db_connect) {
	so_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_important, dgettext("storiqone-job-backup-db", "Start (simulation) format media job (job name: %s), key: %s, num runs %ld"), job->name, job->key, job->num_runs);

	backupdb_pool = db_connect->ops->get_pool(db_connect, "d9f976d4-e087-4d0a-ab79-96267f6613f0", NULL);
	if (backupdb_pool == NULL) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important, dgettext("storiqone-job-backup-db", "Pool (Storiq one database backup) not found"));
		return 1;
	}
	if (backupdb_pool->deleted) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important, dgettext("storiqone-job-backup-db", "Try to back up database into a pool which is deleted"));
		return 1;
	}

	backupdb_medias = db_connect->ops->get_medias_of_pool(db_connect, backupdb_pool);
	if (!backupdb_pool->growable && so_value_list_get_length(backupdb_medias) == 0) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important, dgettext("storiqone-job-backup-db", "Try to back up database into a pool whose is not growable and without any medias"));
		return 1;
	}

	backupdb_checksums = db_connect->ops->get_checksums_from_pool(db_connect, backupdb_pool);

	return 0;
}

static void backupdb_script_on_error(struct so_job * job, struct so_database_connection * db_connect) {
	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_on_error, backupdb_pool) < 1)
		return;
}

static void backupdb_script_post_run(struct so_job * job, struct so_database_connection * db_connect) {
	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_on_error, backupdb_pool) < 1)
		return;
}

static bool backupdb_script_pre_run(struct so_job * job, struct so_database_connection * db_connect) {
	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_pre_job, backupdb_pool) < 1)
		return true;

	return true;
}

