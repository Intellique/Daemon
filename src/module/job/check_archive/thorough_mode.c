/****************************************************************************\
*                             __________                                     *
*                            / __/_  __/__  ___  ___                         *
*                           _\ \  / / / _ \/ _ \/ -_)                        *
*                          /___/ /_/  \___/_//_/\__/                         *
*                                                                            *
*  ------------------------------------------------------------------------  *
*  This file is a part of STone                                              *
*                                                                            *
*  STone is free software; you can redistribute it and/or modify             *
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
*  Copyright (C) 2013, Clercin guillaume <gclercin@intellique.com>           *
*  Last modified: Thu, 16 Jan 2014 16:32:06 +0100                            *
\****************************************************************************/

// free
#include <stdlib.h>
// S_*
#include <sys/stat.h>
// S_*
#include <sys/types.h>
// sleep
#include <unistd.h>


#include <libstone/format.h>
#include <libstone/io.h>
#include <libstone/job.h>
#include <libstone/library/drive.h>
#include <libstone/library/ressource.h>
#include <libstone/library/slot.h>
#include <libstone/log.h>
#include <libstone/util/hashtable.h>
#include <stoned/library/changer.h>

#include "common.h"

static struct st_stream_reader * st_job_check_archive_thorough_mode_add_filter(struct st_stream_reader * reader, void * param);


int st_job_check_archive_thorough_mode(struct st_job_check_archive_private * self) {
	unsigned int i;
	ssize_t total_read = 0;
	unsigned int nb_errors = 0;

	struct st_stream_writer * file_check = NULL;
	bool error = false;
	for (i = 0; i < self->archive->nb_volumes; i++) {
		struct st_archive_volume * vol = self->archive->volumes + i;
		ssize_t total_read_vol = 0;

		bool stop = false, has_alerted_user = false;
		struct st_slot * slot = NULL;
		struct st_drive * drive = NULL;
		while (!stop) {
			slot = st_changer_find_slot_by_media(vol->media);
			if (slot == NULL) {
				// slot not found
				// TODO: alert user
				if (!has_alerted_user)
					st_job_add_record(self->connect, st_log_level_warning, self->job, st_job_record_notif_important, "Warning, media named (%s) is not found, please insert it", vol->media->name);
				has_alerted_user = true;
				self->job->sched_status = st_job_status_pause;

				sleep(5);

				continue;
			}

			struct st_changer * changer = slot->changer;
			drive = slot->drive;

			self->job->sched_status = st_job_status_pause;

			if (drive != NULL) {
				if (drive->lock->ops->timed_lock(drive->lock, 5000))
					continue;

				stop = true;
			} else {
				if (slot->lock->ops->timed_lock(slot->lock, 5000))
					continue;

				drive = changer->ops->find_free_drive(changer, slot->media->format, true, false);

				if (drive == NULL) {
					slot->lock->ops->unlock(slot->lock);

					sleep(5);
					continue;
				}

				stop = true;
			}
		}

		self->job->sched_status = st_job_status_running;

		if (drive->slot->media != NULL && drive->slot != slot) {
			st_job_add_record(self->connect, st_log_level_info, self->job, st_job_record_notif_normal, "Unloading media (%s)", drive->slot->media->name);
			int failed = drive->changer->ops->unload(drive->changer, drive);
			if (failed) {
				st_job_add_record(self->connect, st_log_level_info, self->job, st_job_record_notif_important, "Unloading media (%s) has failed", drive->slot->media->name);
				nb_errors++;
				goto end_of_work;
			}
		}

		if (drive->slot->media == NULL) {
			st_job_add_record(self->connect, st_log_level_info, self->job, st_job_record_notif_normal, "Loading media (%s)", slot->media->name);

			int failed = drive->changer->ops->load_slot(drive->changer, slot, drive);

			slot->lock->ops->unlock(slot->lock);

			if (failed) {
				st_job_add_record(self->connect, st_log_level_info, self->job, st_job_record_notif_important, "Loading media (%s) has failed", slot->media->name);
				nb_errors++;
				goto end_of_work;
			}
		}

		st_job_check_archive_report_add_volume(self->report, vol);

		self->nb_vol_checksums = self->connect->ops->get_checksums_of_archive_volume(self->connect, vol);
		self->vol_checksums = (char **) st_hashtable_keys(vol->digests, NULL);

		struct st_format_reader * reader = drive->ops->get_reader(drive, vol->media_position, st_job_check_archive_thorough_mode_add_filter, self);
		ssize_t block_size = reader->ops->get_block_size(reader);

		unsigned int j;
		for (j = 0; j < vol->nb_files && !error; j++) {
			struct st_archive_files * f = vol->files + j;
			struct st_archive_file * file = f->file;

			st_job_check_archive_report_add_file(self->report, f);

			ssize_t position = reader->ops->position(reader) / block_size;
			if (position != f->position) {
				st_job_add_record(self->connect, st_log_level_error, self->job, st_job_record_notif_important, "Position of file is invalid");
				nb_errors++;
				error = true;
				break;
			}

			struct st_format_file header;
			enum st_format_reader_header_status status = reader->ops->get_header(reader, &header);

			switch (status) {
				case st_format_reader_header_bad_header:
					st_job_add_record(self->connect, st_log_level_error, self->job, st_job_record_notif_important, "Error while reading header");
					nb_errors++;
					error = true;
					break;

				case st_format_reader_header_not_found:
					st_job_add_record(self->connect, st_log_level_error, self->job, st_job_record_notif_important, "No header found but we expected one");
					nb_errors++;
					error = true;
					break;

				case st_format_reader_header_ok:
					break;
			}

			if (error)
				break;

			if (file->type != st_archive_file_type_regular_file) {
				st_format_file_free(&header);
				continue;
			}

			if (file_check == NULL) {
				unsigned int nb_file_checksum = self->connect->ops->get_checksums_of_file(self->connect, file);
				const void ** file_checksums = st_hashtable_keys(file->digests, NULL);

				file_check = st_checksum_writer_new(NULL, (char **) file_checksums, nb_file_checksum, true);

				free(file_checksums);
			}

			char buffer[4096];
			ssize_t nb_read, total_read_file = 0;
			while (nb_read = reader->ops->read(reader, buffer, 4096), nb_read > 0) {
				file_check->ops->write(file_check, buffer, nb_read);
				total_read_file += nb_read;

				total_read_vol = reader->ops->position(reader);

				float done = total_read_vol + total_read;
				done *= 0.97;
				done /= self->archive_size;
				self->job->done = 0.02 + done;
			}

			if (total_read_file == header.size) {
				file_check->ops->close(file_check);

				struct st_hashtable * results = st_checksum_writer_get_checksums(file_check);
				bool ok = st_hashtable_equals(file->digests, results);
				st_hashtable_free(results);

				file_check->ops->free(file_check);
				file_check = NULL;

				st_job_check_archive_report_check_file(self->report, ok);

				if (ok) {
					self->connect->ops->mark_archive_file_as_checked(self->connect, self->archive, file, true);
					st_job_add_record(self->connect, st_log_level_info, self->job, st_job_record_notif_normal, "Checking file (%s), status: OK", file->name);
				} else {
					self->connect->ops->mark_archive_file_as_checked(self->connect, self->archive, file, false);
					st_job_add_record(self->connect, st_log_level_error, self->job, st_job_record_notif_important, "Checking file (%s), status: checksum mismatch", file->name);
				}
			}

			st_format_file_free(&header);
		}

		total_read += total_read_vol;

		reader->ops->read_to_end_of_data(reader);

		reader->ops->close(reader);

		if (!error && self->checksum_reader != NULL) {
			struct st_hashtable * results = st_checksum_reader_get_checksums(self->checksum_reader);
			bool ok = st_hashtable_equals(vol->digests, results);
			st_hashtable_free(results);

			st_job_check_archive_report_check_volume(self->report, ok);

			if (ok) {
				self->connect->ops->mark_archive_volume_as_checked(self->connect, vol, true);
				st_job_add_record(self->connect, st_log_level_info, self->job, st_job_record_notif_normal, "Checking volume #%lu, status: OK", vol->sequence);
			} else {
				self->connect->ops->mark_archive_volume_as_checked(self->connect, vol, false);
				st_job_add_record(self->connect, st_log_level_error, self->job, st_job_record_notif_important, "Checking volume #%lu, status: checksum mismatch", vol->sequence);
			}
		}

		reader->ops->free(reader);

end_of_work:
		drive->lock->ops->unlock(drive->lock);
	}

	if (!error) {
		self->job->done = 0.99;

		self->connect->ops->mark_archive_as_checked(self->connect, self->archive, true);

		self->job->done = 1;
	} else {
		self->connect->ops->mark_archive_as_checked(self->connect, self->archive, false);
		self->job->sched_status = st_job_status_error;
	}

	return error;
}

static struct st_stream_reader * st_job_check_archive_thorough_mode_add_filter(struct st_stream_reader * reader, void * param) {
	struct st_job_check_archive_private * self = param;

	if (self->nb_vol_checksums > 0)
		return self->checksum_reader = st_checksum_reader_new(reader, self->vol_checksums, self->nb_vol_checksums, true);

	return reader;
}

