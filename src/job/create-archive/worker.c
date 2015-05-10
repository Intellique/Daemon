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

// dgettext
#include <libintl.h>
// calloc, free, malloc
#include <stdlib.h>
// strdup, strlen
#include <string.h>
// bzero
#include <strings.h>
// ssize_t
#include <sys/types.h>
// time
#include <time.h>
// uuid
#include <uuid/uuid.h>

#include <libstoriqone/archive.h>
#include <libstoriqone/database.h>
#include <libstoriqone/file.h>
#include <libstoriqone/format.h>
#include <libstoriqone/io.h>
#include <libstoriqone/json.h>
#include <libstoriqone/log.h>
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
		soj_worker_status_give_up,
		soj_worker_status_reserved,
		soj_worker_status_ready,
		soj_worker_status_error,
		soj_worker_status_finished
	} state;
};

static enum so_format_writer_status soj_create_archive_worker_add_file2(struct so_job * job, struct soj_create_archive_worker * worker, struct so_format_file * file, struct so_database_connection * db_connect);
static int soj_create_archive_worker_change_volume(struct so_job * job, struct soj_create_archive_worker * worker, struct so_database_connection * db_connect);
static int soj_create_archive_worker_close2(struct soj_create_archive_worker * worker);
static struct so_archive_file * soj_create_archive_worker_copy_file(struct soj_create_archive_worker * worker, struct so_archive_file * file);
static void soj_create_archive_worker_free(struct soj_create_archive_worker * worker);
static struct soj_create_archive_worker * soj_create_archive_worker_new(struct so_job * job, struct so_archive * archive, struct so_pool * pool);
ssize_t soj_create_archive_worker_write2(struct so_job * job, struct soj_create_archive_worker * worker, struct so_format_file * file, const char * buffer, ssize_t length, struct so_database_connection * db_connect);
static bool soj_create_archive_worker_write_meta(struct soj_create_archive_worker * worker);

static struct soj_create_archive_worker * primary_worker = NULL;
static unsigned int nb_mirror_workers = 0;
static struct soj_create_archive_worker ** mirror_workers = NULL;


struct so_value * soj_create_archive_worker_archives() {
	struct so_value * archive_mirrors = so_value_new_linked_list();

	unsigned int i;
	for (i = 0; i < nb_mirror_workers; i++) {
		struct soj_create_archive_worker * worker = mirror_workers[i];

		if (worker->state != soj_worker_status_ready)
			continue;

		so_value_list_push(archive_mirrors, so_archive_convert(worker->archive), true);
	}

	return so_value_pack("{soso}",
		"main", so_archive_convert(primary_worker->archive),
		"mirrors", archive_mirrors
	);
}

enum so_format_writer_status soj_create_archive_worker_add_file(struct so_job * job, struct so_format_file * file, struct so_database_connection * db_connect) {
	if (primary_worker->state == soj_worker_status_ready) {
		enum so_format_writer_status status = soj_create_archive_worker_add_file2(job, primary_worker, file, db_connect);
		if (status != so_format_writer_ok) {
			so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
				dgettext("storiqone-job-create-archive", "Error while adding file (%s) to pool %s"),
				file->filename, primary_worker->pool->name);
			primary_worker->state = soj_worker_status_error;
			return status;
		}
	}

	unsigned int i;
	for (i = 0; i < nb_mirror_workers; i++) {
		struct soj_create_archive_worker * worker = mirror_workers[i];

		if (worker->state != soj_worker_status_ready)
			continue;

		enum so_format_writer_status status = soj_create_archive_worker_add_file2(job, worker, file, db_connect);
		if (status != so_format_writer_ok) {
			so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
				dgettext("storiqone-job-create-archive", "Error while adding file (%s) to pool %s"),
				file->filename, worker->pool->name);
			worker->state = soj_worker_status_error;
		}
	}

	if (primary_worker->state == soj_worker_status_ready)
		return so_format_writer_ok;

	for (i = 0; i < nb_mirror_workers; i++)
		if (mirror_workers[i]->state == soj_worker_status_ready)
			return so_format_writer_ok;

	return so_format_writer_error;
}

static enum so_format_writer_status soj_create_archive_worker_add_file2(struct so_job * job, struct soj_create_archive_worker * worker, struct so_format_file * file, struct so_database_connection * db_connect) {
	ssize_t position = worker->writer->ops->position(worker->writer) / worker->writer->ops->get_block_size(worker->writer);

	if (worker->pool->unbreakable_level == so_pool_unbreakable_level_file) {
		ssize_t available_size = worker->writer->ops->get_available_size(worker->writer);
		ssize_t file_size = worker->writer->ops->compute_size_of_file(worker->writer, file);

		if (available_size < file_size && soj_create_archive_worker_change_volume(job, worker, db_connect) != 0)
			return so_format_writer_error;

		position = 0;
	}

	int failed = 0;
	enum so_format_writer_status status = worker->writer->ops->add_file(worker->writer, file);
	switch (status) {
		case so_format_writer_end_of_volume:
			failed = soj_create_archive_worker_change_volume(job, worker, db_connect);
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

static int soj_create_archive_worker_change_volume(struct so_job * job, struct soj_create_archive_worker * worker, struct so_database_connection * db_connect) {
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
		new_file->file = soj_create_archive_worker_copy_file(worker, so_value_custom_get(vfile));
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

		so_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_important, dgettext("storiqone-job-create-archive", "Archive continue to media (%s)"), primary_worker->media->name);

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

int soj_create_archive_worker_close(int round) {
	if (round == 1)
		soj_create_archive_meta_worker_wait(true);

	if (primary_worker->state == soj_worker_status_ready) {
		int failed = soj_create_archive_worker_close2(primary_worker);
		if (failed != 0)
			primary_worker->state = soj_worker_status_error;
		else
			soj_create_archive_worker_write_meta(primary_worker);
	}

	unsigned int i;
	for (i = 0; i < nb_mirror_workers; i++) {
		struct soj_create_archive_worker * worker = mirror_workers[i];

		if (worker->state != soj_worker_status_ready)
			continue;

		int failed = soj_create_archive_worker_close2(worker);
		if (failed != 0)
			worker->state = soj_worker_status_error;
		else
			soj_create_archive_worker_write_meta(worker);
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
		new_file->file = soj_create_archive_worker_copy_file(worker, so_value_custom_get(vfile));
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

static struct so_archive_file * soj_create_archive_worker_copy_file(struct soj_create_archive_worker * worker, struct so_archive_file * file) {
	struct so_archive_file * new_file = malloc(sizeof(struct so_archive_file));
	bzero(new_file, sizeof(struct so_archive_file));

	new_file->path = strdup(file->path);
	new_file->perm = file->perm;
	new_file->type = file->type;
	new_file->ownerid = file->ownerid;
	new_file->owner = strdup(file->owner);
	new_file->groupid = file->groupid;
	new_file->group = strdup(file->group);

	new_file->create_time = file->create_time;
	new_file->modify_time = file->modify_time;

	new_file->check_ok = false;
	new_file->check_time = 0;

	new_file->size = file->size;
	new_file->mime_type = strdup(file->mime_type);
	new_file->selected_path = strdup(file->selected_path);

	if (file->digests != NULL) {
		new_file->digests = so_value_new_hashtable2();

		struct so_value_iterator * iter = so_value_list_get_iterator(worker->checksums);
		while (so_value_iterator_has_next(iter)) {
			struct so_value * key = so_value_iterator_get_value(iter, false);

			if (!so_value_hashtable_has_key(file->digests, key))
				continue;

			struct so_value * val = so_value_hashtable_get(file->digests, key, false, false);
			so_value_hashtable_put(new_file->digests, key, false, val, false);
		}
		so_value_iterator_free(iter);
	} else
		new_file->digests = NULL;

	new_file->db_data = NULL;

	return new_file;
}

ssize_t soj_create_archive_worker_end_of_file() {
	ssize_t nb_write = primary_worker->writer->ops->end_of_file(primary_worker->writer);
	if (nb_write != 0)
		return nb_write;

	unsigned int i;
	for (i = 0; i < nb_mirror_workers; i++) {
		struct soj_create_archive_worker * worker = mirror_workers[i];

		if (worker->state != soj_worker_status_ready)
			continue;

		nb_write = worker->writer->ops->end_of_file(worker->writer);
		if (nb_write != 0)
			return nb_write;
	}

	return nb_write;
}

bool soj_create_archive_worker_finished() {
	if (primary_worker->state != soj_worker_status_error && primary_worker->state != soj_worker_status_finished)
		return false;

	unsigned int i;
	for (i = 0; i < nb_mirror_workers; i++) {
		struct soj_create_archive_worker * worker = mirror_workers[i];
		if (worker->state != soj_worker_status_error && worker->state != soj_worker_status_finished)
			return false;
	}

	return true;
}

static void soj_create_archive_worker_free(struct soj_create_archive_worker * worker) {
	so_archive_free(worker->archive);
	free(worker);
}

void soj_create_archive_worker_init_archive(struct so_job * job, struct so_archive * primary_archive, struct so_value * archive_mirrors) {
	struct so_pool * pool = primary_archive->volumes->media->pool;
	primary_worker = soj_create_archive_worker_new(job, primary_archive, pool);

	unsigned int i;
	nb_mirror_workers = so_value_list_get_length(archive_mirrors);
	mirror_workers = calloc(nb_mirror_workers, sizeof(struct soj_create_archive_worker *));
	struct so_value_iterator * iter = so_value_list_get_iterator(archive_mirrors);
	for (i = 0; so_value_iterator_has_next(iter); i++) {
		struct so_value * varchive = so_value_iterator_get_value(iter, false);
		struct so_archive * archive = so_value_custom_get(varchive);
		pool = archive->volumes->media->pool;
		mirror_workers[i] = soj_create_archive_worker_new(job, archive, pool);
	}
	so_value_iterator_free(iter);
}

void soj_create_archive_worker_init_pool(struct so_job * job, struct so_pool * primary_pool, struct so_value * pool_mirrors) {
	primary_worker = soj_create_archive_worker_new(job, NULL, primary_pool); 

	unsigned int i;
	nb_mirror_workers = so_value_list_get_length(pool_mirrors);
	mirror_workers = calloc(nb_mirror_workers, sizeof(struct soj_create_archive_worker *));
	struct so_value_iterator * iter = so_value_list_get_iterator(pool_mirrors);
	for (i = 0; so_value_iterator_has_next(iter); i++) {
		struct so_value * vpool = so_value_iterator_get_value(iter, false);
		struct so_pool * pool = so_value_custom_get(vpool);
		mirror_workers[i] = soj_create_archive_worker_new(job, NULL, pool);
	}
	so_value_iterator_free(iter);
}

static struct soj_create_archive_worker * soj_create_archive_worker_new(struct so_job * job, struct so_archive * archive, struct so_pool * pool) {
	struct soj_create_archive_worker * worker = malloc(sizeof(struct soj_create_archive_worker));
	bzero(worker, sizeof(struct soj_create_archive_worker));

	if (archive == NULL) {
		worker->archive = so_archive_new();
		uuid_t uuid;
		uuid_generate(uuid);
		uuid_unparse_lower(uuid, worker->archive->uuid);
		worker->archive->name = strdup(job->name);
		worker->archive->creator = strdup(job->user);
		worker->archive->owner = strdup(job->user);
	} else
		worker->archive = archive;

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
	struct so_value * checksums = so_value_new_hashtable2();

	primary_worker->media_iterator = soj_media_get_iterator(primary_worker->pool);
	struct so_value * vmedia = so_value_iterator_get_value(primary_worker->media_iterator, false);
	primary_worker->media = so_value_custom_get(vmedia);
	primary_worker->drive = soj_media_load(primary_worker->media, false);

	if (primary_worker->drive != NULL) {
		primary_worker->checksums = db_connect->ops->get_checksums_from_pool(db_connect, primary_worker->pool);
		primary_worker->writer = primary_worker->drive->ops->get_writer(primary_worker->drive, primary_worker->checksums);
		primary_worker->state = soj_worker_status_ready;

		struct so_archive_volume * vol = so_archive_add_volume(primary_worker->archive);
		vol->media = primary_worker->media;
		vol->media_position = primary_worker->writer->ops->file_position(primary_worker->writer);
		vol->job = soj_job_get();

		struct so_value_iterator * iter = so_value_list_get_iterator(primary_worker->checksums);
		while (so_value_iterator_has_next(iter)) {
			struct so_value * val = so_value_iterator_get_value(iter, false);
			if (!so_value_hashtable_has_key(checksums, val))
				so_value_hashtable_put(checksums, val, false, val, false);
		}
		so_value_iterator_free(iter);
	}

	unsigned int i;
	for (i = 0; i < nb_mirror_workers; i++) {
		struct soj_create_archive_worker * worker = mirror_workers[i];

		if (worker->state != soj_worker_status_reserved)
			continue;

		worker->media_iterator = soj_media_get_iterator(worker->pool);
		vmedia = so_value_iterator_get_value(worker->media_iterator, false);
		worker->media = so_value_custom_get(vmedia);
		worker->drive = soj_media_load(worker->media, true);
		worker->checksums = db_connect->ops->get_checksums_from_pool(db_connect, worker->pool);

		if (worker->drive != NULL) {
			worker->writer = worker->drive->ops->get_writer(worker->drive, worker->checksums);
			worker->state = soj_worker_status_ready;
		}

		struct so_value_iterator * iter = so_value_list_get_iterator(worker->checksums);
		while (so_value_iterator_has_next(iter)) {
			struct so_value * val = so_value_iterator_get_value(iter, false);
			if (!so_value_hashtable_has_key(checksums, val))
				so_value_hashtable_put(checksums, val, false, val, false);
		}
		so_value_iterator_free(iter);
	}

	struct so_value * unique_checksums = so_value_hashtable_keys(checksums);
	so_value_free(checksums);
	soj_create_archive_meta_worker_start(unique_checksums);
	so_value_free(unique_checksums);
}

void soj_create_archive_worker_prepare_medias2() {
	bool found = false;
	unsigned int i;
	for (i = 0; i < nb_mirror_workers; i++) {
		struct soj_create_archive_worker * worker = mirror_workers[i];

		if (worker->state != soj_worker_status_reserved)
			continue;

		worker->drive = soj_media_load(worker->media, true);

		if (worker->drive != NULL) {
			worker->writer = worker->drive->ops->get_writer(worker->drive, worker->checksums);
			worker->state = soj_worker_status_ready;
			found = true;
		}
	}

	if (found)
		return;

	for (i = 0; i < nb_mirror_workers; i++) {
		struct soj_create_archive_worker * worker = mirror_workers[i];

		if (worker->state != soj_worker_status_reserved)
			continue;

		worker->drive = soj_media_load(worker->media, false);

		if (worker->drive != NULL) {
			worker->writer = worker->drive->ops->get_writer(worker->drive, worker->checksums);
			worker->state = soj_worker_status_ready;
			found = true;
		}
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

void soj_create_archive_worker_reserve_medias(struct so_job * job, ssize_t archive_size, struct so_database_connection * db_connect) {
	primary_worker->archive_size = archive_size;
	primary_worker->state = soj_worker_status_reserved;

	unsigned int i;
	for (i = 0; i < nb_mirror_workers; i++) {
		struct soj_create_archive_worker * worker = mirror_workers[i];

		ssize_t reserved = soj_media_prepare(worker->pool, archive_size, db_connect);
		if (reserved < archive_size) {
			reserved += soj_media_prepare_unformatted(worker->pool, true, db_connect);

			if (reserved < archive_size)
				reserved += soj_media_prepare_unformatted(worker->pool, false, db_connect);
		}

		if (reserved >= archive_size) {
			worker->state = soj_worker_status_reserved;
			worker->archive_size = archive_size;
		} else {
			worker->state = soj_worker_status_give_up;
			soj_media_release_all_medias(worker->pool);

			char str_reserved[12];
			char str_archive_size[12];

			so_file_convert_size_to_string(reserved, str_reserved, 12);
			so_file_convert_size_to_string(archive_size, str_archive_size, 12);

			so_job_add_record(job, db_connect, so_log_level_warning, so_job_record_notif_important,
				dgettext("storiqone-job-create-archive", "Warning, we will not archive into pool '%s' because we can not reserve enough space on it (space reserved: %s, space require: %s)"),
				worker->pool->name, str_reserved, str_archive_size);
		}
	}
}

int soj_create_archive_worker_sync_archives(struct so_database_connection * db_connect) {
	int failed = db_connect->ops->sync_archive(db_connect, primary_worker->archive);

	unsigned int i;
	for (i = 0; i < nb_mirror_workers && failed == 0; i++) {
		struct soj_create_archive_worker * worker = mirror_workers[i];
		failed = db_connect->ops->sync_archive(db_connect, worker->archive);
	}

	return failed;
}

ssize_t soj_create_archive_worker_write(struct so_job * job, struct so_format_file * file, const char * buffer, ssize_t length, struct so_database_connection * db_connect) {
	ssize_t nb_write = 0;
	if (primary_worker->state == soj_worker_status_ready) {
		nb_write = soj_create_archive_worker_write2(job, primary_worker, file, buffer, length, db_connect);
		if (nb_write < 0)
			primary_worker->state = soj_worker_status_error;
	}

	unsigned int i;
	for (i = 0; i < nb_mirror_workers; i++) {
		struct soj_create_archive_worker * worker = mirror_workers[i];

		if (worker->state != soj_worker_status_ready)
			continue;

		nb_write = soj_create_archive_worker_write2(job, worker, file, buffer, length, db_connect);
		if (nb_write < 0)
			return nb_write;
	}

	return nb_write;
}

ssize_t soj_create_archive_worker_write2(struct so_job * job, struct soj_create_archive_worker * worker, struct so_format_file * file, const char * buffer, ssize_t length, struct so_database_connection * db_connect) {
	ssize_t available = worker->writer->ops->get_available_size(worker->writer);
	ssize_t nb_total_write = 0;
	while (nb_total_write < length) {
		if (available <= 0) {
			int failed = soj_create_archive_worker_change_volume(job, worker, db_connect);
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

static bool soj_create_archive_worker_write_meta(struct soj_create_archive_worker * worker) {
	struct so_value * archive = so_archive_convert(worker->archive);
	char * json_archive = so_json_encode_to_string(archive);
	ssize_t length = strlen(json_archive);

	struct so_stream_writer * writer = worker->drive->ops->get_raw_writer(worker->drive);
	writer->ops->write(writer, json_archive, length);
	writer->ops->close(writer);
	writer->ops->free(writer);

	free(json_archive);
	so_value_free(archive);

	return true;
}

