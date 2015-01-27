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
*  Copyright (C) 2015, Guillaume Clercin <gclercin@intellique.com>           *
\****************************************************************************/

#include <libstoriqone/backup.h>
#include <libstoriqone/media.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/value.h>
#include <libstoriqone-job/changer.h>
#include <libstoriqone-job/drive.h>
#include <libstoriqone-job/io.h>
#include <libstoriqone-job/job.h>

// free, malloc
#include <stdlib.h>
// bzero
#include <strings.h>
// time
#include <time.h>
// sleep
#include <unistd.h>

#include "common.h"


void soj_checkbackupdb_worker_do(void * arg) {
	struct soj_checkbackupdb_worker * worker = arg;
	worker->working = true;

	struct so_job * job = soj_job_get();

	enum {
		check_media,
		find_media,
		get_media,
		open_file,
		reserve_media,
	} state = reserve_media;

	struct so_slot * slot = NULL;
	struct so_drive * dr = NULL;
	struct so_stream_reader * sr_dr = NULL;

	bool stop = false;
	while (!stop) {
		switch (state) {
			case check_media:
				state = open_file;
				break;

			case find_media:
				slot = soj_changer_find_slot(worker->volume->media);
				if (slot == NULL) {
					// TODO: add message
					sleep(5);
				} else
					state = reserve_media;
				break;

			case get_media:
				dr = slot->changer->ops->get_media(slot->changer, slot);
				if (dr == NULL) {
					state = find_media;
					sleep(5);
				} else
					state = check_media;

				break;

			case open_file:
				sr_dr = dr->ops->get_raw_reader(dr, worker->volume->position);
				if (sr_dr == NULL) {
					sleep(5);
					// TODO:
				} else
					stop = true;
				break;

			case reserve_media:
				slot->changer->ops->reserve_media(slot->changer, slot, 0, so_pool_unbreakable_level_none);
				break;
		}
	}

	struct so_value * checksums = so_value_hashtable_keys(worker->volume->digests);
	struct so_stream_reader * ck_dr = soj_checksum_reader_new(sr_dr, checksums, true);

	char * buffer = malloc(65536);
	ssize_t nb_read;
	do {
		nb_read = ck_dr->ops->read(ck_dr, buffer, 65536);
		if (nb_read > 0)
			worker->position += nb_read;
	} while (nb_read > 0 && !job->stopped_by_user);

	free(buffer);
	ck_dr->ops->close(ck_dr);

	struct so_value * digests = soj_checksum_reader_get_checksums(ck_dr);
	worker->volume->checksum_ok = so_value_equals(worker->volume->digests, digests);
	worker->volume->checktime = time(NULL);

	so_value_free(digests);
	ck_dr->ops->free(ck_dr);
	so_value_free(checksums);

	slot->changer->ops->release_media(slot->changer, slot);

	worker->working = false;
}

void soj_checkbackupdb_worker_free(struct soj_checkbackupdb_worker * worker) {
	while (worker != NULL) {
		struct soj_checkbackupdb_worker * ptr = worker;
		worker = ptr->next;
		free(worker);
	}
}

struct soj_checkbackupdb_worker * soj_checkbackupdb_worker_new(struct so_backup * backup, struct so_backup_volume * volume, size_t size, struct soj_checkbackupdb_worker * previous_worker) {
	struct soj_checkbackupdb_worker * worker = malloc(sizeof(struct soj_checkbackupdb_worker));
	bzero(worker, sizeof(struct soj_checkbackupdb_worker));

	worker->backup = backup;
	worker->volume = volume;

	worker->position = 0;
	worker->size = size;

	worker->working = false;

	if (previous_worker != NULL)
		previous_worker = worker;

	return worker;
}

