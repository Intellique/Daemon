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
*  Last modified: Mon, 04 Feb 2013 17:46:40 +0100                            *
\****************************************************************************/

// mknod, open
#include <fcntl.h>
// free, malloc
#include <stdlib.h>
// S_*, mknod, open
#include <sys/stat.h>
// S_*, lseek, open, mknod
#include <sys/types.h>
// chmod, chown, fchmod, fchown, lseek, mknod, write
#include <unistd.h>

#include <libstone/database.h>
#include <libstone/format.h>
#include <libstone/library/archive.h>
#include <libstone/library/changer.h>
#include <libstone/library/drive.h>
#include <libstone/library/ressource.h>
#include <libstone/library/slot.h>
#include <libstone/log.h>

#include "copy_archive.h"

int st_job_copy_archive_indirect_copy(struct st_job_copy_archive_private * self) {
	unsigned int i;
	for (i = 0; i < self->archive->nb_volumes; i++) {
		struct st_archive_volume * vol = self->archive->volumes + i;

		self->connect->ops->get_archive_files_by_job_and_archive_volume(self->connect, self->job, vol);

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

			struct st_format_file header;
			enum st_format_reader_header_status status = reader->ops->get_header(reader, &header);
			if (status != st_format_reader_header_ok) {
				st_job_add_record(self->connect, st_log_level_error, self->job, "Error while reading header of file (%s)", file->name);

				st_archive_file_free(file);
				break;
			}

			if (status != st_format_reader_header_ok) {
				st_job_add_record(self->connect, st_log_level_error, self->job, "Error while reading header of file (%s)", file->name);

				st_archive_file_free(file);
				break;
			}

			char * restore_to = self->connect->ops->get_restore_path_from_file(self->connect, self->job, file);

			if (S_ISREG(header.mode)) {
				int fd = open(restore_to, O_CREAT | O_WRONLY, header.mode & 07777);
				if (fd < 0) {
					st_job_add_record(self->connect, st_log_level_error, self->job, "Error while opening file (%s) for writing because %m", restore_to);

					st_archive_file_free(file);
					break;
				}

				if (header.position > 0) {
					if (lseek(fd, header.position, SEEK_SET) != header.position) {
						st_job_add_record(self->connect, st_log_level_error, self->job, "Error while seeking into file (%s) because %m", restore_to);
						close(fd);

						st_archive_file_free(file);
						break;
					}
				}

				ssize_t nb_read, nb_write = 0;
				char buffer[4096];
				while (nb_read = reader->ops->read(reader, buffer, 4096), nb_read > 0) {
					nb_write = write(fd, buffer, nb_read);

					if (nb_write > 0) {
						// self->total_restored += nb_write;
					} else {
						st_job_add_record(self->connect, st_log_level_error, self->job, "Error while writing to file (%s) because %m", restore_to);

						st_archive_file_free(file);
						break;
					}
				}

				if (nb_read < 0) {
					st_job_add_record(self->connect, st_log_level_error, self->job, "Error while reading from media (%s) because %m", vol->media->name);

					st_archive_file_free(file);
					break;
				} else if (nb_write >= 0) {
					if (fchown(fd, file->ownerid, file->groupid)) {
						st_job_add_record(self->connect, st_log_level_warning, self->job, "Error while restoring user of file (%s) because %m", restore_to);
					}

					if (fchmod(fd, file->perm)) {
						st_job_add_record(self->connect, st_log_level_warning, self->job, "Error while restoring permission of file (%s) because %m", restore_to);
					}

					struct timeval tv[] = {
						{ file->modify_time, 0 },
						{ file->modify_time, 0 },
					};
					if (futimes(fd, tv)) {
						st_job_add_record(self->connect, st_log_level_warning, self->job, "Error while motification time of file (%s) because %m", restore_to);
					}
				}

				close(fd);
			} else if (S_ISDIR(header.mode)) {
				// do nothing because directory is already created
			} else if (S_ISLNK(header.mode)) {
				symlink(header.link, header.filename);
			} else if (S_ISFIFO(header.mode)) {
				mknod(file->name, S_IFIFO, 0);
			} else if (S_ISCHR(header.mode)) {
				mknod(file->name, S_IFCHR, header.dev);
			} else if (S_ISBLK(header.mode)) {
				mknod(file->name, S_IFBLK, header.dev);
			}

			if (!(S_ISREG(header.mode) || S_ISDIR(header.mode))) {
				if (chown(header.filename, file->ownerid, file->groupid)) {
					st_job_add_record(self->connect, st_log_level_warning, self->job, "Error while restoring user of file (%s) because %m", header.filename);
				}

				if (chmod(header.filename, file->perm)) {
					st_job_add_record(self->connect, st_log_level_warning, self->job, "Error while restoring permission of file (%s) because %m", header.filename);
				}

				struct timeval tv[] = {
					{ file->modify_time, 0 },
					{ file->modify_time, 0 },
				};
				if (utimes(header.filename, tv)) {
					st_job_add_record(self->connect, st_log_level_warning, self->job, "Error while motification time of file (%s) because %m", header.filename);
				}
			}

			free(restore_to);
			st_format_file_free(&header);
		}

		reader->ops->close(reader);
		reader->ops->free(reader);
	}

	bool ok = st_job_copy_archive_select_output_media(self, true);

	if (ok) {
		struct st_format_writer * writer = self->drive_output->ops->get_writer(self->drive_output, true, NULL, NULL);
	}

	return 0;
}

