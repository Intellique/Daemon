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

#define _GNU_SOURCE
// open, mknod
#include <fcntl.h>
// dgettext
#include <libintl.h>
// free, malloc
#include <stdlib.h>
// asprintf
#include <stdio.h>
// strcmp
#include <string.h>
// bzero
#include <strings.h>
// S_*, mknod, mkstat, open
#include <sys/stat.h>
// S_*, lseek, mknod, open
#include <sys/types.h>
// chmod, chown, close, fchmod, futimes, lseek, mknod, sleep, symlink
#include <unistd.h>

#include <libstoriqone/archive.h>
#include <libstoriqone/database.h>
#include <libstoriqone/file.h>
#include <libstoriqone/format.h>
#include <libstoriqone/log.h>
#include <libstoriqone/media.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/string.h>
#include <libstoriqone/thread_pool.h>
#include <libstoriqone-job/changer.h>
#include <libstoriqone-job/drive.h>
#include <libstoriqone-job/job.h>

#include "common.h"

static void so_restorearchive_data_worker_do(void * arg);


struct soj_restorearchive_data_worker * soj_restorearchive_data_worker_new(struct so_archive * archive, struct so_media * media, struct so_database_config * db_config, struct soj_restorearchive_data_worker * previous_worker) {
	struct soj_restorearchive_data_worker * worker = malloc(sizeof(struct soj_restorearchive_data_worker));
	bzero(worker, sizeof(struct soj_restorearchive_data_worker));

	worker->archive = archive;
	worker->media = media;
	worker->db_config = db_config;
	worker->status = so_job_status_running;

	if (previous_worker != NULL)
		previous_worker->next = worker;

	return worker;
}

static void soj_restorearchive_data_worker_do(void * arg) {
	struct soj_restorearchive_data_worker * worker = arg;

	struct so_job * job = soj_job_get();
	struct so_database_connection * db_connect = worker->db_config->ops->connect(worker->db_config);

	enum {
		alert_user,
		get_media,
		look_for_media,
		reserve_media,
	} state = look_for_media;
	bool stop = false, has_alert_user = false;
	struct so_slot * slot = NULL;
	ssize_t reserved_size = 0;
	struct so_drive * drive = NULL;

	while (!stop && !worker->stop_request) {
		switch (state) {
			case alert_user:
				worker->status = so_job_status_waiting;
				if (!has_alert_user)
					so_job_add_record(job, db_connect, so_log_level_warning, so_job_record_notif_important, dgettext("storiqone-job-restore-archive", "Media not found (named: %s)"), worker->media->name);
				has_alert_user = true;

				sleep(15);

				state = look_for_media;
				worker->status = so_job_status_running;
				soj_changer_sync_all();
				break;

			case get_media:
				so_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_important, dgettext("storiqone-job-restore-archive", "Getting media (%s)"), worker->media->name);
				drive = slot->changer->ops->get_media(slot->changer, slot, false);
				if (drive == NULL) {
					worker->status = so_job_status_waiting;

					sleep(15);

					state = look_for_media;
					worker->status = so_job_status_running;
					soj_changer_sync_all();
				} else
					stop = true;
				break;

			case look_for_media:
				so_job_add_record(job, db_connect, so_log_level_debug, so_job_record_notif_important, dgettext("storiqone-job-restore-archive", "Looking for media (%s)"), worker->media->name);
				slot = soj_changer_find_slot(worker->media);
				state = slot != NULL ? reserve_media : alert_user;
				break;

			case reserve_media:
				reserved_size = slot->changer->ops->reserve_media(slot->changer, slot, 0, so_pool_unbreakable_level_none);
				if (reserved_size < 0) {
					worker->status = so_job_status_waiting;

					sleep(15);

					state = look_for_media;
					worker->status = so_job_status_running;
					soj_changer_sync_all();
				} else
					state = get_media;
				break;
		}
	}

	if (worker->stop_request)
		goto stop_worker;

	unsigned int i;
	for (i = 0; i < worker->archive->nb_volumes && !worker->stop_request; i++) {
		struct so_archive_volume * vol = worker->archive->volumes + i;

		if (strcmp(vol->media->medium_serial_number, worker->media->medium_serial_number) != 0)
			continue;

		struct so_format_reader * reader = drive->ops->get_reader(drive, vol->media_position, NULL);

		unsigned int j = 0;
		for (j = 0; j < vol->nb_files && !worker->stop_request; j++) {
			struct so_archive_files * ptr_file = vol->files + j;
			struct so_archive_file * file = ptr_file->file;

			struct so_format_file header;
			enum so_format_reader_header_status status = reader->ops->get_header(reader, &header);

			so_string_delete_double_char(header.filename, '/');
			so_string_trim(header.filename, '/');

			const char * restore_to = soj_restorearchive_path_get(header.filename, file->selected_path, file->type == so_archive_file_type_regular_file);
			if (restore_to != NULL)
				file->restored_to = strdup(restore_to);

			char * ptr = strrchr(restore_to, '/');
			if (ptr != NULL) {
				*ptr = '\0';
				if (access(restore_to, R_OK | W_OK | X_OK) != 0) {
					so_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_normal, dgettext("storiqone-job-restore-archive", "Create missing directory '%s' with permission 0777"), restore_to);
					so_file_mkdir(restore_to, 0777);
				}
				*ptr = '/';
			}

			so_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_normal, dgettext("storiqone-job-restore-archive", "Start restoring file '%s'"), restore_to);

			if (S_ISREG(header.mode)) {
				int fd = open(restore_to, O_CREAT | O_WRONLY, header.mode & 07777);
				if (fd < 0) {
					so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important, dgettext("storiqone-job-restore-archive", "Error while opening file (%s) for writing because %m"), restore_to);
					worker->nb_errors++;

					break;
				}

				if (header.position > 0) {
					if (lseek(fd, header.position, SEEK_SET) != header.position) {
						so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important, dgettext("storiqone-job-restore-archive", "Error while seeking into file (%s) because %m"), restore_to);
						worker->nb_errors++;
						close(fd);

						break;
					}
				}

				ssize_t nb_read, nb_write = 0;
				char buffer[16384];
				while (nb_read = reader->ops->read(reader, buffer, 16384), nb_read > 0 && !worker->stop_request) {
					nb_write = write(fd, buffer, nb_read);

					if (nb_write > 0)
						worker->total_restored += nb_write;
					else {
						so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important, dgettext("storiqone-job-restore-archive", "Error while writing to file (%s) because %m"), restore_to);
						worker->nb_errors++;
						break;
					}
				}

				if (nb_read < 0) {
					so_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important, dgettext("storiqone-job-restore-archive", "Error while reading from media (%s) because %m"), vol->media->name);
					worker->nb_errors++;

					break;
				} else if (nb_write >= 0) {
					if (fchown(fd, file->ownerid, file->groupid)) {
						so_job_add_record(job, db_connect, so_log_level_warning, so_job_record_notif_important, dgettext("storiqone-job-restore-archive", "Error while restoring user of file (%s) because %m"), restore_to);
						worker->nb_warnings++;
					}

					if (fchmod(fd, file->perm)) {
						so_job_add_record(job, db_connect, so_log_level_warning, so_job_record_notif_important, dgettext("storiqone-job-restore-archive", "Error while restoring permission of file (%s) because %m"), restore_to);
						worker->nb_warnings++;
					}

					struct timeval tv[] = {
						{ file->modify_time, 0 },
						{ file->modify_time, 0 },
					};
					if (futimes(fd, tv)) {
						so_job_add_record(job, db_connect, so_log_level_warning, so_job_record_notif_important, dgettext("storiqone-job-restore-archive", "Error while motification time of file (%s) because %m"), restore_to);
						worker->nb_warnings++;
					}
				}

				close(fd);

				// st_job_restore_archive_checks_worker_add_file(self->jp->checks, file);
			} else if (S_ISDIR(header.mode)) {
				// do nothing because directory is already created
			} else if (S_ISLNK(header.mode)) {
				if (symlink(header.link, header.filename) != 0) {
					so_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_normal, dgettext("storiqone-job-restore-archive", "Failed to create symbolic link '%s' to '%s' because %m"), restore_to, header.link);
					worker->nb_errors++;
				} else if (chmod(restore_to, file->perm & 0777) != 0) {
					so_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_normal, dgettext("storiqone-job-restore-archive", "Failed to change permission of '%s' to '%03o' because %m"), restore_to, file->perm & 0777);
					worker->nb_warnings++;
				}
			} else if (S_ISFIFO(header.mode)) {
				if (mknod(file->path, S_IFIFO, 0) != 0) {
					so_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_normal, dgettext("storiqone-job-restore-archive", "Failed to create fifo '%s' because %m"), restore_to);
					worker->nb_errors++;
				}
			} else if (S_ISCHR(header.mode)) {
				if (mknod(file->path, S_IFCHR, header.dev) != 0) {
					so_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_normal, dgettext("storiqone-job-restore-archive", "Failed to create character device '%s' because %m"), restore_to);
					worker->nb_errors++;
				}
			} else if (S_ISBLK(header.mode)) {
				int failed = mknod(file->path, S_IFBLK, header.dev);
				if (failed != 0) {
					so_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_normal, dgettext("storiqone-job-restore-archive", "Failed to create block device '%s' because %m"), restore_to);
					worker->nb_errors++;
				}
			}

			if (chown(restore_to, file->ownerid, file->groupid) != 0) {
				so_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_normal, dgettext("storiqone-job-restore-archive", "Failed to change owner and group of '%s' because %m"), restore_to);
				worker->nb_warnings++;
			}
		}

		reader->ops->free(reader);
	}

stop_worker:
	worker->status = worker->stop_request ? so_job_status_stopped : so_job_status_finished;
}

void soj_restorearchive_data_worker_start(struct soj_restorearchive_data_worker * first_worker, struct so_job * job, struct so_database_connection * db_connect) {
	unsigned int i;
	for (i = 0; first_worker != NULL; i++, first_worker = first_worker->next) {
		char * name;
		asprintf(&name, "worker #%u", i);

		so_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_normal, dgettext("storiqone-job-restore-archive", "Starting worker #%u"), i);

		// so_thread_pool_run(name, soj_restorearchive_data_worker_do, first_worker);
		soj_restorearchive_data_worker_do(first_worker);
		free(name);
	}
}
