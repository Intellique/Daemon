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
*  Copyright (C) 2013-2021, Guillaume Clercin <gclercin@intellique.com>      *
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
#include <libstoriqone/host.h>
#include <libstoriqone/io.h>
#include <libstoriqone/json.h>
#include <libstoriqone/log.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/value.h>
#include <libstoriqone-job/drive.h>
#include <libstoriqone-job/job.h>
#include <libstoriqone-job/io.h>
#include <libstoriqone-job/media.h>

#include "meta_worker.h"
#include "worker.h"

struct soj_create_archive_worker {
	struct so_archive * archive;
	struct so_value * checksums;

	ssize_t total_done;
	ssize_t partial_done;
	ssize_t archive_size;

	struct so_media * media;
	struct so_drive * drive;
	struct so_format_writer * writer;

	ssize_t last_position;
	ssize_t last_write;
	bool is_sync_write;
	bool is_completed;

	struct soj_create_archive_files {
		char * path;
		char * alternate_path;
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

static enum so_format_writer_status soj_create_archive_worker_add_file_async(struct soj_create_archive_worker * worker, struct so_format_file * file, const char * selected_path, struct so_database_connection * db_connect);
static enum so_format_writer_status soj_create_archive_worker_add_file_return(struct soj_create_archive_worker * worker, struct so_format_file * file, struct so_database_connection * db_connect);
static void soj_create_archive_add_file3(struct soj_create_archive_worker * worker, struct so_format_file * file, char * alternate_path, ssize_t position);
static int soj_create_archive_worker_change_volume(struct soj_create_archive_worker * worker, struct so_format_file * file, struct so_database_connection * db_connect);
static int soj_create_archive_worker_close2(struct soj_create_archive_worker * worker, bool skip_last_file, bool change_volume, struct so_database_connection * db_connect);
static struct so_archive_file * soj_create_archive_worker_copy_file(struct soj_create_archive_worker * worker, struct so_archive_file * file, char * alternate_path);
static int soj_create_archive_worker_create_check_archive2(struct soj_create_archive_worker * worker, struct so_value * option, struct so_database_connection * db_connect);
static void soj_create_archive_worker_generate_report2(struct so_archive * archive, struct so_value * selected_path, struct so_database_connection * db_connect);
static struct soj_create_archive_worker * soj_create_archive_worker_new(struct so_job * job, struct so_archive * archive, struct so_pool * pool);
static void soj_create_archive_worker_write_async(struct soj_create_archive_worker * worker, struct so_format_file * file, const char * buffer, ssize_t length, struct so_database_connection * db_connect);
static bool soj_create_archive_worker_write_meta(struct soj_create_archive_worker * worker);
static ssize_t soj_create_archive_worker_write_return(struct soj_create_archive_worker * worker);

static struct so_job * current_job = NULL;
static bool soj_create_archive_adding_file = false;
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

enum so_format_writer_status soj_create_archive_worker_add_file(struct so_format_file * file, const char * selected_path, struct so_database_connection * db_connect) {
	if (primary_worker->state == soj_worker_status_ready) {
		enum so_format_writer_status status = soj_create_archive_worker_add_file_async(primary_worker, file, selected_path, db_connect);
		if (status != so_format_writer_ok) {
			soj_job_add_record(current_job, db_connect, so_log_level_error, so_job_record_notif_important,
				dgettext("storiqone-job-create-archive", "Error while adding file (%s) to pool %s"),
				file->filename, primary_worker->archive->pool->name);
			soj_create_archive_worker_close2(primary_worker, false, true, db_connect);
			primary_worker->state = soj_worker_status_error;
			return status;
		}
	}

	unsigned int i;
	for (i = 0; i < nb_mirror_workers; i++) {
		struct soj_create_archive_worker * worker = mirror_workers[i];

		if (worker->state != soj_worker_status_ready)
			continue;

		enum so_format_writer_status status = soj_create_archive_worker_add_file_async(worker, file, selected_path, db_connect);
		if (status != so_format_writer_ok) {
			soj_job_add_record(current_job, db_connect, so_log_level_error, so_job_record_notif_important,
				dgettext("storiqone-job-create-archive", "Error while adding file (%s) to pool %s"),
				file->filename, worker->archive->pool->name);
			soj_create_archive_worker_close2(worker, false, true, db_connect);
			worker->state = soj_worker_status_error;
		}
	}

	if (primary_worker->state == soj_worker_status_ready) {
		enum so_format_writer_status status = soj_create_archive_worker_add_file_return(primary_worker, file, db_connect);
		if (status != so_format_writer_ok) {
			soj_job_add_record(current_job, db_connect, so_log_level_error, so_job_record_notif_important,
				dgettext("storiqone-job-create-archive", "Error while adding file (%s) to pool %s"),
				file->filename, primary_worker->archive->pool->name);
			soj_create_archive_worker_close2(primary_worker, false, true, db_connect);
			primary_worker->state = soj_worker_status_error;
			return status;
		}
	}

	for (i = 0; i < nb_mirror_workers; i++) {
		struct soj_create_archive_worker * worker = mirror_workers[i];

		if (worker->state != soj_worker_status_ready)
			continue;

		enum so_format_writer_status status = soj_create_archive_worker_add_file_return(worker, file, db_connect);
		if (status != so_format_writer_ok) {
			soj_job_add_record(current_job, db_connect, so_log_level_error, so_job_record_notif_important,
				dgettext("storiqone-job-create-archive", "Error while adding file (%s) to pool %s"),
				file->filename, worker->archive->pool->name);
			soj_create_archive_worker_close2(worker, false, true, db_connect);
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

static enum so_format_writer_status soj_create_archive_worker_add_file_async(struct soj_create_archive_worker * worker, struct so_format_file * file, const char * selected_path, struct so_database_connection * db_connect) {
	worker->last_position = worker->writer->ops->position(worker->writer) / worker->writer->ops->get_block_size(worker->writer);

	if (worker->archive->pool->unbreakable_level == so_pool_unbreakable_level_file) {
		ssize_t available_size = worker->writer->ops->get_available_size(worker->writer);
		ssize_t file_size = worker->writer->ops->compute_size_of_file(worker->writer, file);

		if (available_size < file_size) {
			if (soj_create_archive_worker_change_volume(worker, NULL, db_connect) != 0)
				return so_format_writer_error;

			worker->last_position = 0;
		} else {
			char str_available_size[12], str_file_size[12];
			so_file_convert_size_to_string(available_size, str_available_size, 12);
			so_file_convert_size_to_string(file_size, str_file_size, 12);

			so_log_write(so_log_level_debug, "Available size: %s, File size: %s", str_available_size, str_file_size);
		}
	}

	soj_format_writer_add_file_async(worker->writer, file, selected_path);
	return so_format_writer_ok;
}

static enum so_format_writer_status soj_create_archive_worker_add_file_return(struct soj_create_archive_worker * worker, struct so_format_file * file, struct so_database_connection * db_connect) {
	enum so_format_writer_status status = soj_format_writer_add_file_return(worker->writer);

	int failed = 0;
	switch (status) {
		case so_format_writer_end_of_volume:
			failed = soj_create_archive_worker_change_volume(worker, file, db_connect);
			if (failed != 0)
				return so_format_writer_error;
			worker->last_position = 0;
			break;

		case so_format_writer_error:
		case so_format_writer_unsupported:
			return so_format_writer_error;

		case so_format_writer_ok:
			break;
	}

	char * alternate_path = worker->writer->ops->get_alternate_path(worker->writer);
	soj_create_archive_add_file3(worker, file, alternate_path, worker->last_position);

	worker->partial_done = worker->writer->ops->position(worker->writer);

	return so_format_writer_ok;
}

static void soj_create_archive_add_file3(struct soj_create_archive_worker * worker, struct so_format_file * file, char * alternate_path, ssize_t position) {
	struct soj_create_archive_files * new_file = malloc(sizeof(struct soj_create_archive_files));
	new_file->path = strdup(file->filename);
	new_file->alternate_path = alternate_path;
	new_file->position = position;
	new_file->archived_time = time(NULL);
	new_file->next = NULL;

	if (worker->nb_files > 0)
		worker->last_files = worker->last_files->next = new_file;
	else
		worker->first_files = worker->last_files = new_file;
	worker->nb_files++;
	worker->position = 0;
}

static int soj_create_archive_worker_change_volume(struct soj_create_archive_worker * worker, struct so_format_file * file, struct so_database_connection * db_connect) {
	soj_create_archive_worker_close2(worker, false, true, db_connect);

	worker->writer->ops->free(worker->writer);
	worker->writer = NULL;

	worker->drive = soj_media_find_and_load_next(worker->archive->pool, false, NULL, db_connect);
	if (worker->drive != NULL) {
		worker->drive->ops->sync(worker->drive);
		worker->media = worker->drive->slot->media;

		soj_job_add_record(current_job, db_connect, so_log_level_info, so_job_record_notif_important,
			dgettext("storiqone-job-create-archive", "Archive continue on media (%s)"),
			worker->media->name);

		struct so_archive_volume * vol = so_archive_add_volume(worker->archive);
		vol->job = soj_job_get();

		worker->writer = worker->drive->ops->create_archive_volume(worker->drive, vol, worker->checksums);

		if (file != NULL) {
			char * alternate_path = worker->writer->ops->get_alternate_path(worker->writer);
			soj_create_archive_add_file3(worker, file, alternate_path, 0);
		}

		return 0;
	} else {
		soj_job_add_record(current_job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-create-archive", "No more media on pool '%s'"),
			worker->archive->pool->name);

		return 1;
	}
}

int soj_create_archive_worker_close(struct so_job * job, struct so_database_connection * db_connect) {
	soj_create_archive_meta_worker_wait(false);

	if (primary_worker->state == soj_worker_status_ready) {
		int failed = soj_create_archive_worker_close2(primary_worker, false, false, db_connect);
		if (failed != 0) {
			primary_worker->state = soj_worker_status_error;
			soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
				dgettext("storiqone-job-create-archive", "Error while closing archive '%s' from pool '%s'"),
				primary_worker->archive->name, primary_worker->archive->pool->name);
		} else
			primary_worker->archive->status = primary_worker->is_completed ? so_archive_status_data_complete : so_archive_status_incomplete;
	}

	unsigned int i;
	for (i = 0; i < nb_mirror_workers; i++) {
		struct soj_create_archive_worker * worker = mirror_workers[i];

		if (worker->state != soj_worker_status_ready)
			continue;

		int failed = soj_create_archive_worker_close2(worker, false, false, db_connect);
		if (failed != 0) {
			worker->state = soj_worker_status_error;
			soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
				dgettext("storiqone-job-create-archive", "Error while closing archive '%s' from pool '%s'"),
				worker->archive->name, worker->archive->pool->name);
		} else
			worker->archive->status = worker->is_completed ? so_archive_status_data_complete : so_archive_status_incomplete;
	}

	if (primary_worker->state == soj_worker_status_ready) {
		soj_create_archive_worker_write_meta(primary_worker);
		primary_worker->archive->status = so_archive_status_complete;

		primary_worker->writer->ops->free(primary_worker->writer);
		primary_worker->writer = NULL;
	} else if (primary_worker->state == soj_worker_status_error && primary_worker->writer != NULL) {
		primary_worker->writer->ops->free(primary_worker->writer);
		primary_worker->writer = NULL;
	}

	for (i = 0; i < nb_mirror_workers; i++) {
		struct soj_create_archive_worker * worker = mirror_workers[i];

		if (worker->state == soj_worker_status_ready) {
			soj_create_archive_worker_write_meta(worker);
			worker->archive->status = so_archive_status_complete;
		}

		if (worker->writer != NULL)
			worker->writer->ops->free(worker->writer);
		worker->writer = NULL;
	}

	return 0;
}

static int soj_create_archive_worker_close2(struct soj_create_archive_worker * worker, bool skip_last_file, bool change_volume, struct so_database_connection * db_connect) {
	if (worker->writer == NULL)
		return 0;

	worker->writer->ops->close(worker->writer, change_volume);

	worker->partial_done = 0;
	worker->total_done += worker->writer->ops->position(worker->writer);

	struct so_archive_volume * last_vol = worker->archive->volumes + (worker->archive->nb_volumes - 1);
	last_vol->end_time = time(NULL);
	last_vol->size = worker->writer->ops->position(worker->writer);

	last_vol->digests = worker->writer->ops->get_digests(worker->writer);

	soj_create_archive_meta_worker_wait(false);

	struct so_value * files_metadata = NULL;
	so_value_unpack(current_job->meta, "{so}", "files", &files_metadata);

	if (skip_last_file)
		worker->nb_files--;

	unsigned int i;
	struct soj_create_archive_files * ptr_file = worker->first_files;
	struct so_value * files = soj_create_archive_meta_worker_get_files();
	last_vol->files = calloc(worker->nb_files, sizeof(struct so_archive_files));
	last_vol->nb_files = worker->nb_files;
	for (i = 0; i < last_vol->nb_files; i++) {
		struct so_value * vfile = so_value_hashtable_get2(files, ptr_file->path, false, false);

		struct so_archive_files * new_file = last_vol->files + i;
		new_file->file = soj_create_archive_worker_copy_file(worker, so_value_custom_get(vfile), ptr_file->alternate_path);
		new_file->position = ptr_file->position;
		new_file->archived_time = ptr_file->archived_time;
		new_file->file->min_version = new_file->file->max_version = worker->archive->current_version;

		if (files_metadata != NULL && so_value_hashtable_has_key2(files_metadata, new_file->file->path))
			new_file->file->metadata = so_value_hashtable_get2(files_metadata, new_file->file->path, true, false);
		else
			new_file->file->metadata = so_value_new_hashtable2();

		worker->first_files = ptr_file->next;
		free(ptr_file->path);
		free(ptr_file);
		ptr_file = worker->first_files;
	}

	if (worker->first_files != NULL) {
		free(worker->first_files->path);
		free(worker->first_files);
	}

	worker->first_files = worker->last_files = NULL;
	worker->nb_files = 0;

	soj_create_archive_worker_sync_archives(false, false, worker->archive, db_connect);

	return 0;
}

static struct so_archive_file * soj_create_archive_worker_copy_file(struct soj_create_archive_worker * worker, struct so_archive_file * file, char * alternate_path) {
	struct so_archive_file * new_file = malloc(sizeof(struct so_archive_file));
	bzero(new_file, sizeof(struct so_archive_file));

	new_file->path = strdup(file->path);
	new_file->alternate_path = alternate_path;
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

	so_archive_file_update_hash(new_file);

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

int soj_create_archive_worker_create_check_archive(struct so_value * option, struct so_database_connection * db_connect) {
	int failed = 0;

	if (primary_worker->state == soj_worker_status_finished)
		failed = soj_create_archive_worker_create_check_archive2(primary_worker, option, db_connect);

	unsigned int i;
	for (i = 0; i < nb_mirror_workers; i++) {
		struct soj_create_archive_worker * worker = mirror_workers[i];

		if (worker->state == soj_worker_status_finished)
			failed += soj_create_archive_worker_create_check_archive2(worker, option, db_connect);
	}

	return failed;
}

static int soj_create_archive_worker_create_check_archive2(struct soj_create_archive_worker * worker, struct so_value * option, struct so_database_connection * db_connect) {
	bool check_archive = worker->archive->pool->auto_check == so_pool_autocheck_quick_mode || worker->archive->pool->auto_check == so_pool_autocheck_thorough_mode;
	so_value_unpack(option, "{sb}", "check_archive", &check_archive);

	if (check_archive) {
		bool quick_mode = worker->archive->pool->auto_check == so_pool_autocheck_quick_mode;

		char * mode = NULL;
		so_value_unpack(option, "{ss}", "check_archive_mode", &mode);
		if (mode != NULL) {
			quick_mode = strcmp(mode, "quick_mode") == 0;
			free(mode);
		}

		return db_connect->ops->create_check_archive_job(db_connect, current_job, worker->archive, quick_mode);
	} else
		return 0;
}

ssize_t soj_create_archive_worker_end_of_file() {
	if (primary_worker->state == soj_worker_status_ready) {
		ssize_t nb_write = primary_worker->writer->ops->end_of_file(primary_worker->writer);
		if (nb_write < 0)
			primary_worker->state = soj_worker_status_error;
	}

	unsigned int i;
	for (i = 0; i < nb_mirror_workers; i++) {
		struct soj_create_archive_worker * worker = mirror_workers[i];

		if (worker->state != soj_worker_status_ready)
			continue;

		ssize_t nb_write = worker->writer->ops->end_of_file(worker->writer);
		if (nb_write < 0)
			worker->state = soj_worker_status_error;
	}

	return 0;
}

bool soj_create_archive_worker_finished() {
	if (primary_worker->state != soj_worker_status_give_up && primary_worker->state != soj_worker_status_error && primary_worker->state != soj_worker_status_finished)
		return false;

	unsigned int i;
	for (i = 0; i < nb_mirror_workers; i++) {
		struct soj_create_archive_worker * worker = mirror_workers[i];
		if (worker->state != soj_worker_status_give_up && worker->state != soj_worker_status_error && worker->state != soj_worker_status_finished)
			return false;
	}

	return true;
}

enum so_job_status soj_create_archive_worker_finished_with_errors() {
	unsigned int nb_archives = 1, nb_good_archives = 0;

	if (primary_worker->state == soj_worker_status_finished)
		nb_good_archives++;

	unsigned int i;
	for (i = 0; i < nb_mirror_workers; i++) {
		struct soj_create_archive_worker * worker = mirror_workers[i];
		if (worker->state == soj_worker_status_finished)
			nb_good_archives++;
		nb_archives++;
	}

	if (nb_archives == nb_good_archives)
		return so_job_status_finished;
	else if (nb_archives > 0)
		return so_job_status_finished_with_warnings;
	else
		return so_job_status_error;
}

void soj_create_archive_worker_generate_report(struct so_value * selected_path, struct so_database_connection * db_connect) {
	soj_create_archive_worker_generate_report2(primary_worker->archive, selected_path, db_connect);

	unsigned int i;
	for (i = 0; i < nb_mirror_workers; i++) {
		struct soj_create_archive_worker * worker = mirror_workers[i];

		if (worker->state != soj_worker_status_ready)
			continue;

		soj_create_archive_worker_generate_report2(worker->archive, selected_path, db_connect);
	}
}

static void soj_create_archive_worker_generate_report2(struct so_archive * archive, struct so_value * selected_path, struct so_database_connection * db_connect) {
	if (archive->nb_volumes < 1)
		return;

	struct so_archive_volume * vol = archive->volumes;
	struct so_pool * pool = vol->media->pool;

	struct so_value * report = so_value_pack("{sisososososO}",
		"report version", 2,
		"job", so_job_convert(current_job),
		"host", so_host_get_info2(),
		"pool", so_pool_convert(pool),
		"archive", so_archive_convert(archive),
		"selected paths", selected_path
	);

	char * json = so_json_encode_to_string(report);
	db_connect->ops->add_report(db_connect, current_job, archive, NULL, json);

	free(json);
	so_value_free(report);
}

void soj_create_archive_worker_init_archive(struct so_job * job, struct so_archive * primary_archive, struct so_value * archive_mirrors) {
	primary_archive->current_version++;

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

	soj_create_archive_adding_file = true;
}

void soj_create_archive_worker_init_pool(struct so_job * job, struct so_pool * primary_pool, struct so_value * pool_mirrors) {
	primary_worker = soj_create_archive_worker_new(job, NULL, primary_pool); 

	nb_mirror_workers = so_value_list_get_length(pool_mirrors);
	mirror_workers = NULL;

	if (nb_mirror_workers > 0) {
		mirror_workers = calloc(nb_mirror_workers, sizeof(struct soj_create_archive_worker *));

		struct so_value_iterator * iter = so_value_list_get_iterator(pool_mirrors);
		unsigned int i;
		for (i = 0; so_value_iterator_has_next(iter); i++) {
			struct so_value * vpool = so_value_iterator_get_value(iter, false);
			struct so_pool * pool = so_value_custom_get(vpool);
			mirror_workers[i] = soj_create_archive_worker_new(job, NULL, pool);
		}
		so_value_iterator_free(iter);
	}
}

void soj_create_archive_worker_marks_as_imcompleted() {
	if (primary_worker->state == soj_worker_status_ready)
		primary_worker->is_completed = false;

	unsigned int i;
	for (i = 0; i < nb_mirror_workers; i++) {
		struct soj_create_archive_worker * worker = mirror_workers[i];
		worker->is_completed = false;
	}
}

static struct soj_create_archive_worker * soj_create_archive_worker_new(struct so_job * job, struct so_archive * archive, struct so_pool * pool) {
	current_job = job;

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
		worker->archive->metadata = so_value_new_hashtable2();
		worker->archive->pool = pool;
		worker->archive->current_version = 1;
	} else {
		worker->archive = archive;
		worker->archive->status = so_archive_status_incomplete;
	}

	struct so_value * metadata = NULL;
	so_value_unpack(job->meta, "{so}", "archive", &metadata);
	if (metadata != NULL) {
		struct so_value_iterator * iter = so_value_hashtable_get_iterator(metadata);
		while (so_value_iterator_has_next(iter)) {
			struct so_value * key = so_value_iterator_get_key(iter, false, true);
			struct so_value * value = so_value_iterator_get_value(iter, true);
			so_value_hashtable_put(worker->archive->metadata, key, true, value, true);
		}
		so_value_iterator_free(iter);
	}

	worker->total_done = worker->partial_done = 0;
	worker->archive_size = 0;

	worker->media = NULL;
	worker->drive = NULL;

	worker->last_position = 0;
	worker->last_write = 0;
	worker->is_sync_write = false;
	worker->is_completed = true;

	worker->first_files = worker->last_files = NULL;
	worker->nb_files = 0;
	worker->position = 0;

	worker->state = soj_worker_status_not_ready;

	return worker;
}

void soj_create_archive_worker_prepare_medias(struct so_job * job, struct so_database_connection * db_connect) {
	struct so_value * checksums = so_value_new_hashtable2();

	primary_worker->drive = soj_media_find_and_load_next(primary_worker->archive->pool, false, NULL, db_connect);

	if (primary_worker->drive != NULL) {
		primary_worker->checksums = db_connect->ops->get_checksums_from_pool(db_connect, primary_worker->archive->pool);

		struct so_archive_volume * vol = so_archive_add_volume(primary_worker->archive);
		vol->job = soj_job_get();

		primary_worker->writer = primary_worker->drive->ops->create_archive_volume(primary_worker->drive, vol, primary_worker->checksums);
	} else
		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-create-archive", "Error while getting drive from pool '%s'"),
			primary_worker->archive->pool->name);

	if (primary_worker->drive != NULL && primary_worker->writer != NULL) {
		primary_worker->media = primary_worker->drive->slot->media;
		primary_worker->state = soj_worker_status_ready;

		struct so_value_iterator * iter = so_value_list_get_iterator(primary_worker->checksums);
		while (so_value_iterator_has_next(iter)) {
			struct so_value * val = so_value_iterator_get_value(iter, false);
			if (!so_value_hashtable_has_key(checksums, val))
				so_value_hashtable_put(checksums, val, false, val, false);
		}
		so_value_iterator_free(iter);
	} else {
		primary_worker->state = soj_worker_status_error;
		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-create-archive", "Error while opening media from pool '%s'"),
			primary_worker->archive->pool->name);
	}

	unsigned int i;
	for (i = 0; i < nb_mirror_workers; i++) {
		struct soj_create_archive_worker * worker = mirror_workers[i];

		if (worker->state != soj_worker_status_reserved)
			continue;

		bool error = false;
		worker->drive = soj_media_find_and_load_next(worker->archive->pool, true, &error, db_connect);
		worker->checksums = db_connect->ops->get_checksums_from_pool(db_connect, worker->archive->pool);

		if (error) {
			worker->state = soj_worker_status_give_up;
			soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
				dgettext("storiqone-job-create-archive", "Error while getting drive from pool '%s'"),
				worker->archive->pool->name);
		} else if (worker->drive != NULL) {
			struct so_archive_volume * vol = so_archive_add_volume(worker->archive);
			vol->job = soj_job_get();

			worker->writer = worker->drive->ops->create_archive_volume(worker->drive, vol, worker->checksums);
		} else {
			soj_job_add_record(job, db_connect, so_log_level_warning, so_job_record_notif_read,
				dgettext("storiqone-job-create-archive", "No drive currently available for pool '%s', will retry later"),
				worker->archive->pool->name);
			continue;
		}

		if (!error && worker->drive != NULL && worker->writer != NULL) {
			worker->media = worker->drive->slot->media;
			worker->state = soj_worker_status_ready;

			struct so_value_iterator * iter = so_value_list_get_iterator(worker->checksums);
			while (so_value_iterator_has_next(iter)) {
				struct so_value * val = so_value_iterator_get_value(iter, false);
				if (!so_value_hashtable_has_key(checksums, val))
					so_value_hashtable_put(checksums, val, false, val, false);
			}
			so_value_iterator_free(iter);
		} else {
			worker->state = soj_worker_status_give_up;
			soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
				dgettext("storiqone-job-create-archive", "Error while opening media '%s' from pool '%s'"),
				worker->media->name, worker->archive->pool->name);
		}
	}

	struct so_value * unique_checksums = so_value_hashtable_keys(checksums);
	so_value_free(checksums);
	soj_create_archive_meta_worker_start(unique_checksums);
	so_value_free(unique_checksums);
}

bool soj_create_archive_worker_prepare_medias2(struct so_job * job, struct so_database_connection * db_connect) {
	unsigned int i, nb_loaded = 0;
	for (i = 0; i < nb_mirror_workers; i++) {
		struct soj_create_archive_worker * worker = mirror_workers[i];

		if (worker->state != soj_worker_status_reserved)
			continue;

		bool error = false;
		worker->drive = soj_media_find_and_load_next(worker->archive->pool, nb_loaded != 0, &error, db_connect);

		if (error) {
			worker->state = soj_worker_status_give_up;
			soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
				dgettext("storiqone-job-create-archive", "Error while getting drive from pool '%s'"),
				worker->archive->pool->name);
			continue;
		} else if (worker->drive != NULL)
			worker->writer = worker->drive->ops->get_writer(worker->drive, worker->checksums);
		else {
			if (nb_loaded > 0)
				soj_job_add_record(job, db_connect, so_log_level_warning, so_job_record_notif_read,
					dgettext("storiqone-job-create-archive", "No drive currently available for pool '%s', will retry later"),
					worker->archive->pool->name);
			continue;
		}

		if (worker->drive != NULL && worker->writer != NULL) {
			nb_loaded++;

			worker->media = worker->drive->slot->media;
			worker->state = soj_worker_status_ready;

			struct so_archive_volume * vol = so_archive_add_volume(worker->archive);
			vol->media = so_media_dup(worker->media);
			vol->media_position = worker->writer->ops->file_position(worker->writer);
			vol->job = soj_job_get();
		} else {
			worker->state = soj_worker_status_give_up;
			soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
				dgettext("storiqone-job-create-archive", "Error while opening media '%s' from pool '%s'"),
				worker->media->name, worker->archive->pool->name);
		}
	}

	return nb_loaded > 0;
}

float soj_create_archive_progress() {
	ssize_t nb_done = primary_worker->total_done + primary_worker->partial_done;
	ssize_t nb_total = primary_worker->archive_size;

	unsigned int i;
	for (i = 0; i < nb_mirror_workers; i++) {
		struct soj_create_archive_worker * worker = mirror_workers[i];

		if (worker->state < soj_worker_status_reserved)
			continue;

		nb_done += worker->total_done + worker->partial_done;
		nb_total += worker->archive_size;
	}

	return ((float) nb_done) / nb_total;
}

void soj_create_archive_worker_reserve_medias(ssize_t archive_size, struct so_database_connection * db_connect) {
	primary_worker->archive_size = archive_size;
	primary_worker->state = soj_worker_status_reserved;

	unsigned int i;
	for (i = 0; i < nb_mirror_workers; i++) {
		struct soj_create_archive_worker * worker = mirror_workers[i];

		ssize_t reserved = soj_media_prepare(worker->archive->pool, archive_size, true, worker->archive->uuid, db_connect);
		if (reserved >= archive_size) {
			worker->state = soj_worker_status_reserved;
			worker->archive_size = archive_size;
		} else {
			worker->state = soj_worker_status_give_up;
			soj_media_release_all_medias(worker->archive->pool);

			char str_reserved[12];
			char str_archive_size[12];

			so_file_convert_size_to_string(reserved, str_reserved, 12);
			so_file_convert_size_to_string(archive_size, str_archive_size, 12);

			soj_job_add_record(current_job, db_connect, so_log_level_warning, so_job_record_notif_important,
				dgettext("storiqone-job-create-archive", "Warning, not archiving to pool '%s': not enough space (space reserved: %s, space required: %s)"),
				worker->archive->pool->name, str_reserved, str_archive_size);
		}
	}
}

int soj_create_archive_worker_sync_archives(bool first_synchro, bool close_archive, struct so_archive * archive, struct so_database_connection * db_connect) {
	int failed = db_connect->ops->start_transaction(db_connect);
	if (failed != 0) {
		soj_job_add_record(current_job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-create-archive", "Failed to start database transaction"));
		return 1;
	}

	if (primary_worker->state == soj_worker_status_ready && (archive == NULL || archive == primary_worker->archive)) {
		failed = db_connect->ops->sync_archive(db_connect, primary_worker->archive, NULL, close_archive);

		if (failed == 0 && close_archive)
			failed = db_connect->ops->update_link_archive(db_connect, primary_worker->archive, current_job);

		if (failed != 0) {
			db_connect->ops->cancel_transaction(db_connect);
			primary_worker->state = soj_worker_status_error;
			return failed;
		} else if (close_archive)
			primary_worker->state = soj_worker_status_finished;
	}

	unsigned int i;
	for (i = 0; i < nb_mirror_workers && failed == 0; i++) {
		struct soj_create_archive_worker * worker = mirror_workers[i];

		if (archive != NULL && archive != worker->archive)
			continue;

		if ((worker->state == soj_worker_status_reserved && first_synchro) || worker->state == soj_worker_status_ready) {
			failed = db_connect->ops->sync_archive(db_connect, worker->archive, primary_worker->archive, close_archive);

			if (failed == 0) {
				if (first_synchro && !soj_create_archive_adding_file) {
					struct so_pool * pool = primary_worker->archive->pool;
					if (pool == NULL && primary_worker->archive != NULL && primary_worker->archive->nb_volumes > 0 && primary_worker->archive->volumes->media != NULL)
						pool = primary_worker->archive->volumes->media->pool;

					failed = db_connect->ops->link_archives(db_connect, current_job, primary_worker->archive, worker->archive, pool);
				} else if (close_archive)
					failed = db_connect->ops->update_link_archive(db_connect, worker->archive, current_job);
			}

			if (failed != 0) {
				db_connect->ops->cancel_transaction(db_connect);
				worker->state = soj_worker_status_error;
				return failed;
			} else if (close_archive)
				worker->state = soj_worker_status_finished;
		}
	}

	db_connect->ops->finish_transaction(db_connect);

	return 0;
}

ssize_t soj_create_archive_worker_write(struct so_format_file * file, const char * buffer, ssize_t length, struct so_database_connection * db_connect) {
	ssize_t nb_write = 0;
	if (primary_worker->state == soj_worker_status_ready)
		soj_create_archive_worker_write_async(primary_worker, file, buffer, length, db_connect);

	unsigned int i;
	for (i = 0; i < nb_mirror_workers; i++) {
		struct soj_create_archive_worker * worker = mirror_workers[i];

		if (worker->state != soj_worker_status_ready)
			continue;

		soj_create_archive_worker_write_async(worker, file, buffer, length, db_connect);
	}

	if (primary_worker->state == soj_worker_status_ready) {
		nb_write = soj_create_archive_worker_write_return(primary_worker);
		if (nb_write < 0) {
			primary_worker->state = soj_worker_status_error;
			soj_create_archive_worker_close2(primary_worker, true, true, db_connect);
		}
	}

	for (i = 0; i < nb_mirror_workers; i++) {
		struct soj_create_archive_worker * worker = mirror_workers[i];

		if (worker->state != soj_worker_status_ready)
			continue;

		nb_write = soj_create_archive_worker_write_return(worker);
		if (nb_write < 0) {
			worker->state = soj_worker_status_error;
			soj_create_archive_worker_close2(worker, true, true, db_connect);
		}
	}

	return nb_write;
}

static void soj_create_archive_worker_write_async(struct soj_create_archive_worker * worker, struct so_format_file * file, const char * buffer, ssize_t length, struct so_database_connection * db_connect) {
	ssize_t available = worker->writer->ops->get_available_size(worker->writer);
	ssize_t nb_total_write = 0;
	ssize_t current_file_position = file->position;
	while (nb_total_write < length) {
		if (available <= 0) {
			int failed = soj_create_archive_worker_change_volume(worker, file, db_connect);
			if (failed) {
				file->position = current_file_position;
				worker->state = soj_worker_status_error;
				return;
			}

			worker->writer->ops->restart_file(worker->writer, file);

			available = worker->writer->ops->get_available_size(worker->writer);
		}

		ssize_t will_write = length - nb_total_write;
		if (will_write >= available)
			will_write = available;

		ssize_t nb_write;
		if (nb_total_write + will_write == length) {
			soj_format_writer_write_async(worker->writer, buffer + nb_total_write, will_write);
			nb_write = will_write;

			worker->is_sync_write = false;
		} else {
			worker->last_write = nb_write = worker->writer->ops->write(worker->writer, buffer + nb_total_write, will_write);
			worker->is_sync_write = true;
			if (nb_write < 0) {
				file->position = current_file_position;
				worker->state = soj_worker_status_error;
				return;
			}
		}

		file->position += nb_write;
		nb_total_write += nb_write;
		available -= nb_write;
		worker->position += nb_write;
	}

	file->position = current_file_position;
	worker->partial_done = worker->writer->ops->position(worker->writer);
}

static bool soj_create_archive_worker_write_meta(struct soj_create_archive_worker * worker) {
	struct so_value * archive = so_archive_convert(worker->archive);
	ssize_t nb_write = worker->writer->ops->write_metadata(worker->writer, archive);
	so_value_free(archive);

	return nb_write <= 0;
}

static ssize_t soj_create_archive_worker_write_return(struct soj_create_archive_worker * worker) {
	if (worker->is_sync_write)
		return worker->last_write;
	else
		return soj_format_writer_write_return(worker->writer);
}
