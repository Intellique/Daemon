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
// calloc
#include <stdlib.h>
// S_*
#include <sys/stat.h>
// access
#include <unistd.h>

#include <libstoriqone/archive.h>
#include <libstoriqone/database.h>
#include <libstoriqone/file.h>
#include <libstoriqone/host.h>
#include <libstoriqone/log.h>
#include <libstoriqone/media.h>
#include <libstoriqone/value.h>
#include <libstoriqone-job/format.h>
#include <libstoriqone-job/io.h>
#include <libstoriqone-job/job.h>
#include <libstoriqone-job/media.h>
#include <libstoriqone-job/script.h>

#include "meta_worker.h"
#include "worker.h"

#include <job_create-archive.chcksum>

#include "config.h"


static struct so_media * media_source = NULL;
static struct so_value * selected_path = NULL;
static struct so_format_reader ** src_files = NULL;
static unsigned int nb_src_files = 0;

static struct so_archive * primary_archive = NULL;
static struct so_value * archives_mirrors = NULL;
static struct so_pool * primary_pool = NULL;
static struct so_value * pool_mirrors = NULL;

static ssize_t archive_size = 0;

static void soj_create_archive_exit(struct so_job * job, struct so_database_connection * db_connect);
static void soj_create_archive_init(void) __attribute__((constructor));
static int soj_create_archive_run(struct so_job * job, struct so_database_connection * db_connect);
static int soj_create_archive_simulate(struct so_job * job, struct so_database_connection * db_connect);
static void soj_create_archive_script_on_error(struct so_job * job, struct so_database_connection * db_connect);
static void soj_create_archive_script_post_run(struct so_job * job, struct so_database_connection * db_connect);
static bool soj_create_archive_script_pre_run(struct so_job * job, struct so_database_connection * db_connect);

static struct so_job_driver create_archive = {
	.name = "create-archive",

	.exit            = soj_create_archive_exit,
	.run             = soj_create_archive_run,
	.simulate        = soj_create_archive_simulate,
	.script_on_error = soj_create_archive_script_on_error,
	.script_post_run = soj_create_archive_script_post_run,
	.script_pre_run  = soj_create_archive_script_pre_run,
};


static void soj_create_archive_exit(struct so_job * job __attribute__((unused)), struct so_database_connection * db_connect __attribute__((unused))) {
	if (media_source != NULL)
		so_media_free(media_source);
	so_value_free(selected_path);
	unsigned int i;
	for (i = 0; i < nb_src_files; i++)
		if (src_files[i] != NULL)
			src_files[i]->ops->free(src_files[i]);
	free(src_files);

	if (primary_archive != NULL)
		so_archive_free(primary_archive);

	if (archives_mirrors != NULL)
		so_value_free(archives_mirrors);

	if (primary_pool != NULL)
		so_pool_free(primary_pool);

	if (pool_mirrors != NULL)
		so_value_free(pool_mirrors);
}

static void soj_create_archive_init() {
	soj_job_register(&create_archive);

	bindtextdomain("storiqone-job-create-archive", LOCALE_DIR);
}

static int soj_create_archive_run(struct so_job * job, struct so_database_connection * db_connect) {
	if (primary_archive != NULL)
		soj_create_archive_worker_init_archive(job, primary_archive, archives_mirrors);
	else
		soj_create_archive_worker_init_pool(job, primary_pool, pool_mirrors);
	soj_create_archive_worker_reserve_medias(job, archive_size, db_connect);
	soj_create_archive_worker_prepare_medias(db_connect);

	bool stop = false;
	unsigned int i, round;
	int failed = 0;
	for (round = 1; !stop; round++) {
		for (i = 0; i < nb_src_files; i++) {
			char * root = src_files[i]->ops->get_root(src_files[i]);

			struct so_format_file file;
			enum so_format_reader_header_status status;
			while (status = src_files[i]->ops->get_header(src_files[i], &file), status == so_format_reader_header_ok) {
				so_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_normal,
						dgettext("storiqone-job-create-archive", "Adding %s to archive"),
						file.filename);

				if (round == 1)
					soj_create_archive_meta_worker_add_file(file.filename, root);

				enum so_format_writer_status write_status = soj_create_archive_worker_add_file(job, &file, round == 1, db_connect);
				if (write_status != so_format_writer_ok) {
					so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
							dgettext("storiqone-job-create-archive", "Error while adding %s to archive"),
							file.filename);
					break;
				}

				if (S_ISREG(file.mode)) {
					static char buffer[16384];
					ssize_t nb_read;
					while (nb_read = src_files[i]->ops->read(src_files[i], buffer, 16384), nb_read > 0) {
						ssize_t nb_write = soj_create_archive_worker_write(job, &file, buffer, nb_read, round == 1, db_connect);
						if (nb_write < 0) {
							failed = -2;
							break;
						}

						float done = 0.96 * soj_create_archive_progress();
						job->done = 0.02 + done;
					}

					if (nb_read < 0) {
						so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
								dgettext("storiqone-job-create-archive", "Error while reading from %s"),
								file.filename);
						failed = -1;
					} else {
						failed = soj_create_archive_worker_end_of_file();
						// if (failed != 0)
					}
				}

				so_format_file_free(&file);
			}

			free(root);
		}

		so_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_normal,
			dgettext("storiqone-job-create-archive", "Closing archive"));
		soj_create_archive_worker_close(round == 1);

		so_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_normal,
			dgettext("storiqone-job-create-archive", "Synchronizing archive with database"));
		soj_create_archive_worker_sync_archives(job, db_connect);

		stop = soj_create_archive_worker_finished();
		if (!stop) {
			soj_create_archive_worker_prepare_medias2(db_connect);

			for (i = 0; i < nb_src_files; i++)
				src_files[i]->ops->rewind(src_files[i]);
		}
	}

	if (failed != 0)
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-create-archive", "Error while synchronize archive into database"));

	return failed;
}

static int soj_create_archive_simulate(struct so_job * job, struct so_database_connection * db_connect) {
	/**
	 * reserved for future (see ltfs)
	 * media_source = db_connect->ops->get_media(db_connect, NULL, NULL, job);
	**/

	selected_path = db_connect->ops->get_selected_files_by_job(db_connect, job);
	if (selected_path == NULL || so_value_list_get_length(selected_path) == 0) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-create-archive", "There is no select files to create new archive or to add into an archive"));
		return 1;
	}

	nb_src_files = so_value_list_get_length(selected_path);
	src_files = calloc(nb_src_files, sizeof(struct so_format_reader));
	struct so_value_iterator * iter = so_value_list_get_iterator(selected_path);
	unsigned int i;
	for (i = 0; so_value_iterator_has_next(iter); i++) {
		struct so_value * elt = so_value_iterator_get_value(iter, false);
		const char * path = so_value_string_get(elt);

		if (access(path, F_OK) != 0) {
			so_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_normal,
				dgettext("storiqone-job-create-archive", "Error, path '%s' do not exist"),
				path);
			so_value_iterator_free(iter);
			return 1;
		}

		src_files[i] = soj_io_filesystem_reader(path);
		ssize_t sub_total = soj_format_compute_tar_size(src_files[i]);
		archive_size += sub_total;
		src_files[i]->ops->rewind(src_files[i]);
	}
	so_value_iterator_free(iter);

	char sarchive_size[12];
	so_file_convert_size_to_string(archive_size, sarchive_size, 12);


	primary_archive = db_connect->ops->get_archive_by_job(db_connect, job);
	primary_pool = db_connect->ops->get_pool(db_connect, NULL, job);

	if (primary_archive == NULL && primary_pool == NULL) {
		so_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_normal,
			dgettext("storiqone-job-create-archive", "Error, Can't create new archive or add into an archive"));
		return 1;
	}

	if (primary_archive != NULL && primary_pool != NULL) {
		// TODO: job with an archive and a pool
	}

	if (primary_archive != NULL) {
		if (!primary_archive->can_append) {
			so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
				dgettext("storiqone-job-create-archive", "Error, can't add file into an archive '%s' which forbid to add them"),
				primary_archive->name);
			return 1;
		}

		if (primary_archive->nb_volumes < 1) {
			so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
				dgettext("storiqone-job-create-archive", "Bug: archive '%s' do not contain files"),
				primary_archive->name);
			return 1;
		}

		struct so_pool * pool = primary_archive->volumes->media->pool;
		if (pool->deleted) {
			so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
				dgettext("storiqone-job-create-archive", "Try to create an archive '%s' into a pool '%s' which is deleted"),
				primary_archive->name, pool->name);
			return 1;
		}

		if (!db_connect->ops->is_archive_synchronized(db_connect, primary_archive)) {
			so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
				dgettext("storiqone-job-create-archive", "Error, can't add file to the archive '%s' because this archive should be synchronized"),
				primary_archive->name);
			return 1;
		}

		ssize_t reserved = soj_media_prepare(pool, archive_size, db_connect);
		if (reserved < archive_size) {
			reserved += soj_media_prepare_unformatted(pool, true, db_connect);

			if (reserved < archive_size)
				reserved += soj_media_prepare_unformatted(pool, false, db_connect);
		}

		if (reserved < archive_size) {
			so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
				dgettext("storiqone-job-create-archive", "Error: not enought space available into pool (%s)"),
				pool->name);
			return 1;
		}

		archives_mirrors = so_value_new_linked_list();
		struct so_value * archives = db_connect->ops->get_archives_by_archive_mirror(db_connect, primary_archive);
		struct so_value_iterator * iter = so_value_list_get_iterator(archives);
		while (so_value_iterator_has_next(iter)) {
			struct so_value * varchive = so_value_iterator_get_value(iter, false);
			struct so_archive * archive = so_value_custom_get(varchive);

			if (db_connect->ops->is_archive_synchronized(db_connect, archive))
				so_value_list_push(archives_mirrors, varchive, false);
			else
				so_job_add_record(job, db_connect, so_log_level_warning, so_job_record_notif_important,
					dgettext("storiqone-job-create-archive", "Warning, ignoring archive '%s' from pool '%s' because this archive is not synchronized"),
					archive->name, archive->volumes->media->pool->name);
		}
		so_value_iterator_free(iter);

		so_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_normal,
			dgettext("storiqone-job-create-archive", "Size require (%s) for creating new archive '%s'"),
			sarchive_size, job->name);
	} else {
		if (primary_pool->deleted) {
			so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
				dgettext("storiqone-job-create-archive", "Try to create an archive into a pool '%s' which is deleted"),
				primary_pool->name);
			return 1;
		}

		ssize_t reserved = soj_media_prepare(primary_pool, archive_size, db_connect);
		if (reserved < archive_size) {
			reserved += soj_media_prepare_unformatted(primary_pool, true, db_connect);

			if (reserved < archive_size)
				reserved += soj_media_prepare_unformatted(primary_pool, false, db_connect);
		}

		if (reserved < archive_size) {
			so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
				dgettext("storiqone-job-create-archive", "Error: not enought space available into pool (%s)"),
				primary_pool->name);
			return 1;
		}

		pool_mirrors = db_connect->ops->get_pool_by_pool_mirror(db_connect, primary_pool);

		so_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_normal,
			dgettext("storiqone-job-create-archive", "Size require (%s) for adding to the archive '%s'"),
			sarchive_size, job->name);
	}

	return 0;
}

static void soj_create_archive_script_on_error(struct so_job * job, struct so_database_connection * db_connect) {
	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_on_error, primary_pool) < 1)
		return;

	struct so_value * vpool_mirrors = so_value_new_linked_list();
	struct so_value_iterator * iter = so_value_list_get_iterator(pool_mirrors);
	while (so_value_iterator_has_next(iter)) {
		struct so_value * vpool = so_value_iterator_get_value(iter, false);
		struct so_pool * pool = so_value_custom_get(vpool);

		so_value_list_push(vpool_mirrors, so_pool_convert(pool), true);
	}
	so_value_iterator_free(iter);

	struct so_value * json = so_value_pack("{sosos{soso}sO}",
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"pool",
			"primary", so_pool_convert(primary_pool),
			"mirrors", vpool_mirrors,
		"selected paths", selected_path
	);

	struct so_value * returned = soj_script_run(db_connect, job, so_script_type_on_error, primary_pool, json);
	so_value_free(returned);
	so_value_free(json);
}

static void soj_create_archive_script_post_run(struct so_job * job, struct so_database_connection * db_connect) {
	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_post_job, primary_pool) < 1)
		return;

	struct so_value * vpool_mirrors = so_value_new_linked_list();
	struct so_value_iterator * iter = so_value_list_get_iterator(pool_mirrors);
	while (so_value_iterator_has_next(iter)) {
		struct so_value * vpool = so_value_iterator_get_value(iter, false);
		struct so_pool * pool = so_value_custom_get(vpool);

		so_value_list_push(vpool_mirrors, so_pool_convert(pool), true);
	}
	so_value_iterator_free(iter);

	struct so_value * json = so_value_pack("{sosos{soso}sosO}",
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"pool",
			"primary", so_pool_convert(primary_pool),
			"mirrors", vpool_mirrors,
		"archive", soj_create_archive_worker_archives(),
		"selected paths", selected_path
	);

	struct so_value * returned = soj_script_run(db_connect, job, so_script_type_post_job, primary_pool, json);
	so_value_free(returned);
	so_value_free(json);
}

static bool soj_create_archive_script_pre_run(struct so_job * job, struct so_database_connection * db_connect) {
	if (db_connect->ops->get_nb_scripts(db_connect, job->type, so_script_type_pre_job, primary_pool) < 1)
		return true;

	struct so_value * vpool_mirrors = so_value_new_linked_list();
	struct so_value_iterator * iter = so_value_list_get_iterator(pool_mirrors);
	while (so_value_iterator_has_next(iter)) {
		struct so_value * vpool = so_value_iterator_get_value(iter, false);
		struct so_pool * pool = so_value_custom_get(vpool);

		so_value_list_push(vpool_mirrors, so_pool_convert(pool), true);
	}
	so_value_iterator_free(iter);

	struct so_value * json = so_value_pack("{sosos{soso}sO}",
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"pool",
			"primary", so_pool_convert(primary_pool),
			"mirrors", vpool_mirrors,
		"selected paths", selected_path
	);

	struct so_value * returned = soj_script_run(db_connect, job, so_script_type_pre_job, primary_pool, json);
	so_value_free(json);

	bool should_run = false;
	so_value_unpack(returned, "{sb}", "should run", &should_run);
	so_value_free(returned);

	return should_run;
}

