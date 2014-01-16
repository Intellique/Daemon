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
*  Last modified: Thu, 16 Jan 2014 16:33:30 +0100                            *
\****************************************************************************/

#define _GNU_SOURCE
// mknod, open
#include <fcntl.h>
// asprintf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// S_*, mknod, open
#include <sys/stat.h>
// S_*, lseek, open, mknod
#include <sys/types.h>
// statfs
#include <sys/statvfs.h>
// time
#include <time.h>
// chmod, chown, fchmod, fchown, lseek, mknod, write
#include <unistd.h>

#include <libstone/database.h>
#include <libstone/format.h>
#include <libstone/io.h>
#include <libstone/library/archive.h>
#include <libstone/library/changer.h>
#include <libstone/library/drive.h>
#include <libstone/library/ressource.h>
#include <libstone/library/slot.h>
#include <libstone/log.h>
#include <libstone/user.h>
#include <libstone/util/file.h>

#include "copy_archive.h"

int st_job_copy_archive_indirect_copy(struct st_job_copy_archive_private * self) {
	struct statvfs st;
	int failed = statvfs(self->job->user->home_directory, &st);
	if (failed) {
		self->job->sched_status = st_job_status_error;
		return failed;
	}

	if ((ssize_t) (st.f_bsize * st.f_favail * 0.95) < self->archive_size) {
		self->job->sched_status = st_job_status_error;

		char bufSize[16];
		st_util_file_convert_size_to_string(self->archive_size, bufSize, 16);

		st_job_add_record(self->connect, st_log_level_error, self->job, st_job_record_notif_important, "Error, indirect copy require at least %s", bufSize);
		self->job->sched_status = st_job_status_error;
		return 2;
	}

	char * temp_filename;
	asprintf(&temp_filename, "%s/copy-archive_XXXXXX.tar", self->job->user->home_directory);
	struct st_stream_writer * temp_writer = st_io_temp_writer(temp_filename, 4);
	struct st_format_writer * writer = st_format_get_writer(temp_writer, self->pool->format);

	unsigned int i;
	ssize_t total_done = 0;
	for (i = 0; i < self->archive->nb_volumes; i++) {
		struct st_archive_volume * vol = self->archive->volumes + i;

		if (self->drive_input->slot->media != NULL && self->drive_input->slot != self->slot_input) {
			struct st_changer * changer = self->drive_input->changer;
			changer->ops->unload(changer, self->drive_input);
		}

		if (self->drive_input->slot->media == NULL) {
			struct st_changer * changer = self->drive_input->changer;
			changer->ops->load_slot(changer, self->slot_input, self->drive_input);

			if (self->slot_input != NULL)
				self->slot_input->lock->ops->unlock(self->slot_input->lock);
			self->slot_input = NULL;
		}

		struct st_format_reader * reader = self->drive_input->ops->get_reader(self->drive_input, vol->media_position, NULL, NULL);

		unsigned int j;
		for (j = 0; j < vol->nb_files; j++) {
			struct st_format_file header;
			enum st_format_reader_header_status sr = reader->ops->get_header(reader, &header);
			switch (sr) {
				case st_format_reader_header_ok:
					writer->ops->add(writer, &header);

					if (S_ISREG(header.mode)) {
						char buffer[4096];

						ssize_t nb_read;
						while (nb_read = reader->ops->read(reader, buffer, 4096), nb_read > 0) {
							writer->ops->write(writer, buffer, nb_read);

							total_done = reader->ops->position(reader);
							float done = self->total_done + total_done;
							done /= self->archive_size << 1;

							self->job->done = done;
						}

						writer->ops->end_of_file(writer);
					}
					break;

				default:
					break;
			}

			st_format_file_free(&header);
		}

		reader->ops->close(reader);
		reader->ops->free(reader);
	}

	writer->ops->close(writer);
	writer->ops->free(writer);
	writer = NULL;

	self->drive_output = self->drive_input;
	self->drive_input = NULL;

	self->total_done = 0;

	struct st_stream_reader * temp_reader = st_io_file_reader(temp_filename);
	struct st_format_reader * reader = st_format_get_reader(temp_reader, self->pool->format);

	st_job_copy_archive_select_output_media(self);

	self->writer = self->drive_output->ops->get_writer(self->drive_output, true, st_job_copy_archive_add_filter, self);

	int position = self->drive_output->ops->get_position(self->drive_output);
	st_archive_add_volume(self->copy, self->drive_output->slot->media, position);

	self->current_volume = self->copy->volumes;
	self->current_volume->files = calloc(self->nb_remain_files, sizeof(struct st_archive_files));

	for (i = 0; i < self->archive->nb_volumes; i++) {
		struct st_archive_volume * vol = self->archive->volumes + i;

		unsigned int j;
		for (j = 0; j < vol->nb_files; j++) {
			struct st_archive_files * f = vol->files + j;
			struct st_archive_file * file = f->file;

			ssize_t position = self->writer->ops->position(self->writer) / self->writer->ops->get_block_size(self->writer);

			struct st_archive_files * copy_f = self->current_volume->files + self->current_volume->nb_files;
			self->current_volume->nb_files++;
			file->archived_time = time(NULL);
			copy_f->file = file;
			copy_f->position = position;

			struct st_format_file header;
			enum st_format_reader_header_status sr = reader->ops->get_header(reader, &header);
			enum st_format_writer_status sw;
			switch (sr) {
				case st_format_reader_header_ok:
					sw = self->writer->ops->add(self->writer, &header);
					if (sw == st_format_writer_end_of_volume) {
						st_job_copy_archive_change_ouput_media(self);

						copy_f = self->current_volume->files + self->current_volume->nb_files;
						self->current_volume->nb_files++;
						copy_f->file = file;
						copy_f->position = 0;
					}

					if (S_ISREG(header.mode)) {
						char buffer[4096];

						ssize_t nb_read;
						while (nb_read = reader->ops->read(reader, buffer, 4096), nb_read > 0) {
							ssize_t available = self->writer->ops->get_available_size(self->writer);
							if (available == 0) {
								st_job_copy_archive_change_ouput_media(self);

								copy_f = self->current_volume->files + self->current_volume->nb_files;
								self->current_volume->nb_files++;
								copy_f->file = file;
								copy_f->position = 0;
							}

							self->writer->ops->write(self->writer, buffer, nb_read);

							total_done = reader->ops->position(reader);
							float done = self->total_done + total_done;
							done /= self->archive_size << 1;
							done += 0.5;

							self->job->done = done;
						}

						self->writer->ops->end_of_file(self->writer);
					}
					break;

				default:
					break;
			}

			st_format_file_free(&header);

			self->nb_remain_files--;
		}

		reader->ops->close(reader);
		reader->ops->free(reader);
	}
	self->writer->ops->close(self->writer);

	self->current_volume->end_time = self->copy->end_time = time(NULL);
	self->current_volume->size = self->writer->ops->position(self->writer);

	self->current_volume->digests = NULL;
	if (self->checksum_writer != NULL)
		self->current_volume->digests = st_checksum_writer_get_checksums(self->checksum_writer);

	self->writer->ops->free(self->writer);
	self->checksum_writer = NULL;
	self->writer = NULL;

	self->job->done = 0.98;
	st_job_add_record(self->connect, st_log_level_info, self->job, st_job_record_notif_normal, "Synchronize data with database");

	// sync with database
	self->connect->ops->sync_archive(self->connect, self->copy);

	st_job_add_record(self->connect, st_log_level_info, self->job, st_job_record_notif_normal, "Write metadatas on media");
	self->job->done = 0.99;

	// write metadatas
	struct st_stream_writer * meta_writer = self->drive_output->ops->get_raw_writer(self->drive_output, true);
	st_io_json_writer(meta_writer, self->copy);
	meta_writer->ops->close(meta_writer);
	meta_writer->ops->free(meta_writer);

	st_util_file_rm(temp_filename);
	free(temp_filename);

	return 0;
}

