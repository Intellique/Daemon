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

// dgettext
#include <libintl.h>
// free, calloc
#include <stdlib.h>
// strlen
#include <string.h>
// time
#include <time.h>

#include <libstoriqone/archive.h>
#include <libstoriqone/database.h>
#include <libstoriqone/format.h>
#include <libstoriqone/io.h>
#include <libstoriqone/log.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/value.h>
#include <libstoriqone-job/drive.h>
#include <libstoriqone-job/job.h>
#include <libstoriqone-job/media.h>

#include "common.h"

static struct so_value * files = NULL;

void soj_copyarchive_util_add_file(struct soj_copyarchive_private * self, struct so_format_file * file, ssize_t block_size) {
	struct soj_copyarchive_files * ptr_file = malloc(sizeof(struct soj_copyarchive_files));
	ptr_file->path = strdup(file->filename);
	ptr_file->position = self->writer->ops->position(self->writer) / block_size;
	ptr_file->archived_time = time(NULL);
	ptr_file->next = NULL;
	self->nb_files++;

	if (self->first_files == NULL)
		self->first_files = self->last_files = ptr_file;
	else
		self->last_files = self->last_files->next = ptr_file;
}

int soj_copyarchive_util_change_media(struct so_job * job, struct so_database_connection * db_connect, struct soj_copyarchive_private * self) {
	int failed = soj_copyarchive_util_close_media(job, db_connect, self);
	if (failed != 0)
		return failed;

	self->writer->ops->free(self->writer);
	self->writer = NULL;

	self->dest_drive = soj_media_find_and_load_next(self->pool, false, NULL, db_connect);
	if (self->dest_drive == NULL)
		return 3;

	struct so_value * checksums = db_connect->ops->get_checksums_from_pool(db_connect, self->pool);

	struct so_archive_volume * vol = so_archive_add_volume(self->copy_archive);
	vol->job = job;

	self->writer = self->dest_drive->ops->create_archive_volume(self->dest_drive, vol, checksums);

	return 0;
}

int soj_copyarchive_util_close_media(struct so_job * job, struct so_database_connection * db_connect, struct soj_copyarchive_private * self) {
	int failed = self->writer->ops->close(self->writer);
	if (failed != 0) {
		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-copy-archive", "Failed to reopen temporary file"));
		return 1;
	}

	struct so_archive_volume * vol = self->copy_archive->volumes + (self->copy_archive->nb_volumes - 1);
	self->copy_archive->size += vol->size = self->writer->ops->position(self->writer);
	vol->end_time = time(NULL);
	vol->digests = self->writer->ops->get_digests(self->writer);

	vol->files = calloc(self->nb_files, sizeof(struct so_archive_files));
	vol->nb_files = self->nb_files;

	unsigned int i;
	struct soj_copyarchive_files * ptr_file;
	for (i = 0, ptr_file = self->first_files; i < vol->nb_files; i++) {
		struct so_archive_files * ptr_copy_file = vol->files + i;
		ptr_copy_file->position = ptr_file->position;
		ptr_copy_file->archived_time = ptr_file->archived_time;

		struct so_value * vfile = so_value_hashtable_get2(files, ptr_file->path, false, false);
		struct so_archive_file * file = so_value_custom_get(vfile);

		ptr_copy_file->file = so_archive_file_copy(file);
		ptr_copy_file->file->db_data = so_value_share(file->db_data);

		struct soj_copyarchive_files * old = ptr_file;
		ptr_file = ptr_file->next;

		free(old->path);
		free(old);
	}

	self->first_files = self->last_files = NULL;
	self->nb_files = 0;

	return 0;
}

void soj_copyarchive_util_init(struct so_archive * archive) {
	unsigned int i;
	files = so_value_new_hashtable2();
	for (i = 0; i < archive->nb_volumes; i++) {
		struct so_archive_volume * vol = archive->volumes + i;

		unsigned int j;
		for (j = 0; j < vol->nb_files; j++) {
			struct so_archive_files * ptr_file = vol->files + j;
			struct so_archive_file * file = ptr_file->file;

			if (!so_value_hashtable_has_key2(files, file->path))
				so_value_hashtable_put2(files, file->hash, so_value_new_custom(file, NULL), true);
		}
	}
}

int soj_copyarchive_util_sync_archive(struct so_job * job, struct so_archive * archive, struct so_database_connection * db_connect) {
	int failed = db_connect->ops->start_transaction(db_connect);
	if (failed != 0) {
		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-copy-archive", "Failed to start database transaction"));
	} else {
		failed = db_connect->ops->sync_archive(db_connect, archive, NULL);

		if (failed != 0)
			db_connect->ops->cancel_transaction(db_connect);
		else
			db_connect->ops->finish_transaction(db_connect);
	}

	return failed;
}

int soj_copyarchive_util_write_meta(struct soj_copyarchive_private * self) {
	struct so_value * archive = so_archive_convert(self->copy_archive);
	ssize_t nb_write = self->writer->ops->write_metadata(self->writer, archive);
	so_value_free(archive);

	return nb_write <= 0;
}
