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

// calloc, free, malloc
#include <stdlib.h>
// strdup
#include <string.h>
// bzero
#include <strings.h>
// ssize_t
#include <sys/types.h>
// time
#include <time.h>

#include <libstoriqone/archive.h>
#include <libstoriqone/database.h>
#include <libstoriqone/format.h>
#include <libstoriqone/value.h>
#include <libstoriqone-job/drive.h>
#include <libstoriqone-job/job.h>
#include <libstoriqone-job/media.h>

#include "meta_worker.h"
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

	struct soj_create_archive_files {
		char * path;
		ssize_t position;
		time_t archived_time;
		struct soj_create_archive_files * next;
	} * first_files, * last_files;
	unsigned int nb_files;
	ssize_t position;

	enum {
		soj_worker_status_not_ready,
		soj_worker_status_ready,
		soj_worker_status_finished
	} state;
};

static enum so_format_writer_status soj_create_archive_worker_add_file2(struct soj_create_archive_worker * worker, struct so_format_file * file);
static int soj_create_archive_worker_change_volume(struct soj_create_archive_worker * worker);
static int soj_create_archive_worker_close2(struct soj_create_archive_worker * worker);
static void soj_create_archive_worker_exit(void) __attribute__((destructor));
static void soj_create_archive_worker_free(struct soj_create_archive_worker * worker);
static struct soj_create_archive_worker * soj_create_archive_worker_new(struct so_pool * pool);
ssize_t soj_create_archive_worker_write2(struct soj_create_archive_worker * worker, struct so_format_file * file, const char * buffer, ssize_t length);

static struct soj_create_archive_worker * primary_worker = NULL;
static unsigned int nb_mirror_workers = 0;
static struct soj_create_archive_worker ** mirror_workers = NULL;


enum so_format_writer_status soj_create_archive_worker_add_file(struct so_format_file * file) {
	enum so_format_writer_status status = soj_create_archive_worker_add_file2(primary_worker, file);
	if (status != so_format_writer_ok)
		return status;

	unsigned int i;
	for (i = 0; i < nb_mirror_workers; i++) {
		struct soj_create_archive_worker * worker = mirror_workers[i];

		if (worker->state != soj_worker_status_ready)
			continue;

		status = soj_create_archive_worker_add_file2(worker, file);
		if (status != so_format_writer_ok)
			return status;
	}

	return status;
}

static enum so_format_writer_status soj_create_archive_worker_add_file2(struct soj_create_archive_worker * worker, struct so_format_file * file) {
	ssize_t position = worker->writer->ops->position(worker->writer) / worker->writer->ops->get_block_size(worker->writer);

	if (worker->pool->unbreakable_level == so_pool_unbreakable_level_file) {
		ssize_t available_size = worker->writer->ops->get_available_size(worker->writer);
		ssize_t file_size = worker->writer->ops->compute_size_of_file(worker->writer, file->filename, false);

		if (available_size < file_size && soj_create_archive_worker_change_volume(worker) != 0)
			return so_format_writer_error;

		position = 0;
	}

	int failed = 0;
	enum so_format_writer_status status = worker->writer->ops->add_file(worker->writer, file);
	switch (status) {
		case so_format_writer_end_of_volume:
			failed = soj_create_archive_worker_change_volume(worker);
			if (failed != 0)
				return so_format_writer_error;
			position = 0;
			break;

		case so_format_writer_error:
		case so_format_writer_unsupported:
			return so_format_writer_error;

		case so_format_writer_ok:
			break;
	}

	struct soj_create_archive_files * new_file = malloc(sizeof(struct soj_create_archive_files));
	new_file->path = strdup(file->filename);
	new_file->position = position;
	new_file->archived_time = time(NULL);
	new_file->next = NULL;

	if (worker->nb_files > 0)
		worker->last_files = worker->last_files->next = new_file;
	else
		worker->first_files = worker->last_files = new_file;
	worker->nb_files++;
	worker->position = 0;

	return status;
}

static int soj_create_archive_worker_change_volume(struct soj_create_archive_worker * worker) {
	worker->writer->ops->close(worker->writer);

	struct so_archive_volume * last_vol = worker->archive->volumes + (worker->archive->nb_volumes - 1);
	last_vol->end_time = time(NULL);
	last_vol->size = worker->writer->ops->position(worker->writer);

	last_vol->digests = worker->writer->ops->get_digests(worker->writer);
	worker->writer->ops->free(worker->writer);

	soj_create_archive_meta_worker_wait(false);

	unsigned int i;
	struct soj_create_archive_files * ptr_file = worker->first_files;
	struct so_value * files = soj_create_archive_meta_worker_get_files();
	last_vol->files = calloc(worker->nb_files, sizeof(struct so_archive_files));
	last_vol->nb_files = worker->nb_files;
	for (i = 0; i < last_vol->nb_files; i++) {
		struct so_value * vfile = so_value_hashtable_get2(files, ptr_file->path, false, false);

		struct so_archive_files * new_file = last_vol->files + i;
		new_file->file = so_value_custom_get(vfile);
		new_file->position = ptr_file->position;
		new_file->archived_time = ptr_file->archived_time;

		struct soj_create_archive_files * next = ptr_file->next;
		free(ptr_file->path);
		free(ptr_file);
		ptr_file = next;
	}

	worker->first_files = worker->last_files = NULL;
	worker->nb_files = 0;

	if (so_value_iterator_has_next(worker->media_iterator)) {
		struct so_value * vmedia = so_value_iterator_get_value(primary_worker->media_iterator, false);
		primary_worker->media = so_value_custom_get(vmedia);
		primary_worker->drive = soj_media_load(primary_worker->media, false);

		if (primary_worker->drive != NULL) {
			primary_worker->writer = primary_worker->drive->ops->get_writer(primary_worker->drive, primary_worker->checksums);

			struct so_archive_volume * vol = so_archive_add_volume(primary_worker->archive);
			vol->media = primary_worker->media;
			vol->media_position = primary_worker->writer->ops->file_position(primary_worker->writer);
			vol->job = soj_job_get();
		}
	}

	return 0;
}

int soj_create_archive_worker_close() {
	soj_create_archive_meta_worker_wait(true);
	int failed = soj_create_archive_worker_close2(primary_worker);
	if (failed != 0)
		return failed;

	unsigned int i;
	for (i = 0; i < nb_mirror_workers; i++) {
		struct soj_create_archive_worker * worker = mirror_workers[i];

		if (worker->state != soj_worker_status_ready)
			continue;

		failed = soj_create_archive_worker_close2(worker);
		if (failed != 0)
			return failed;
	}

	return 0;
}

static int soj_create_archive_worker_close2(struct soj_create_archive_worker * worker) {
	worker->writer->ops->close(worker->writer);

	struct so_archive_volume * last_vol = worker->archive->volumes + (worker->archive->nb_volumes - 1);
	last_vol->end_time = time(NULL);
	last_vol->size = worker->writer->ops->position(worker->writer);

	last_vol->digests = worker->writer->ops->get_digests(worker->writer);
	worker->writer->ops->free(worker->writer);

	unsigned int i;
	struct soj_create_archive_files * ptr_file = worker->first_files;
	struct so_value * files = soj_create_archive_meta_worker_get_files();
	last_vol->files = calloc(worker->nb_files, sizeof(struct so_archive_files));
	last_vol->nb_files = worker->nb_files;
	for (i = 0; i < last_vol->nb_files; i++) {
		struct so_value * vfile = so_value_hashtable_get2(files, ptr_file->path, false, false);

		struct so_archive_files * new_file = last_vol->files + i;
		new_file->file = so_value_custom_get(vfile);
		new_file->position = ptr_file->position;
		new_file->archived_time = ptr_file->archived_time;

		struct soj_create_archive_files * next = ptr_file->next;
		free(ptr_file->path);
		free(ptr_file);
		ptr_file = next;
	}

	worker->first_files = worker->last_files = NULL;
	worker->nb_files = 0;

	return 0;
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

	worker->first_files = worker->last_files = NULL;
	worker->nb_files = 0;
	worker->position = 0;

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
		vol->media_position = primary_worker->writer->ops->file_position(primary_worker->writer);
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

float soj_create_archive_progress() {
	ssize_t nb_done = primary_worker->total_done;
	ssize_t nb_total = primary_worker->archive_size;

	unsigned int i;
	for (i = 0; i < nb_mirror_workers; i++) {
		struct soj_create_archive_worker * worker = mirror_workers[i];

		if (worker->state != soj_worker_status_ready)
			continue;

		nb_done += worker->total_done;
		nb_total += worker->archive_size;
	}

	return ((float) nb_done) / nb_total;
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

ssize_t soj_create_archive_worker_write(struct so_format_file * file, const char * buffer, ssize_t length) {
	ssize_t nb_write = soj_create_archive_worker_write2(primary_worker, file, buffer, length);
	if (nb_write < 0)
		return nb_write;

	unsigned int i;
	for (i = 0; i < nb_mirror_workers; i++) {
		struct soj_create_archive_worker * worker = mirror_workers[i];

		if (worker->state != soj_worker_status_ready)
			continue;

		nb_write = soj_create_archive_worker_write2(worker, file, buffer, length);
		if (nb_write < 0)
			return nb_write;
	}

	return nb_write;
}

ssize_t soj_create_archive_worker_write2(struct soj_create_archive_worker * worker, struct so_format_file * file, const char * buffer, ssize_t length) {
	ssize_t available = worker->writer->ops->get_available_size(worker->writer);
	ssize_t nb_total_write = 0;
	while (nb_total_write < length) {
		if (available <= 0) {
			int failed = soj_create_archive_worker_change_volume(worker);
			if (failed)
				return -1;

			worker->writer->ops->restart_file(worker->writer, file, worker->position);

			available = worker->writer->ops->get_available_size(worker->writer);
		}

		ssize_t nb_write = worker->writer->ops->write(worker->writer, buffer + nb_total_write, length - nb_total_write);
		if (nb_write < 0)
			return nb_write;

		nb_total_write += nb_write;
		available -= nb_write;
	}

	return nb_total_write;
}
