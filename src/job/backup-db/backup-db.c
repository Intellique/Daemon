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
// sleep
#include <unistd.h>

#include <libstoriqone/database.h>
#include <libstoriqone/file.h>
#include <libstoriqone/host.h>
#include <libstoriqone/io.h>
#include <libstoriqone/log.h>
#include <libstoriqone/media.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/value.h>
#include <libstoriqone-job/backup.h>
#include <libstoriqone-job/changer.h>
#include <libstoriqone-job/drive.h>
#include <libstoriqone-job/job.h>
#include <libstoriqone-job/media.h>
#include <libstoriqone-job/script.h>

#include <job_backup-db.chcksum>

#include "config.h"

static struct so_backup * soj_backupdb_backup = NULL;
static struct so_value * soj_backupdb_checksums = NULL;
static struct so_value * soj_backupdb_medias = NULL;
static struct so_pool * soj_backupdb_pool = NULL;

static void soj_backupdb_exit(struct so_job * job, struct so_database_connection * db_connect);
static void soj_backupdb_init(void) __attribute__((constructor));
static int soj_backupdb_run(struct so_job * job, struct so_database_connection * db_connect);
static int soj_backupdb_simulate(struct so_job * job, struct so_database_connection * db_connect);
static void soj_backupdb_script_on_error(struct so_job * job, struct so_database_connection * db_connect);
static void soj_backupdb_script_post_run(struct so_job * job, struct so_database_connection * db_connect);
static bool soj_backupdb_script_pre_run(struct so_job * job, struct so_database_connection * db_connect);

static struct so_job_driver backupdb = {
	.name = "backup-db",

	.exit            = soj_backupdb_exit,
	.run             = soj_backupdb_run,
	.simulate        = soj_backupdb_simulate,
	.script_on_error = soj_backupdb_script_on_error,
	.script_post_run = soj_backupdb_script_post_run,
	.script_pre_run  = soj_backupdb_script_pre_run,
};


static void soj_backupdb_exit(struct so_job * job __attribute__((unused)), struct so_database_connection * db_connect __attribute__((unused))) {
	so_value_free(soj_backupdb_checksums);
	soj_backupdb_checksums = NULL;
	so_value_free(soj_backupdb_medias);
	soj_backupdb_medias = NULL;
	so_pool_free(soj_backupdb_pool);
	soj_backupdb_pool = NULL;
	soj_backup_free(soj_backupdb_backup);
}

static void soj_backupdb_init() {
	soj_job_register(&backupdb);

	bindtextdomain("storiqone-job-backup-db", LOCALE_DIR);
}

static int soj_backupdb_run(struct so_job * job, struct so_database_connection * db_connect) {
	struct so_stream_reader * db_reader = db_connect->config->ops->backup_db(db_connect->config);
	if (db_reader == NULL) {
		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-backup-db", "Failed to get database handle"));
		return 1;
	}

	struct so_stream_writer * tmp_writer = so_io_tmp_writer();
	if (tmp_writer == NULL) {
		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-backup-db", "Failed to create temporary file"));
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
				soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
					dgettext("storiqone-job-backup-db", "Error while writting into temporary file because %m"));
				goto tmp_writer;
			} else {
				nb_total_write += nb_write;
			}

			if (job->stopped_by_user)
				goto tmp_writer;
		}
	}

	if (nb_read < 0) {
		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-backup-db", "Error while reading from database because %m"));
		goto tmp_writer;
	}

	ssize_t backup_size = db_reader->ops->position(db_reader);

	db_reader->ops->close(db_reader);
	db_reader->ops->free(db_reader);
	db_reader = NULL;

	job->done = 0.02;

	enum {
		copy_backup,
		get_media,
		reserve_media,
	} state = reserve_media;

	bool stop = false;
	ssize_t size_available;
	struct so_stream_reader * tmp_reader = tmp_writer->ops->reopen(tmp_writer);
	struct so_drive * drive = NULL;

	tmp_writer->ops->free(tmp_writer);
	tmp_writer = NULL;

	while (!stop && !job->stopped_by_user) {
		switch (state) {
			case copy_backup: {
					struct so_stream_writer * dr_writer = drive->ops->get_raw_writer(drive);
					struct so_stream_writer * cksum_writer = dr_writer;

					if (so_value_list_get_length(soj_backupdb_checksums) > 0)
						cksum_writer = so_io_checksum_writer_new(dr_writer, soj_backupdb_checksums, true);

					off_t position = 0;
					while (nb_read = tmp_reader->ops->read(tmp_reader, buffer, 16384), nb_read > 0) {
						ssize_t nb_total_write = 0;
						while (nb_total_write < nb_read) {
							ssize_t nb_write = cksum_writer->ops->write(cksum_writer, buffer, nb_read);
							if (nb_write < 0) {
								soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
									dgettext("storiqone-job-backup-db", "Error while writing to drive because %m"));
							} else {
								nb_total_write += nb_write;
								position += nb_write;
							}
						}

						job->done = 0.02 + 0.98 * ((float) (position)) / nb_total_read;

						if (job->stopped_by_user)
							goto tmp_writer;
					}

					if (nb_read < 0) {
						soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
							dgettext("storiqone-job-backup-db", "Error while reading from temporary file because %m"));
					}

					cksum_writer->ops->close(cksum_writer);

					int file_position = cksum_writer->ops->file_position(cksum_writer);
					ssize_t size = cksum_writer->ops->position(cksum_writer);

					struct so_media * media = drive->slot->media;
					if (dr_writer != cksum_writer) {
						struct so_value * checksums = so_io_checksum_writer_get_checksums(cksum_writer);
						soj_backup_add_volume(soj_backupdb_backup, media, size, file_position, checksums);
					} else
						soj_backup_add_volume(soj_backupdb_backup, media, size, file_position, NULL);

					cksum_writer->ops->free(cksum_writer);

					stop = true;
				}
				break;

			case get_media:
				drive = soj_media_find_and_load_next(soj_backupdb_pool, false, db_connect);

				if (drive != NULL)
					state = copy_backup;
				else {
					stop = true;
					job->status = so_job_status_error;

					soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
						dgettext("storiqone-job-backup-db", "No media found, aborting database backup"));

					goto tmp_reader;
				}

				break;

			case reserve_media:
				size_available = soj_media_prepare(soj_backupdb_pool, backup_size, db_connect);

				if (size_available < backup_size)
					soj_job_add_record(job, db_connect, so_log_level_warning, so_job_record_notif_important,
						dgettext("storiqone-job-backup-db", "Not enough space available to complete backup"));

				state = get_media;
				break;
		}
	}

	tmp_reader->ops->close(tmp_reader);
	tmp_reader->ops->free(tmp_reader);

	db_connect->ops->backup_add(db_connect, soj_backupdb_backup);

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

tmp_reader:
	if (tmp_reader != NULL) {
		tmp_reader->ops->close(tmp_reader);
		tmp_reader = NULL;
	}

	return 1;
}

static int soj_backupdb_simulate(struct so_job * job, struct so_database_connection * db_connect) {
	soj_backupdb_pool = db_connect->ops->get_pool(db_connect, "d9f976d4-e087-4d0a-ab79-96267f6613f0", NULL);
	if (soj_backupdb_pool == NULL) {
		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-backup-db", "Pool (Storiq one database backup) not found"));
		return 1;
	}
	if (soj_backupdb_pool->deleted) {
		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-backup-db", "Trying to back up database to a deleted pool"));
		return 1;
	}

	soj_backupdb_medias = db_connect->ops->get_medias_of_pool(db_connect, soj_backupdb_pool);
	if (!soj_backupdb_pool->growable && so_value_list_get_length(soj_backupdb_medias) == 0) {
		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-backup-db", "Trying to back up database to a pool which is not growable and without any media"));
		return 1;
	}

	soj_backupdb_checksums = db_connect->ops->get_checksums_from_pool(db_connect, soj_backupdb_pool);

	soj_backupdb_backup = soj_backup_new(job);

	return 0;
}

static void soj_backupdb_script_on_error(struct so_job * job, struct so_database_connection * db_connect) {
	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_on_error, soj_backupdb_pool) < 1)
		return;

	struct so_value * json = so_value_pack("{sOsososo}",
		"checksums", soj_backupdb_checksums,
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"pool", so_pool_convert(soj_backupdb_pool)
	);

	struct so_value * returned = soj_script_run(db_connect, job, so_script_type_on_error, soj_backupdb_pool, json);
	so_value_free(json);
	so_value_free(returned);
}

static void soj_backupdb_script_post_run(struct so_job * job, struct so_database_connection * db_connect) {
	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_on_error, soj_backupdb_pool) < 1)
		return;

	struct so_value * json = so_value_pack("{sosOsososo}",
		"backup", soj_backup_convert(soj_backupdb_backup),
		"checksums", soj_backupdb_checksums,
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"pool", so_pool_convert(soj_backupdb_pool)
	);

	struct so_value * returned = soj_script_run(db_connect, job, so_script_type_post_job, soj_backupdb_pool, json);
	so_value_free(json);
	so_value_free(returned);
}

static bool soj_backupdb_script_pre_run(struct so_job * job, struct so_database_connection * db_connect) {
	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_pre_job, soj_backupdb_pool) < 1)
		return true;

	struct so_value * json = so_value_pack("{sOsososo}",
		"checksums", soj_backupdb_checksums,
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"pool", so_pool_convert(soj_backupdb_pool)
	);

	struct so_value * returned = soj_script_run(db_connect, job, so_script_type_pre_job, soj_backupdb_pool, json);
	so_value_free(json);

	bool should_run = false;
	so_value_unpack(returned, "{sb}", "should run", &should_run);
	so_value_free(returned);

	return should_run;
}

