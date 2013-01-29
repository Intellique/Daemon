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
*  Last modified: Tue, 01 Jan 2013 18:07:24 +0100                         *
\*************************************************************************/

// S_ISREG
#include <sys/stat.h>
// S_ISREG
#include <sys/types.h>
// S_ISREG
#include <unistd.h>

#include <libstone/format.h>
#include <libstone/library/archive.h>
#include <libstone/library/changer.h>
#include <libstone/library/drive.h>
#include <libstone/library/ressource.h>
#include <libstone/library/slot.h>

#include "copy_archive.h"

int st_job_copy_archive_direct_copy(struct st_job_copy_archive_private * self) {
	if (self->drive_output->slot != self->slot_output) {
		struct st_changer * changer = self->drive_output->changer;
		changer->ops->unload(changer, self->drive_output);
	}

	if (self->drive_output->slot->media != NULL) {
		struct st_changer * changer = self->drive_output->changer;
		changer->ops->load_slot(changer, self->slot_output, self->drive_output);

		self->slot_output->lock->ops->unlock(self->slot_output->lock);
		self->slot_output = NULL;
	}

	struct st_format_writer * writer = self->drive_output->ops->get_writer(self->drive_output, true, NULL, NULL);

	unsigned int i;
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

		struct st_format_reader * reader = self->drive_input->ops->get_reader(self->drive_input, vol->media_position);

		struct st_format_file header;
		enum st_format_reader_header_status sr = reader->ops->get_header(reader, &header);
		enum st_format_writer_status sw;
		switch (sr) {
			case st_format_reader_header_ok:
				sw = writer->ops->add(writer, &header);
				if (sw == st_format_writer_end_of_volume) {
					st_job_copy_archive_select_output_media(self, true);

					struct st_stream_writer * stw = self->drive_output->ops->get_raw_writer(self->drive_output, true);
					writer->ops->new_volume(writer, stw);
				}

				if (S_ISREG(header.mode)) {
					char buffer[4096];

					ssize_t nb_read;
					while (nb_read = reader->ops->read(reader, buffer, 4096), nb_read > 0) {
						writer->ops->write(writer, buffer, nb_read);
					}

					writer->ops->end_of_file(writer);
				}
				break;

			default:
				break;
		}
	}

	return 0;
}

