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

	struct so_value * checksums = db_connect->ops->get_checksums_from_pool(db_connect, self->copy_archive->pool);

	struct so_archive_volume * dest_vol = so_archive_add_volume(self->copy_archive);
	dest_vol->job = job;

	self->writer = self->dest_drive->ops->create_archive_volume(self->dest_drive, dest_vol, checksums);

	unsigned int i;
	int failed = 0;
	bool ok = true;
	ssize_t nb_total_read = 0, block_size = self->writer->ops->get_block_size(self->writer);
	struct so_media * media = self->dest_drive->slot->media;
	for (i = 0; i < self->src_archive->nb_volumes; i++) {
		unsigned int j = 0;

		struct so_archive_volume * src_vol = self->src_archive->volumes + i;

		if (i > 0) {
			self->src_drive = soj_media_find_and_load(src_vol->media, false, 0, false, NULL, NULL, db_connect);
			if (self->src_drive == NULL) {
				soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
					dgettext("storiqone-job-copy-archive", "Failed to load media '%s'"),
					src_vol->media->name);

				job->status = so_job_status_error;

				return 1;
			}

			if (dest_vol->max_version < src_vol->max_version)
				dest_vol->max_version = src_vol->max_version;
		} else {
			dest_vol->min_version = src_vol->min_version;
			dest_vol->max_version = src_vol->max_version;
		}

		struct so_format_reader * reader = self->src_drive->ops->open_archive_volume(self->src_drive, src_vol, NULL);

		while (rdr_status = reader->ops->get_header(reader, &file, NULL, NULL), rdr_status == so_format_reader_header_ok) {
			ssize_t available_size = self->writer->ops->get_available_size(self->writer);
			if (available_size == 0 || (S_ISREG(file.mode) && self->writer->ops->compute_size_of_file(self->writer, &file) > available_size && self->copy_archive->pool->unbreakable_level == so_pool_unbreakable_level_file)) {
				failed = soj_copyarchive_util_change_media(job, db_connect, self);
				if (failed != 0) {
					ok = false;
					break;
				}

				dest_vol = self->copy_archive->volumes + (self->copy_archive->nb_volumes - 1);
				dest_vol->min_version = src_vol->min_version;
				dest_vol->max_version = src_vol->max_version;
			}

			if (file.position == 0) {
				soj_copyarchive_util_add_file(self, &file, block_size);

				soj_job_add_record(job, db_connect, so_log_level_notice, so_job_record_notif_normal,
					dgettext("storiqone-job-copy-archive", "Add file '%s' to archive"),
					file.filename);

				struct so_archive_files * ptr_file = src_vol->files + j++;
				enum so_format_writer_status wrtr_status = self->writer->ops->add_file(self->writer, &file, ptr_file->file->selected_path);
				if (wrtr_status != so_format_writer_ok) {
					soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
						dgettext("storiqone-job-copy-archive", "Error while writing file header '%s' to media '%s'"),
						file.filename, media->name);
					ok = false;
					break;
				}
			}

			if (S_ISREG(file.mode)) {
				available_size = self->writer->ops->get_available_size(self->writer);
				if (available_size == 0) {
					soj_copyarchive_util_change_media(job, db_connect, self);
					if (failed != 0) {
						ok = false;
						break;
					}

					dest_vol = self->copy_archive->volumes + (self->copy_archive->nb_volumes - 1);
					dest_vol->min_version = src_vol->min_version;
					dest_vol->max_version = src_vol->max_version;

					block_size = self->writer->ops->get_block_size(self->writer);
					soj_copyarchive_util_add_file(self, &file, block_size);

					enum so_format_writer_status wrtr_status = self->writer->ops->restart_file(self->writer, &file);
					if (wrtr_status != so_format_writer_ok) {
						soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
							dgettext("storiqone-job-copy-archive", "Error while writing file header '%s' to media '%s'"),
							file.filename, media->name);
						ok = false;
						break;
					}

					available_size = self->writer->ops->get_available_size(self->writer);
				}

				ssize_t nb_read;
				static char buffer[65536];

				ssize_t will_read = available_size < 65536 ? available_size : 65536;
				while (nb_read = reader->ops->read(reader, buffer, will_read), nb_read > 0) {
					ssize_t nb_total_write = 0;
					while (nb_total_write < nb_read) {
						ssize_t nb_write = self->writer->ops->write(self->writer, buffer + nb_total_write, nb_read - nb_total_write);
						if (nb_write >= 0) {
							nb_total_write += nb_write;
							file.position += nb_write;
						} else {
							soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
								dgettext("storiqone-job-copy-archive", "Error while writing file data '%s' to media '%s'"),
								file.filename, media->name);

							ok = false;
							return 1;
						}
					}

					available_size = self->writer->ops->get_available_size(self->writer);
					if (available_size == 0) {
						failed = soj_copyarchive_util_change_media(job, db_connect, self);
						if (failed != 0) {
							ok = false;
							break;
						}

						dest_vol = self->copy_archive->volumes + (self->copy_archive->nb_volumes - 1);
						dest_vol->min_version = src_vol->min_version;
						dest_vol->max_version = src_vol->max_version;

						block_size = self->writer->ops->get_block_size(self->writer);
						soj_copyarchive_util_add_file(self, &file, block_size);

						enum so_format_writer_status wrtr_status = self->writer->ops->restart_file(self->writer, &file);
						if (wrtr_status != so_format_writer_ok) {
							soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
								dgettext("storiqone-job-copy-archive", "Error while writing file header '%s' to media '%s'"),
								file.filename, media->name);
							ok = false;
							break;
						}

						available_size = self->writer->ops->get_available_size(self->writer);
					}
					will_read = available_size < 65536 ? available_size : 65536;

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
						src_vol->media->name);

					ok = false;
					break;
				}

				if (file.position == file.size)
					self->writer->ops->end_of_file(self->writer);
			}

			so_format_file_free(&file);
		}

		nb_total_read += reader->ops->position(reader);

		reader->ops->close(reader);
		reader->ops->free(reader);
	}

	soj_copyarchive_util_close_media(job, db_connect, self, false);

	job->done = 0.99;

	if (failed == 0 && !ok)
		failed = 1;

	return failed;
}
