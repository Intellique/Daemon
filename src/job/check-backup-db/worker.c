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
// free, malloc
#include <stdlib.h>
// bzero
#include <strings.h>
// time
#include <time.h>
// sleep
#include <unistd.h>

#include <libstoriqone/backup.h>
#include <libstoriqone/database.h>
#include <libstoriqone/io.h>
#include <libstoriqone/log.h>
#include <libstoriqone/media.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/value.h>
#include <libstoriqone-job/changer.h>
#include <libstoriqone-job/drive.h>
#include <libstoriqone-job/job.h>

#include "common.h"


void soj_checkbackupdb_worker_do(void * arg) {
	struct soj_checkbackupdb_worker * worker = arg;
	struct so_job * job = soj_job_get();

	enum {
		check_media,
		find_media,
		get_media,
		open_file,
		reserve_media,
	} state = find_media;

	struct so_slot * slot = NULL;
	struct so_drive * dr = NULL;
	struct so_stream_reader * sr_dr = NULL;

	bool error = false, stop = false;
	bool has_alert_user = false;
	while (!stop) {
		switch (state) {
			case check_media:
				// TODO:
				state = open_file;
				break;

			case find_media:
				slot = soj_changer_find_slot(worker->volume->media);
				if (slot == NULL) {
					if (!has_alert_user)
						soj_job_add_record(job, worker->db_connect, so_log_level_info, so_job_record_notif_important,
							dgettext("storiqone-job-check-backup-db", "Warning, media '%s' not found"),
							worker->volume->media->name);
					has_alert_user = true;
					worker->status = so_job_status_pause;

					sleep(5);

					// state = look_for_media;
					worker->status = so_job_status_running;
					soj_changer_sync_all();
				} else {
					state = reserve_media;
					has_alert_user = false;
				}
				break;

			case get_media:
				dr = slot->changer->ops->get_media(slot->changer, worker->volume->media, false, &error);

				if (error) {
					stop = true;
					break;
				}

				if (dr == NULL) {
					state = find_media;
					sleep(5);
				} else
					state = check_media;

				break;

			case open_file:
				sr_dr = dr->ops->get_raw_reader(dr, worker->volume->position);
				if (sr_dr == NULL) {
					soj_job_add_record(job, worker->db_connect, so_log_level_info, so_job_record_notif_important,
						dgettext("storiqone-job-check-backup-db", "Error, failed to open backup from media '%s'"),
						worker->volume->media->name);

					sleep(5);
				} else
					stop = true;
				break;

			case reserve_media:
				slot->changer->ops->reserve_media(slot->changer, worker->volume->media, 0, so_pool_unbreakable_level_none);
				state = get_media;
				break;
		}
	}

	if (error) {
		worker->status = so_job_status_error;
		worker->working = false;
		return;
	}

	struct so_value * checksums = so_value_hashtable_keys(worker->volume->digests);
	struct so_stream_reader * ck_dr = so_io_checksum_reader_new(sr_dr, checksums, true);

	char * buffer = malloc(65536);
	ssize_t nb_read;
	do {
		nb_read = ck_dr->ops->read(ck_dr, buffer, 65536);
		if (nb_read > 0)
			worker->position += nb_read;
	} while (nb_read > 0 && !job->stopped_by_user);

	free(buffer);
	ck_dr->ops->close(ck_dr);

	struct so_value * digests = so_io_checksum_reader_get_checksums(ck_dr);
	worker->volume->checksum_ok = so_value_equals(worker->volume->digests, digests);
	worker->volume->checktime = time(NULL);

	so_value_free(digests);
	ck_dr->ops->free(ck_dr);
	so_value_free(checksums);

	slot->changer->ops->release_media(slot->changer, worker->volume->media);

	worker->working = false;

	worker->db_connect->ops->free(worker->db_connect);
	worker->db_connect = NULL;
}

void soj_checkbackupdb_worker_free(struct soj_checkbackupdb_worker * worker) {
	while (worker != NULL) {
		struct soj_checkbackupdb_worker * ptr = worker;
		worker = ptr->next;
		free(worker);
	}
}

struct soj_checkbackupdb_worker * soj_checkbackupdb_worker_new(struct so_backup * backup, struct so_backup_volume * volume, size_t size, struct so_database_config * db_config, struct soj_checkbackupdb_worker * previous_worker) {
	struct soj_checkbackupdb_worker * worker = malloc(sizeof(struct soj_checkbackupdb_worker));
	bzero(worker, sizeof(struct soj_checkbackupdb_worker));

	worker->backup = backup;
	worker->volume = volume;

	worker->position = 0;
	worker->size = size;

	worker->working = true;

	worker->db_connect = db_config->ops->connect(db_config);

	if (previous_worker != NULL)
		previous_worker = worker;

	return worker;
}

