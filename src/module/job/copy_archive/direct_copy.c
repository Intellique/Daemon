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
*  Copyright (C) 2014, Clercin guillaume <gclercin@intellique.com>           *
*  Last modified: Tue, 21 Jan 2014 17:53:37 +0100                            *
\****************************************************************************/

// calloc
#include <stdlib.h>
// S_ISREG
#include <sys/stat.h>
// S_ISREG
#include <sys/types.h>
// time
#include <time.h>
// S_ISREG
#include <unistd.h>

#include <libstone/database.h>
#include <libstone/format.h>
#include <libstone/io.h>
#include <libstone/log.h>
#include <libstone/library/archive.h>
#include <libstone/library/changer.h>
#include <libstone/library/drive.h>
#include <libstone/library/ressource.h>
#include <libstone/library/slot.h>

#include "copy_archive.h"


int st_job_copy_archive_direct_copy(struct st_job_copy_archive_private * self) {
	self->job->done = 0.02;

	self->writer = self->drive_output->ops->get_writer(self->drive_output, true, st_job_copy_archive_add_filter, self);

	unsigned int i;
	int position = self->drive_output->ops->get_position(self->drive_output);
	st_archive_add_volume(self->copy, self->drive_output->slot->media, position, self->job);

	self->current_volume = self->copy->volumes;
	self->current_volume->files = calloc(self->nb_remain_files, sizeof(struct st_archive_files));

	ssize_t total_done = 0;
	for (i = 0; i < self->archive->nb_volumes; i++) {
		struct st_archive_volume * vol = self->archive->volumes + i;

		if (self->drive_input->slot != self->slot_input) {
			struct st_changer * changer = self->drive_input->changer;
			changer->ops->unload(changer, self->drive_input);
		}

		if (self->drive_input->slot->media == NULL) {
			struct st_changer * changer = self->drive_input->changer;
			changer->ops->load_slot(changer, self->slot_input, self->drive_input);

			self->slot_input->lock->ops->unlock(self->slot_input->lock);
			self->slot_input = NULL;
		}

		struct st_format_reader * reader = self->drive_input->ops->get_reader(self->drive_input, vol->media_position, NULL, NULL);

		unsigned int j;
		for (j = 0; j < vol->nb_files; j++) {
			struct st_archive_files * f = vol->files + j;
			struct st_archive_file * file = f->file;

			ssize_t position = self->writer->ops->position(self->writer) / self->writer->ops->get_block_size(self->writer);

			struct st_archive_files * copy_f = self->current_volume->files + self->current_volume->nb_files;
			self->current_volume->nb_files++;
			copy_f->file = file;
			copy_f->position = position;
			file->archived_time = time(NULL);

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
							self->writer->ops->write(self->writer, buffer, nb_read);

							total_done = reader->ops->position(reader);
							float done = self->total_done + total_done;
							done /= self->archive_size;
							done *= 0.96;

							self->job->done = done + 0.02;
						}

						self->writer->ops->end_of_file(self->writer);
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
	struct st_stream_writer * writer = self->drive_output->ops->get_raw_writer(self->drive_output, true);
	st_io_json_writer(writer, self->copy);
	writer->ops->close(writer);
	writer->ops->free(writer);

	return 0;
}

