/*************************************************************************\
*                            __________                                   *
*                           / __/_  __/__  ___  ___                       *
*                          _\ \  / / / _ \/ _ \/ -_)                      *
*                         /___/ /_/  \___/_//_/\__/                       *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  This file is a part of STone                                           *
*                                                                         *
*  STone is free software; you can redistribute it and/or                 *
*  modify it under the terms of the GNU General Public License            *
*  as published by the Free Software Foundation; either version 3         *
*  of the License, or (at your option) any later version.                 *
*                                                                         *
*  This program is distributed in the hope that it will be useful,        *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
*  GNU General Public License for more details.                           *
*                                                                         *
*  You should have received a copy of the GNU General Public License      *
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  Copyright (C) 2013, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Fri, 08 Feb 2013 09:50:23 +0100                         *
\*************************************************************************/

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
#include <stoned/library/changer.h>

#include "check_archive.h"

static struct st_stream_reader * st_job_check_archive_thorough_mode_add_filter(struct st_stream_reader * reader, void * param);


int st_job_check_archive_thorough_mode(struct st_job_check_archive_private * self) {
	struct st_archive * archive = self->connect->ops->get_archive_volumes_by_job(self->connect, self->job);

	unsigned int i;
	ssize_t total_read = 0, total_size = 0;
	for (i = 0; i < archive->nb_volumes; i++)
		total_size += archive->volumes[i].size;

	unsigned int nb_errors = 0;

	char ** file_checksums = NULL;
	unsigned int nb_checksum = 0;

	struct st_stream_writer * file_check = NULL;
	bool error = false;
	for (i = 0; i < archive->nb_volumes; i++) {
		struct st_archive_volume * vol = archive->volumes + i;
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
					st_job_add_record(self->connect, st_log_level_warning, self->job, "Warning, media named (%s) is not found, please insert it", vol->media->name);
				has_alerted_user = true;

				sleep(5);

				continue;
			}

			struct st_changer * changer = slot->changer;
			drive = slot->drive;

			if (drive != NULL) {
				if (drive->lock->ops->timed_lock(drive->lock, 5000))
					continue;

				stop = true;
			} else {
				if (slot->lock->ops->timed_lock(slot->lock, 5000))
					continue;

				drive = changer->ops->find_free_drive(changer, slot->media->format, true, false);

				if (drive == NULL) {
					sleep(5);
					continue;
				}

				stop = true;
			}
		}

		if (drive->slot->media != NULL && drive->slot != slot) {
			st_job_add_record(self->connect, st_log_level_info, self->job, "Unloading media (%s)", drive->slot->media->name);
			int failed = drive->changer->ops->unload(drive->changer, drive);
			if (failed) {
				st_job_add_record(self->connect, st_log_level_info, self->job, "Unloading media (%s) has failed", drive->slot->media->name);
				nb_errors++;
				goto end_of_work;
			}
		}

		if (drive->slot->media == NULL) {
			st_job_add_record(self->connect, st_log_level_info, self->job, "Loading media (%s)", slot->media->name);
			int failed = drive->changer->ops->load_slot(drive->changer, slot, drive);
			if (failed) {
				st_job_add_record(self->connect, st_log_level_info, self->job, "Loading media (%s) has failed", slot->media->name);
				nb_errors++;
				goto end_of_work;
			}
		}

		self->connect->ops->get_archive_files_by_job_and_archive_volume(self->connect, self->job, vol);

		self->vol_checksums = self->connect->ops->get_checksums_of_archive_volume(self->connect, vol, &self->nb_vol_checksums);

		struct st_format_reader * reader = drive->ops->get_reader(drive, vol->media_position, st_job_check_archive_thorough_mode_add_filter, self);
		ssize_t block_size = reader->ops->get_block_size(reader);

		unsigned int j;
		for (j = 0; j < vol->nb_files && !error; j++) {
			struct st_archive_files * f = vol->files + j;
			struct st_archive_file * file = f->file;

			ssize_t position = reader->ops->position(reader) / block_size;
			if (position != f->position) {
				st_job_add_record(self->connect, st_log_level_error, self->job, "Position of file is invalid");
				nb_errors++;
				error = true;
				break;
			}

			struct st_format_file header;
			enum st_format_reader_header_status status = reader->ops->get_header(reader, &header);

			switch (status) {
				case st_format_reader_header_bad_header:
					st_job_add_record(self->connect, st_log_level_error, self->job, "Error while reading header");
					nb_errors++;
					error = true;
					break;

				case st_format_reader_header_not_found:
					st_job_add_record(self->connect, st_log_level_error, self->job, "No header found but we expected one");
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
				file_checksums = self->connect->ops->get_checksums_of_file(self->connect, file, &nb_checksum);

				file_check = st_checksum_writer_new(NULL, file_checksums, nb_checksum, true);
			}

			char buffer[4096];
			ssize_t nb_read, total_read_file = 0;
			while (nb_read = reader->ops->read(reader, buffer, 4096), nb_read > 0) {
				file_check->ops->write(file_check, buffer, nb_read);
				total_read_file += nb_read;

				total_read_vol = reader->ops->position(reader);

				float done = total_read_vol + total_read;
				done *= 0.97;
				done /= total_size;
				self->job->done = 0.02 + done;
			}

			if (total_read_file == header.size) {
				file_check->ops->close(file_check);

				char ** results = st_checksum_writer_get_checksums(file_check);

				bool ok = self->connect->ops->check_checksums_of_file(self->connect, file, file_checksums, results, nb_checksum);

				file_check->ops->free(file_check);
				file_check = NULL;

				unsigned int k;
				for (k = 0; k < nb_checksum; k++) {
					free(file_checksums[i]);
					free(results[i]);
				}
				free(file_checksums);
				free(results);

				file_checksums = NULL;
				nb_checksum = 0;

				if (ok) {
					self->connect->ops->mark_archive_file_as_checked(self->connect, vol, file);
					st_job_add_record(self->connect, st_log_level_info, self->job, "Checking file (%s), status: OK", file->name);
				} else
					st_job_add_record(self->connect, st_log_level_error, self->job, "Checking file (%s), status: checksum mismatch", file->name);
			}

			st_format_file_free(&header);
		}

		total_read += total_read_vol;

		reader->ops->read_to_end_of_data(reader);

		reader->ops->close(reader);

		if (!error) {
			char ** results = st_checksum_reader_get_checksums(self->checksum_reader);

			bool ok = self->connect->ops->check_checksums_of_archive_volume(self->connect, vol, self->vol_checksums, results, self->nb_vol_checksums);
			if (ok) {
				self->connect->ops->mark_archive_volume_as_checked(self->connect, vol);
				st_job_add_record(self->connect, st_log_level_info, self->job, "Checking volume #%lu, status: OK", vol->sequence);
			} else
				st_job_add_record(self->connect, st_log_level_error, self->job, "Checking volume #%lu, status: checksum mismatch", vol->sequence);

			unsigned int j;
			for (j = 0; j < self->nb_vol_checksums; j++)
				free(results[j]);
			free(results);
		}

		reader->ops->free(reader);

end_of_work:
		drive->lock->ops->unlock(drive->lock);
	}

	if (!error) {
		self->job->done = 0.99;

		self->connect->ops->mark_archive_as_checked(self->connect, archive);
	}

	st_archive_free(archive);

	return 0;
}

static struct st_stream_reader * st_job_check_archive_thorough_mode_add_filter(struct st_stream_reader * reader, void * param) {
	struct st_job_check_archive_private * self = param;

	if (self->nb_vol_checksums > 0)
		return self->checksum_reader = st_checksum_reader_new(reader, self->vol_checksums, self->nb_vol_checksums, true);

	return reader;
}

