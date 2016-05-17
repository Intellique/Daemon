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
#include <libstoriqone/file.h>
#include <libstoriqone/format.h>
#include <libstoriqone/io.h>
#include <libstoriqone/log.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/value.h>
#include <libstoriqone-job/drive.h>
#include <libstoriqone-job/job.h>
#include <libstoriqone-job/media.h>

#include "common.h"

int soj_copyarchive_indirect_copy(struct so_job * job, struct so_database_connection * db_connect, struct soj_copyarchive_private * self) {
	soj_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_normal,
		dgettext("storiqone-job-copy-archive", "Selected copy mode: indirect"));

	soj_copyarchive_util_init(self->src_archive);

	struct so_stream_writer * tmp_file_writer = so_io_tmp_writer();
	if (tmp_file_writer == NULL) {
		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-copy-archive", "Failed to create temporary file"));
		return 1;
	}

	ssize_t available_size = tmp_file_writer->ops->get_available_size(tmp_file_writer);
	if (available_size < self->src_archive->size * 1.1) {
		char buf_required[16], buf_available[16];
		so_file_convert_size_to_string(self->src_archive->size, buf_required, 16);
		so_file_convert_size_to_string(available_size, buf_available, 16);

		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-copy-archive", "Error, not enough disk space (required: %s, available: %s) to copy archive '%s'"),
			buf_required, buf_available, self->src_archive->name);

		tmp_file_writer->ops->free(tmp_file_writer);

		return 2;
	}

	struct so_format_writer * tmp_frmt_writer = so_format_tar_new_writer(tmp_file_writer, NULL);

	enum so_format_reader_header_status rdr_status;
	struct so_format_file file;
	static time_t last_update = 0;
	last_update = time(NULL);
	ssize_t nb_total_read = 0;

	unsigned int i;
	for (i = 0; i < self->src_archive->nb_volumes; i++) {
		unsigned int j = 0;
		struct so_archive_volume * vol = self->src_archive->volumes + i;

		if (i > 0) {
			self->src_drive = soj_media_find_and_load(vol->media, false, 0, db_connect);
			if (self->src_drive == NULL) {
				soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
					dgettext("storiqone-job-copy-archive", "Failed to load media '%s'"),
					vol->media->name);

				job->status = so_job_status_error;

				tmp_frmt_writer->ops->free(tmp_frmt_writer);

				return 1;
			}
		}

		struct so_format_reader * reader = self->src_drive->ops->open_archive_volume(self->src_drive, vol, NULL);

		while (rdr_status = reader->ops->get_header(reader, &file), rdr_status == so_format_reader_header_ok) {
			if (file.position == 0) {
				struct so_archive_files * ptr_file = vol->files + j++;
				enum so_format_writer_status wrtr_status = tmp_frmt_writer->ops->add_file(tmp_frmt_writer, &file, ptr_file->file->selected_path);

				if (wrtr_status != so_format_writer_ok) {
					soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
						dgettext("storiqone-job-copy-archive", "Error while writing file header '%s' to temporary file"),
						file.filename);

					tmp_frmt_writer->ops->free(tmp_frmt_writer);

					return 1;
				}
			}

			if (S_ISREG(file.mode)) {
				ssize_t nb_read;
				static char buffer[65536];
				while (nb_read = reader->ops->read(reader, buffer, 65536), nb_read > 0) {
					ssize_t nb_total_write = 0;
					while (nb_total_write < nb_read) {
						ssize_t nb_write = tmp_frmt_writer->ops->write(tmp_frmt_writer, buffer + nb_total_write, nb_read - nb_total_write);
						if (nb_write >= 0)
							nb_total_write += nb_write;
						else {
							soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
								dgettext("storiqone-job-copy-archive", "Error while writing file data '%s' to temporary file"),
								file.filename);

							tmp_frmt_writer->ops->free(tmp_frmt_writer);

							return 1;
						}
					}

					time_t now = time(NULL);
					if (now > last_update + 5) {
						last_update = now;

						float done = reader->ops->position(reader) + nb_total_read;
						done *= 0.49;
						job->done = 0.01 + done / self->src_archive->size;
					}
				}

				if (nb_read < 0) {
					soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
						dgettext("storiqone-job-copy-archive", "Error while reading from media '%s'"),
						vol->media->name);

					tmp_frmt_writer->ops->free(tmp_frmt_writer);

					return 1;
				}

				tmp_frmt_writer->ops->end_of_file(tmp_frmt_writer);
			}

			so_format_file_free(&file);
		}

		nb_total_read += reader->ops->position(reader);
		reader->ops->free(reader);
	}

	job->done = 0.5;

	struct so_format_reader * tmp_frmt_reader = tmp_frmt_writer->ops->reopen(tmp_frmt_writer);
	if (tmp_frmt_reader == NULL) {
		tmp_frmt_writer->ops->free(tmp_frmt_writer);

		soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-copy-archive", "Failed to reopen temporary file"));
		return 1;
	}

	tmp_frmt_writer->ops->free(tmp_frmt_writer);
	tmp_file_writer = NULL;

	struct so_value * checksums = db_connect->ops->get_checksums_from_pool(db_connect, self->pool);

	struct so_archive_volume * vol = so_archive_add_volume(self->copy_archive);

	self->dest_drive = soj_media_find_and_load_next(self->pool, false, db_connect);
	self->writer = self->dest_drive->ops->create_archive_volume(self->dest_drive, vol, checksums);

	struct so_media * media = self->dest_drive->slot->media;
	vol->job = job;

	vol->media_position = self->writer->ops->file_position(self->writer);

	bool ok = true;
	int failed = 0;
	ssize_t block_size = self->writer->ops->get_block_size(self->writer);

	i = 0;
	vol = self->src_archive->volumes;
	struct so_archive_files * ptr_file = vol->files;

	while (rdr_status = tmp_frmt_reader->ops->get_header(tmp_frmt_reader, &file), rdr_status == so_format_reader_header_ok && ok) {

		if (i < vol->nb_files)
			ptr_file++;
		else {
			vol++;
			ptr_file = vol->files;
			i = 0;
		}


		available_size = self->writer->ops->get_available_size(self->writer);
		if (available_size == 0 || (S_ISREG(file.mode) && self->writer->ops->compute_size_of_file(self->writer, &file) > available_size && self->pool->unbreakable_level == so_pool_unbreakable_level_file)) {
			failed = soj_copyarchive_util_change_media(job, db_connect, self);
			if (failed != 0) {
				ok = false;
				break;
			}

			block_size = self->writer->ops->get_block_size(self->writer);
		}

		soj_job_add_record(job, db_connect, so_log_level_notice, so_job_record_notif_normal,
			dgettext("storiqone-job-copy-archive", "Add file '%s' to archive"),
			file.filename);

		enum so_format_writer_status wrtr_status = self->writer->ops->add_file(self->writer, &file, ptr_file->file->selected_path);
		if (wrtr_status != so_format_writer_ok) {
			soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
				dgettext("storiqone-job-copy-archive", "Error while writing header of file '%s' into media '%s'"),
				file.filename, media->name);
			ok = false;
			break;
		}

		soj_copyarchive_util_add_file(self, &file, block_size);

		if (S_ISREG(file.mode)) {
			available_size = self->writer->ops->get_available_size(self->writer);
			if (available_size == 0) {
				soj_copyarchive_util_change_media(job, db_connect, self);
				if (failed != 0) {
					ok = false;
					break;
				}

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
			while (nb_read = tmp_frmt_reader->ops->read(tmp_frmt_reader, buffer, will_read), nb_read > 0 && ok) {
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

					float done = tmp_frmt_reader->ops->position(tmp_frmt_reader);
					done *= 0.49;
					job->done = 0.50 + done / nb_total_read;
				}
			}

			if (nb_read < 0) {
				soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
					dgettext("storiqone-job-copy-archive", "Error while reading from temporary file"));
				ok = false;
				break;
			}

			self->writer->ops->end_of_file(self->writer);
		}

		so_format_file_free(&file);
	}

	soj_copyarchive_util_close_media(job, db_connect, self);

	self->writer->ops->free(self->writer);
	tmp_frmt_reader->ops->free(tmp_frmt_reader);

	return failed;
}

