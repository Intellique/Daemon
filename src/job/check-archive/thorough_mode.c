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
// S_*
#include <sys/stat.h>
// time
#include <time.h>
// sleep
#include <unistd.h>

#include <libstoriqone/archive.h>
#include <libstoriqone/database.h>
#include <libstoriqone/format.h>
#include <libstoriqone/log.h>
#include <libstoriqone/media.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/value.h>
#include <libstoriqone-job/changer.h>
#include <libstoriqone-job/drive.h>
#include <libstoriqone-job/job.h>

#include "common.h"

int soj_checkarchive_thorough_mode(struct so_job * job, struct so_archive * archive, struct so_database_connection * db_connect) {
	unsigned i;
	bool ok = true;
	for (i = 0; ok && i < archive->nb_volumes; i++) {
		struct so_archive_volume * vol = archive->volumes + i;

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

		while (!stop && !job->stopped_by_user) {
			switch (state) {
				case alert_user:
					job->status = so_job_status_waiting;
					if (!has_alert_user)
						so_job_add_record(job, db_connect, so_log_level_warning, so_job_record_notif_important, dgettext("storiqone-job-check-archive", "Media not found (named: %s)"), vol->media->name);
					has_alert_user = true;

					sleep(15);

					state = look_for_media;
					job->status = so_job_status_running;
					soj_changer_sync_all();
					break;

				case get_media:
					drive = slot->changer->ops->get_media(slot->changer, slot, false);
					if (drive == NULL) {
						job->status = so_job_status_waiting;

						sleep(15);

						state = look_for_media;
						job->status = so_job_status_running;
						soj_changer_sync_all();
					} else
						stop = true;
					break;

				case look_for_media:
					slot = soj_changer_find_slot(vol->media);
					state = slot != NULL ? reserve_media : alert_user;
					break;

				case reserve_media:
					reserved_size = slot->changer->ops->reserve_media(slot->changer, slot, 0, so_pool_unbreakable_level_none);
					if (reserved_size < 0) {
						job->status = so_job_status_waiting;

						sleep(15);

						state = look_for_media;
						job->status = so_job_status_running;
						soj_changer_sync_all();
					} else
						state = get_media;
					break;
			}
		}

		job->status = so_job_status_running;

		drive->ops->sync(drive);

		struct so_value * checksums = so_value_hashtable_keys(vol->digests);
		struct so_format_reader * reader = drive->ops->get_reader(drive, vol->media_position, checksums);
		so_value_free(checksums);

		enum so_format_reader_header_status status;
		struct so_format_file file;
		while (status = reader->ops->get_header(reader, &file), status == so_format_reader_header_ok) {
			if (S_ISREG(file.mode)) {
				static char buffer[16384];
				ssize_t nb_read;
				while (nb_read = reader->ops->read(reader, buffer, 16384), nb_read > 0) {
				}

				if (nb_read < 0) {
					ok = false;
					// TODO: error while reading
				}
			}

			so_format_file_free(&file);

			if (!ok)
				break;
		}

		if (ok)
			reader->ops->read_to_end_of_data(reader);

		reader->ops->close(reader);
		if (ok) {
			struct so_value * digests = reader->ops->get_digests(reader);
			ok = so_value_equals(vol->digests, digests);
		}

		vol->check_ok = ok;
		vol->check_time = time(NULL);
	}

	db_connect->ops->sync_archive(db_connect, archive);

	return ok ? 0 : 1;
}

