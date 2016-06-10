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
// S_*
#include <sys/stat.h>
// time
#include <time.h>
// sleep
#include <unistd.h>

#include <libstoriqone/archive.h>
#include <libstoriqone/database.h>
#include <libstoriqone/format.h>
#include <libstoriqone/io.h>
#include <libstoriqone/log.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/value.h>
#include <libstoriqone-job/changer.h>
#include <libstoriqone-job/drive.h>
#include <libstoriqone-job/job.h>
#include <libstoriqone-job/media.h>

#include "common.h"

int soj_checkarchive_thorough_mode(struct so_job * job, struct so_archive * archive, struct so_database_connection * db_connect) {
	soj_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_important,
		dgettext("storiqone-job-check-archive", "Starting check archive (%s) in thorough mode"),
		archive->name);

	job->done = 0.01;

	unsigned i;
	bool ok = true;
	ssize_t total_read = 0;
	struct so_stream_writer * chcksum_writer = NULL;
	for (i = 0; ok && i < archive->nb_volumes; i++) {
		struct so_archive_volume * vol = archive->volumes + i;

		struct so_value * checksums = so_value_hashtable_keys(vol->digests);
		if (so_value_list_get_length(checksums) == 0) {
			struct so_archive_file * file = NULL;
			unsigned int f;
			for (f = 0; f < vol->nb_files; f++) {
				file = vol->files[f].file;
				if (file->type == so_archive_file_type_regular_file)
					break;
			}

			if (file == NULL || so_value_hashtable_get_length(file->digests) == 0) {
				so_value_free(checksums);

				soj_job_add_record(job, db_connect, so_log_level_warning, so_job_record_notif_important,
					dgettext("storiqone-job-check-archive", "No checksums for volume #%u of archive '%s', ignore this volume"),
					i, vol->archive->name);

				total_read += vol->size;

				continue;
			}
		}

		struct so_drive * drive = soj_media_find_and_load(vol->media, false, 0, NULL, db_connect);
		if (drive == NULL) {
			// TODO: print error
			break;
		}

		if (job->stopped_by_user) {
			if (chcksum_writer != NULL)
				chcksum_writer->ops->free(chcksum_writer);

			job->status = so_job_status_stopped;
			return 1;
		}

		job->status = so_job_status_running;

		drive->ops->sync(drive);

		struct so_format_reader * reader = drive->ops->open_archive_volume(drive, vol, checksums);

		enum so_format_reader_header_status status;
		struct so_format_file file_in;
		unsigned int j = 0;
		static time_t last_update = 0;
		last_update = time(NULL);

		while (status = reader->ops->get_header(reader, &file_in), status == so_format_reader_header_ok) {
			if (S_ISREG(file_in.mode)) {
				struct so_archive_files * ptr_file = vol->files + j;
				struct so_archive_file * file = ptr_file->file;

				if (file_in.position == 0) {
					struct so_value * checksums = so_value_hashtable_keys(file->digests);
					if (so_value_list_get_length(checksums) > 0)
						chcksum_writer = so_io_checksum_writer_new(NULL, checksums, true);
					so_value_free(checksums);
				}

				static char buffer[32768];
				ssize_t nb_read;
				while (nb_read = reader->ops->read(reader, buffer, 32768), nb_read > 0 && !job->stopped_by_user) {
					file_in.position += nb_read;

					if (chcksum_writer != NULL)
						chcksum_writer->ops->write(chcksum_writer, buffer, nb_read);

					time_t now = time(NULL);
					if (now > last_update + 5) {
						last_update = now;

						float done = reader->ops->position(reader) + total_read;
						done *= 0.98;
						job->done = 0.01 + done / archive->size;
					}
				}

				if (job->stopped_by_user) {
					reader->ops->free(reader);
					chcksum_writer->ops->free(chcksum_writer);

					job->status = so_job_status_stopped;
					return 1;
				}

				if (nb_read < 0) {
					ok = false;

					soj_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_important,
						dgettext("storiqone-job-check-archive", "Error while reading from media '%s'"),
						vol->media->name);
				}

				if (chcksum_writer != NULL && file_in.position == file_in.size) {
					chcksum_writer->ops->close(chcksum_writer);

					struct so_value * digests = so_io_checksum_writer_get_checksums(chcksum_writer);
					file->check_ok = so_value_equals(file->digests, digests);
					file->check_time = time(NULL);

					if (so_value_hashtable_get_length(digests) > 0)
						db_connect->ops->check_archive_file(db_connect, archive, file);

					so_value_free(digests);

					chcksum_writer->ops->free(chcksum_writer);
					chcksum_writer = NULL;

					if (file->check_ok)
						soj_job_add_record(job, db_connect, so_log_level_notice, so_job_record_notif_important,
							dgettext("storiqone-job-check-archive", "Data integrity of file '%s' is correct"),
							file->path);
					else
						soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
							dgettext("storiqone-job-check-archive", "Data integrity of file '%s' is not correct"),
							file->path);
				}

				time_t now = time(NULL);
				if (now > last_update + 5) {
					last_update = now;

					float done = reader->ops->position(reader) + total_read;
					done *= 0.98;
					job->done = 0.01 + done / archive->size;
				}
			}

			so_format_file_free(&file_in);

			if (!ok)
				break;

			j++;
		}

		if (ok)
			reader->ops->read_to_end_of_data(reader);

		reader->ops->close(reader);
		if (ok) {
			struct so_value * digests = reader->ops->get_digests(reader);
			if (so_value_list_get_length(digests) > 0)
				ok = so_value_equals(vol->digests, digests);
		}

		vol->check_ok = ok;
		vol->check_time = time(NULL);
		total_read += vol->size;

		if (vol->check_ok)
			soj_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_important,
				dgettext("storiqone-job-check-archive", "Data integrity of volume '%s' is correct"),
				vol->media->name);
		else
			soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
				dgettext("storiqone-job-check-archive", "Data integrity of volume '%s' is not correct"),
				vol->media->name);

		if (so_value_list_get_length(checksums) != 0)
			db_connect->ops->check_archive_volume(db_connect, vol);

		so_value_free(checksums);
	}

	job->done = 1;

	return ok ? 0 : 1;
}

