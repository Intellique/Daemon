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
// NULL
#include <stddef.h>
// S_*
#include <sys/stat.h>
// time
#include <time.h>

#include <libstoriqone/archive.h>
#include <libstoriqone/database.h>
#include <libstoriqone/format.h>
#include <libstoriqone/io.h>
#include <libstoriqone/job.h>
#include <libstoriqone/log.h>
#include <libstoriqone-job/drive.h>
#include <libstoriqone-job/media.h>

#include "common.h"

int soj_copyarchive_indirect_copy(struct so_job * job, struct so_database_connection * db_connect, struct soj_copyarchive_private * self) {
	so_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_normal,
		dgettext("storiqone-job-copy-archive", "Select copy mode: indirect"));

	struct so_stream_writer * tmp_file_writer = so_io_tmp_writer();
	if (tmp_file_writer == NULL) {
		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-copy-archive", "Failed to create temporary file"));
		return 1;
	}

	struct so_format_writer * tmp_frmt_writer = so_format_tar_new_writer(tmp_file_writer, NULL);

	enum so_format_reader_header_status rdr_status;
	struct so_format_file file;
	static time_t last_update = 0;
	last_update = time(NULL);

	unsigned int i;
	for (i = 0; i < self->src_archive->nb_volumes; i++) {
		struct so_archive_volume * vol = self->src_archive->volumes + i;

		if (i > 0) {
			self->src_drive = soj_media_find_and_load(vol->media, false, 0, db_connect);
			if (self->src_drive == NULL) {
				so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
					dgettext("storiqone-job-copy-archive", "Failed to load media '%s'"),
					vol->media->name);

				job->status = so_job_status_error;

				tmp_frmt_writer->ops->free(tmp_frmt_writer);

				return 1;
			}
		}

		struct so_format_reader * reader = self->src_drive->ops->get_reader(self->src_drive, vol->media_position, NULL);

		while (rdr_status = reader->ops->get_header(reader, &file), rdr_status == so_format_reader_header_ok) {
			enum so_format_writer_status wrtr_status = tmp_frmt_writer->ops->add_file(tmp_frmt_writer, &file);

			if (wrtr_status != so_format_writer_ok) {
				so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
					dgettext("storiqone-job-copy-archive", "Error while writing header '%s' into temporary file"),
					file.filename);

				tmp_frmt_writer->ops->free(tmp_frmt_writer);

				return 1;
			}

			if (S_ISREG(file.mode)) {
				ssize_t nb_read, nb_total_read = 0;
				static char buffer[65535];
				while (nb_read = reader->ops->read(reader, buffer, 65535), nb_read > 0) {
					ssize_t nb_total_write = 0;
					while (nb_total_write < nb_read) {
						ssize_t nb_write = tmp_frmt_writer->ops->write(tmp_frmt_writer, buffer + nb_total_write, nb_read - nb_total_write);
						if (nb_write >= 0)
							nb_total_write += nb_write;
						else {
							so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
								dgettext("storiqone-job-copy-archive", "Error while writing data of file '%s' into temporary file"),
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
					so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
						dgettext("storiqone-job-copy-archive", "Error while reading from media '%s'"),
						vol->media->name);

					tmp_frmt_writer->ops->free(tmp_frmt_writer);

					return 1;
				}

				tmp_frmt_writer->ops->end_of_file(tmp_frmt_writer);
			}

			so_format_file_free(&file);
		}

		reader->ops->free(reader);
	}

	job->done = 0.5;

	struct so_format_reader * tmp_frmt_reader = tmp_frmt_writer->ops->reopen(tmp_frmt_writer);
	if (tmp_frmt_reader == NULL) {
		tmp_frmt_writer->ops->free(tmp_frmt_writer);

		so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
			dgettext("storiqone-job-copy-archive", "Failed to reopen temporary file"));
		return 1;
	}

	tmp_frmt_writer->ops->free(tmp_frmt_writer);
	tmp_file_writer = NULL;

	struct so_value * checksums = db_connect->ops->get_checksums_from_pool(db_connect, self->pool);

	self->dest_drive = soj_media_load(self->media, false);
	struct so_format_writer * writer = self->dest_drive->ops->get_writer(self->dest_drive, checksums);

	while (rdr_status = tmp_frmt_reader->ops->get_header(tmp_frmt_reader, &file), rdr_status == so_format_reader_header_ok) {
		ssize_t size_available = writer->ops->get_available_size(writer);
		if (size_available == 0) {
			// TODO: change volume
		}

		enum so_format_writer_status wrtr_status = writer->ops->add_file(writer, &file);
		if (wrtr_status != so_format_writer_ok) {
			so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
				dgettext("storiqone-job-copy-archive", "Error while writing header '%s' into media '%s'"),
				file.filename, self->media->name);
			break;
		}

		if (S_ISREG(file.mode)) {
			size_available = writer->ops->get_available_size(writer);
			if (size_available == 0) {
				// TODO: change volume
			}

			ssize_t nb_read, nb_total_read = 0;
			static char buffer[65535];

			ssize_t will_read = size_available < 65535 ? size_available : 65535;
			while (nb_read = tmp_frmt_reader->ops->read(tmp_frmt_reader, buffer, will_read), nb_read > 0) {
				size_available = writer->ops->get_available_size(writer);
				if (size_available == 0) {
					// TODO: change volume
				}

				ssize_t nb_total_write = 0;
				while (nb_total_write < nb_read) {
					ssize_t nb_write = writer->ops->write(writer, buffer + nb_total_write, nb_read - nb_total_write);
					if (nb_write >= 0)
						nb_total_write += nb_write;
					else {
						so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
							dgettext("storiqone-job-copy-archive", "Error while writing data of file '%s' into temporary file"),
							file.filename);

						return 1;
					}
				}

				time_t now = time(NULL);
				if (now > last_update + 5) {
					last_update = now;

					float done = tmp_frmt_reader->ops->position(tmp_frmt_reader) + nb_total_read;
					done *= 0.49;
					job->done = 0.5 + done / self->src_archive->size;
				}
			}

			if (nb_read < 0) {
				so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
					dgettext("storiqone-job-copy-archive", "Error while reading from temporary file"));
				break;
			}
		}

		so_format_file_free(&file);
	}

	writer->ops->close(writer);

	writer->ops->free(writer);
	tmp_frmt_reader->ops->free(tmp_frmt_reader);

	job->done = 1;

	return 0;
}

