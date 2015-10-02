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
// malloc
#include <stdlib.h>
// strdup
#include <string.h>
// S_*
#include <sys/stat.h>
// time
#include <time.h>

#include <libstoriqone/archive.h>
#include <libstoriqone/database.h>
#include <libstoriqone/format.h>
#include <libstoriqone/log.h>
#include <libstoriqone/slot.h>
#include <libstoriqone-job/drive.h>
#include <libstoriqone-job/job.h>
#include <libstoriqone-job/media.h>

#include "common.h"

int soj_copyarchive_direct_copy(struct so_job * job, struct so_database_connection * db_connect, struct soj_copyarchive_private * self) {
	soj_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_normal,
		dgettext("storiqone-job-copy-archive", "Selected copy mode: direct"));

	enum so_format_reader_header_status rdr_status;
	struct so_format_file file;
	static time_t last_update = 0;
	last_update = time(NULL);

	soj_copyarchive_util_init(self->src_archive);

	struct so_value * checksums = db_connect->ops->get_checksums_from_pool(db_connect, self->pool);

	struct so_archive_volume * vol = so_archive_add_volume(self->copy_archive);
	struct so_media * media = vol->media = self->dest_drive->slot->media;
	vol->job = job;

	self->writer = self->dest_drive->ops->get_writer(self->dest_drive, checksums);

	unsigned int i;
	int failed = 0;
	bool ok = true;
	for (i = 0; i < self->src_archive->nb_volumes; i++) {
		struct so_archive_volume * vol = self->src_archive->volumes + i;

		if (i > 0) {
			self->src_drive = soj_media_find_and_load(vol->media, false, 0, db_connect);
			if (self->src_drive == NULL) {
				soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
					dgettext("storiqone-job-copy-archive", "Failed to load media '%s'"),
					vol->media->name);

				job->status = so_job_status_error;

				return 1;
			}
		}

		struct so_format_reader * reader = self->src_drive->ops->get_reader(self->src_drive, vol->media_position, NULL);

		while (rdr_status = reader->ops->get_header(reader, &file), rdr_status == so_format_reader_header_ok) {
			ssize_t available_size = self->writer->ops->get_available_size(self->writer);
			if (available_size == 0 || (S_ISREG(file.mode) && self->writer->ops->compute_size_of_file(self->writer, &file) > available_size && self->pool->unbreakable_level == so_pool_unbreakable_level_file)) {
				failed = soj_copyarchive_util_change_media(job, db_connect, self);
				if (failed != 0) {
					ok = false;
					break;
				}
			}

			enum so_format_writer_status wrtr_status = self->writer->ops->add_file(self->writer, &file);
			if (wrtr_status != so_format_writer_ok) {
				soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
					dgettext("storiqone-job-copy-archive", "Error while writing file header '%s' to media '%s'"),
					file.filename, media->name);
				ok = false;
				break;
			}

			struct soj_copyarchive_files * ptr_file = malloc(sizeof(struct soj_copyarchive_files));
			ptr_file->path = strdup(file.filename);
			ptr_file->position = self->writer->ops->position(self->writer);
			ptr_file->archived_time = time(NULL);
			ptr_file->next = NULL;
			self->nb_files++;

			if (self->first_files == NULL)
				self->first_files = self->last_files = ptr_file;
			else
				self->last_files = self->last_files->next = ptr_file;

			if (S_ISREG(file.mode)) {
				available_size = self->writer->ops->get_available_size(self->writer);
				if (available_size == 0) {
					soj_copyarchive_util_change_media(job, db_connect, self);
					if (failed != 0) {
						ok = false;
						break;
					}
				}

				ssize_t nb_read, nb_total_read = 0;
				static char buffer[65535];

				ssize_t will_read = available_size < 65535 ? available_size : 65535;
				while (nb_read = reader->ops->read(reader, buffer, will_read), nb_read > 0) {
					available_size = self->writer->ops->get_available_size(self->writer);
					if (available_size == 0) {
						failed = soj_copyarchive_util_change_media(job, db_connect, self);
						if (failed != 0) {
							ok = false;
							break;
						}
					}

					ssize_t nb_total_write = 0;
					while (nb_total_write < nb_read) {
						ssize_t nb_write = self->writer->ops->write(self->writer, buffer + nb_total_write, nb_read - nb_total_write);
						if (nb_write >= 0)
							nb_total_write += nb_write;
						else {
							soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
								dgettext("storiqone-job-copy-archive", "Error while writing file data '%s' to media '%s'"),
								file.filename, media->name);

							ok = false;
							return 1;
						}
					}

					time_t now = time(NULL);
					if (now > last_update + 5) {
						last_update = now;

						float done = reader->ops->position(reader) + nb_total_read;
						done *= 0.98;
						job->done = 0.01 + done / self->src_archive->size;
					}
				}

				if (nb_read < 0) {
					soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
						dgettext("storiqone-job-copy-archive", "Error while reading from media '%s'"),
						vol->media->name);

					ok = false;
					break;
				}

				self->writer->ops->end_of_file(self->writer);
			}

			so_format_file_free(&file);
		}
	}

	soj_copyarchive_util_close_media(job, db_connect, self);

	job->done = 0.99;

	if (failed == 0 && !ok)
		failed = 1;

	return failed;
}

