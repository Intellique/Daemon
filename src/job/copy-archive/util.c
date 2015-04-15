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
// calloc
#include <stdlib.h>
// time
#include <time.h>

#include <libstoriqone/archive.h>
#include <libstoriqone/format.h>
#include <libstoriqone/job.h>
#include <libstoriqone/log.h>
#include <libstoriqone/value.h>
#include <libstoriqone-job/media.h>

#include "common.h"

static struct so_value * files = NULL;

int soj_copyarchive_util_change_media(struct so_job * job, struct so_database_connection * db_connect, struct soj_copyarchive_private * self) {
	int failed = soj_copyarchive_util_close_media(job, db_connect, self);
	if (failed != 0)
		return failed;

	if (!so_value_iterator_has_next(self->media_iterator))
		return 2;

	struct so_value * vmedia = so_value_iterator_get_value(self->media_iterator, false);
	self->media = so_value_custom_get(vmedia);
	self->dest_drive = soj_media_load(self->media, false);
	if (self->dest_drive == NULL)
		return 3;

	return 0;
}

int soj_copyarchive_util_close_media(struct so_job * job, struct so_database_connection * db_connect, struct soj_copyarchive_private * self) {
	int failed = self->writer->ops->close(self->writer);
	if (failed != 0) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
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
				so_value_hashtable_put2(files, file->path, so_value_new_custom(file, NULL), true);
		}
	}
}

