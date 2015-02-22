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

// free, malloc
#include <stdlib.h>
// bzero
#include <strings.h>
// ssize_t
#include <sys/types.h>

#include <libstoriqone/archive.h>
#include <libstoriqone/database.h>
#include <libstoriqone/format.h>
#include <libstoriqone/value.h>
#include <libstoriqone-job/drive.h>
#include <libstoriqone-job/job.h>
#include <libstoriqone-job/media.h>

#include "worker.h"

struct soj_create_archive_worker {
	struct so_archive * archive;
	struct so_pool * pool;
	struct so_value * checksums;

	ssize_t total_done;
	ssize_t archive_size;

	struct so_value_iterator * media_iterator;
	struct so_media * media;
	struct so_drive * drive;
	struct so_format_writer * writer;

	enum {
		soj_worker_status_not_ready,
		soj_worker_status_ready,
		soj_worker_status_finished
	} state;
};

static enum so_format_writer_status soj_create_archive_worker_add_file2(struct soj_create_archive_worker * worker, struct so_format_file * file);
static int soj_create_archive_worker_change_volume(struct soj_create_archive_worker * worker);
static void soj_create_archive_worker_exit(void) __attribute__((destructor));
static void soj_create_archive_worker_free(struct soj_create_archive_worker * worker);
static struct soj_create_archive_worker * soj_create_archive_worker_new(struct so_pool * pool);

static struct soj_create_archive_worker * primary_worker = NULL;
static unsigned int nb_mirror_workers = 0;
static struct soj_create_archive_worker ** mirror_workers = NULL;


enum so_format_writer_status soj_create_archive_worker_add_file(struct so_format_file * file) {
	enum so_format_writer_status status = soj_create_archive_worker_add_file2(primary_worker, file);

	unsigned int i;
	for (i = 0; i < nb_mirror_workers; i++) {
		struct soj_create_archive_worker * worker = mirror_workers[i];

		if (worker->state != soj_worker_status_ready)
			continue;

		status = soj_create_archive_worker_add_file2(worker, file);
	}
}

static enum so_format_writer_status soj_create_archive_worker_add_file2(struct soj_create_archive_worker * worker, struct so_format_file * file) {
	ssize_t position = worker->writer->ops->position(worker->writer) / worker->writer->ops->get_block_size(worker->writer);

	if (worker->pool->unbreakable_level == so_pool_unbreakable_level_file) {
		ssize_t available_size = worker->writer->ops->get_available_size(worker->writer);
		ssize_t file_size = worker->writer->ops->compute_size_of_file(worker->writer, file->filename, false);

		// if (available_size < file_size && st_job_create_archive_single_worker_change_volume(self))
		//	return 1;
	}

	enum so_format_writer_status status = worker->writer->ops->add_file(worker->writer, file);
}

static int soj_create_archive_worker_change_volume(struct soj_create_archive_worker * worker) {
	worker->writer->ops->close(worker->writer);
}

static void soj_create_archive_worker_exit() {}

static void soj_create_archive_worker_free(struct soj_create_archive_worker * worker) {
	so_archive_free(worker->archive);
	free(worker);
}

void soj_create_archive_worker_init(struct so_pool * primary_pool, struct so_value * pool_mirrors) {
	primary_worker = soj_create_archive_worker_new(primary_pool); 

	unsigned int i;
	nb_mirror_workers = so_value_list_get_length(pool_mirrors);
	mirror_workers = calloc(nb_mirror_workers, sizeof(struct soj_create_archive_worker *));
	struct so_value_iterator * iter = so_value_list_get_iterator(pool_mirrors);
	for (i = 0; so_value_iterator_has_next(iter); i++) {
		struct so_value * vpool = so_value_iterator_get_value(iter, false);
		struct so_pool * pool = so_value_custom_get(vpool);
		mirror_workers[i] = soj_create_archive_worker_new(pool);
	}
	so_value_iterator_free(iter);
}

static struct soj_create_archive_worker * soj_create_archive_worker_new(struct so_pool * pool) {
	struct soj_create_archive_worker * worker = malloc(sizeof(struct soj_create_archive_worker));
	bzero(worker, sizeof(struct soj_create_archive_worker));

	worker->archive = so_archive_new();
	worker->pool = pool;

	worker->total_done = 0;
	worker->archive_size = 0;

	worker->media_iterator = NULL;
	worker->media = NULL;
	worker->drive = NULL;

	worker->state = soj_worker_status_not_ready;

	return worker;
}

void soj_create_archive_worker_prepare_medias(struct so_database_connection * db_connect) {
	primary_worker->media_iterator = soj_media_get_iterator(primary_worker->pool);
	struct so_value * vmedia = so_value_iterator_get_value(primary_worker->media_iterator, false);
	primary_worker->media = so_value_custom_get(vmedia);
	primary_worker->drive = soj_media_load(primary_worker->media, false);

	if (primary_worker->drive != NULL) {
		primary_worker->checksums = db_connect->ops->get_checksums_from_pool(db_connect, primary_worker->pool);
		primary_worker->writer = primary_worker->drive->ops->get_writer(primary_worker->drive, primary_worker->checksums);

		struct so_archive_volume * vol = so_archive_add_volume(primary_worker->archive);
		vol->media = primary_worker->media;
		// vol->media_position = primary_worker->writer->ops->
		vol->job = soj_job_get();
	}

	unsigned int i;
	bool need_tmp_archive = false;
	for (i = 0; i < nb_mirror_workers; i++) {
		struct soj_create_archive_worker * worker = mirror_workers[i];

		if (worker->state != soj_worker_status_not_ready)
			continue;

		worker->media_iterator = soj_media_get_iterator(worker->pool);
		vmedia = so_value_iterator_get_value(primary_worker->media_iterator, false);
		worker->drive = soj_media_load(worker->media, true);

		if (worker->drive == NULL)
			need_tmp_archive = true;

		worker->checksums = db_connect->ops->get_checksums_from_pool(db_connect, worker->pool);
		worker->writer = worker->drive->ops->get_writer(worker->drive, worker->checksums);
	}
}

void soj_create_archive_worker_reserve_medias(ssize_t archive_size, struct so_database_connection * db_connect) {
	primary_worker->archive_size = archive_size;

	unsigned int i;
	for (i = 0; i < nb_mirror_workers; i++) {
		struct soj_create_archive_worker * worker = mirror_workers[i];

		ssize_t reserved = soj_media_prepare(worker->pool, archive_size, db_connect);
		if (reserved < archive_size) {
			reserved += soj_media_prepare_unformatted(worker->pool, true, db_connect);

			if (reserved < archive_size)
				reserved += soj_media_prepare_unformatted(worker->pool, false, db_connect);
		}

		if (reserved > archive_size) {
			worker->state = soj_worker_status_ready;
			worker->archive_size = archive_size;
		} else {
			worker->state = soj_worker_status_not_ready;
			soj_media_release_all_medias(worker->pool);
		}
	}
}

