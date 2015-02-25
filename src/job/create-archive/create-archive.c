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
	.name = "backup-db",

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
		src_files[i]->ops->free(src_files[i]);
	free(src_files);
	if (primary_pool != NULL)
		so_pool_free(primary_pool);
}

static void soj_create_archive_init() {
	soj_job_register(&create_archive);

	bindtextdomain("storiqone-job-create-archive", LOCALE_DIR);
}

static int soj_create_archive_run(struct so_job * job, struct so_database_connection * db_connect) {
	soj_create_archive_worker_init(primary_pool, pool_mirrors);
	soj_create_archive_worker_reserve_medias(archive_size, db_connect);
	soj_create_archive_worker_prepare_medias(db_connect);

	unsigned int i;
	int failed = 0;
	for (i = 0; i < nb_src_files; i++) {
		struct so_format_file file;
		enum so_format_reader_header_status status;
		while (status = src_files[i]->ops->get_header(src_files[i], &file), status == so_format_reader_header_ok) {
			soj_create_archive_meta_worker_add_file(file.filename);

			enum so_format_writer_status write_status = soj_create_archive_worker_add_file(&file);
			if (write_status != so_format_writer_ok)
				break;

			if (S_ISREG(file.mode)) {
				static char buffer[16384];
				ssize_t nb_read;
				while (nb_read = src_files[i]->ops->read(src_files[i], buffer, 16384), nb_read > 0) {
					ssize_t nb_write = soj_create_archive_worker_write(&file, buffer, nb_read);
					if (nb_write < 0) {
						failed = -2;
						break;
					}

					float done = 0.96 * soj_create_archive_progress();
					job->done = 0.02 + done;
				}

				if (nb_read < 0) {
					failed = -1;
				} else {
					failed = soj_create_archive_worker_end_of_file();
					// if (failed != 0)
				}
			}

			so_format_file_free(&file);
		}
	}

	soj_create_archive_worker_close();

	return failed;
}

static int soj_create_archive_simulate(struct so_job * job, struct so_database_connection * db_connect) {
	/**
	 * reserved for future (see ltfs)
	 * media_source = db_connect->ops->get_media(db_connect, NULL, NULL, job);
	**/

	selected_path = db_connect->ops->get_selected_files_by_job(db_connect, job);
	if (selected_path == NULL || so_value_list_get_length(selected_path) == 0) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important, dgettext("storiqone-job-create-archive", "There is no select files to create new archive or to add into an archive"));
		return 1;
	}

	nb_src_files = so_value_list_get_length(selected_path);
	src_files = calloc(nb_src_files, sizeof(struct so_format_reader));
	struct so_value_iterator * iter = so_value_list_get_iterator(selected_path);
	unsigned int i;
	for (i = 0; so_value_iterator_has_next(iter); i++) {
		struct so_value * elt = so_value_iterator_get_value(iter, false);
		const char * path = so_value_string_get(elt);

		struct so_format_reader * reader = soj_io_filesystem_reader(path);
		ssize_t sub_total = soj_format_compute_tar_size(reader);
		archive_size += sub_total;
		reader->ops->free(reader);

		src_files[i] = soj_io_filesystem_reader(path);
	}
	so_value_iterator_free(iter);


	primary_pool = db_connect->ops->get_pool(db_connect, NULL, job);
	// TODO: primary_pool IS NULL -> adding file to archive
	if (primary_pool->deleted) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important, dgettext("storiqone-job-create-archive", "Try to create an archive into a pool which is deleted"));
		return 1;
	}

	ssize_t reserved = soj_media_prepare(primary_pool, archive_size, db_connect);
	if (reserved < archive_size) {
		reserved += soj_media_prepare_unformatted(primary_pool, true, db_connect);

		if (reserved < archive_size)
			reserved += soj_media_prepare_unformatted(primary_pool, false, db_connect);
	}

	if (reserved < archive_size)
		return 1;


	pool_mirrors = db_connect->ops->get_pool_by_pool_mirror(db_connect, primary_pool);

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

	struct so_value * json = so_value_pack("{sosos{soso}sO}",
		"job", so_job_convert(job),
		"host", so_host_get_info2(),
		"pool",
			"primary", so_pool_convert(primary_pool),
			"mirrors", vpool_mirrors,
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

