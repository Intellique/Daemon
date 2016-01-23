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

#define _GNU_SOURCE
// dgettext
#include <libintl.h>
// asprintf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// strcmp
#include <string.h>
// bzero
#include <strings.h>
// S_*
#include <sys/stat.h>
// time
#include <time.h>
// sleep
#include <unistd.h>

#include <libstoriqone/archive.h>
#include <libstoriqone/database.h>
#include <libstoriqone/io.h>
#include <libstoriqone/log.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/value.h>
#include <libstoriqone/thread_pool.h>
#include <libstoriqone-job/changer.h>
#include <libstoriqone-job/drive.h>
#include <libstoriqone-job/job.h>
#include <libstoriqone-job/media.h>

#include "common.h"

struct soj_checkarchive_worker {
	unsigned int i_worker;
	struct so_job * job;
	struct so_database_connection * db_connect;

	struct so_archive * archive;
	struct so_media * media;

	volatile ssize_t nb_total_read;
	volatile enum so_job_status status;
	volatile bool stop_request;

	unsigned int nb_errors;
	unsigned int nb_warnings;

	struct soj_checkarchive_worker * next;
};

static void soj_checkarchive_quick_mode_do(void * arg);


int soj_checkarchive_quick_mode(struct so_job * job, struct so_archive * archive, struct so_database_connection * db_connect) {
	soj_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_important,
		dgettext("storiqone-job-check-archive", "Starting check archive (%s) in quick mode"),
		archive->name);
	job->done = 0.01;

	struct soj_checkarchive_worker * workers = NULL;

	unsigned int i;
	for (i = 0; i < archive->nb_volumes; i++) {
		struct so_archive_volume * vol = archive->volumes + i;

		bool found = false;
		struct soj_checkarchive_worker * ptr_worker = workers;
		if (ptr_worker != NULL) {
			if (strcmp(vol->media->medium_serial_number, ptr_worker->media->medium_serial_number) == 0)
				found = true;

			for (; !found && ptr_worker->next != NULL; ptr_worker = ptr_worker->next)
				if (strcmp(vol->media->medium_serial_number, ptr_worker->next->media->medium_serial_number) == 0)
					found = true;
		}

		if (found)
			continue;

		struct soj_checkarchive_worker * new_worker = malloc(sizeof(struct soj_checkarchive_worker));
		bzero(new_worker, sizeof(struct soj_checkarchive_worker));
		new_worker->i_worker = i;
		new_worker->archive = archive;
		new_worker->media = vol->media;
		new_worker->db_connect = db_connect->config->ops->connect(db_connect->config);
		new_worker->nb_total_read = 0;
		new_worker->status = so_job_status_running;
		new_worker->stop_request = false;
		new_worker->nb_errors = new_worker->nb_warnings = 0;
		new_worker->next = NULL;

		if (ptr_worker != NULL)
			ptr_worker->next = new_worker;
		else
			workers = new_worker;
	}

	struct soj_checkarchive_worker * ptr_worker = workers;
	for (ptr_worker = workers, i = 0; ptr_worker != NULL; ptr_worker = ptr_worker->next, i++) {
		char * name = NULL;
		int size = asprintf(&name, "worker #%u", i);

		if (size < 0) {
			so_log_write(so_log_level_error,
				dgettext("storiqone-job-check-archive", "Failed to set worker thread name"));

			free(name);
			name = "worker";
		}

		soj_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_normal,
			dgettext("storiqone-job-check-archive", "Starting worker #%u"),
			i);

		so_thread_pool_run(name, soj_checkarchive_quick_mode_do, ptr_worker);

		if (size >= 0)
			free(name);
	}

	bool running = true;
	while (running) {
		sleep(5);

		ssize_t nb_total_read = 0;
		unsigned int nb_paused_workers = 0;

		for (ptr_worker = workers, running = false, i = 0; ptr_worker != NULL; ptr_worker = ptr_worker->next, i++) {
			if (ptr_worker->status == so_job_status_running)
				running = true;
			else if (ptr_worker->status == so_job_status_pause)
				nb_paused_workers++;

			nb_total_read += ptr_worker->nb_total_read;
		}

		job->status = i == nb_paused_workers ? so_job_status_pause : so_job_status_running;

		float done = nb_total_read;
		done /= archive->size;
		job->done = 0.01 + done * 0.98;
	}

	job->done = 0.99;

	for (i = 0; i < archive->nb_volumes; i++)
		db_connect->ops->check_archive_volume(db_connect, archive->volumes + i);

	unsigned int nb_errors = 0;
	ptr_worker = workers;
	for (i = 0; ptr_worker != NULL; i++) {
		struct soj_checkarchive_worker * next = ptr_worker->next;

		if (ptr_worker->nb_errors > 0)
			nb_errors++;

		soj_job_add_record(job, db_connect, so_log_level_notice, so_job_record_notif_important,
			dgettext("storiqone-job-check-archive", "Worker #%u has finished with %s and %s"), i,
			dngettext("storiqone-job-check-archive", "%u warning", "%u warnings", ptr_worker->nb_warnings),
			dngettext("storiqone-job-check-archive", "%u error", "%u errors", ptr_worker->nb_errors)
		);

		free(ptr_worker);
		ptr_worker = next;
	}

	job->done = 1;

	if (nb_errors > 0)
		job->status = so_job_status_error;

	return 0;
}

static void soj_checkarchive_quick_mode_do(void * arg) {
	struct soj_checkarchive_worker * worker = arg;

	unsigned int i;
	for (i = 0; i < worker->archive->nb_volumes && !worker->stop_request; i++) {
		struct so_archive_volume * vol = worker->archive->volumes + i;

		if (strcmp(vol->media->medium_serial_number, worker->media->medium_serial_number) != 0)
			continue;

		struct so_drive * drive = soj_media_find_and_load(vol->media, false, 0, worker->db_connect);
		if (drive == NULL) {
			// TODO: print error
			break;
		}

		struct so_value * checksums = so_value_hashtable_keys(vol->digests);
		if (so_value_list_get_length(checksums) == 0) {
			so_value_free(checksums);
			continue;
		}

		struct so_stream_reader * reader = drive->ops->get_raw_reader(drive, vol->media_position);
		reader = so_io_checksum_reader_new(reader, checksums, true);
		so_value_free(checksums);

		char buffer[16384];
		ssize_t nb_read;
		while (nb_read = reader->ops->read(reader, buffer, 16384), nb_read > 0 && !worker->stop_request)
			worker->nb_total_read += nb_read;

		reader->ops->close(reader);

		if (nb_read < 0) {
			soj_job_add_record(worker->job, worker->db_connect, so_log_level_info, so_job_record_notif_important,
				dgettext("storiqone-job-check-archive", "Worker #%u : error while reading from media '%s'"),
				worker->i_worker, vol->media->name);

			reader->ops->free(reader);
			worker->status = so_job_status_error;
			return;
		}

		if (worker->stop_request) {
			reader->ops->free(reader);
			worker->status = so_job_status_stopped;
			return;
		}

		struct so_value * digests = so_io_checksum_reader_get_checksums(reader);
		vol->check_ok = so_value_equals(vol->digests, digests);
		vol->check_time = time(NULL);
		so_value_free(digests);

		if (vol->check_ok)
			soj_job_add_record(worker->job, worker->db_connect, so_log_level_info, so_job_record_notif_important,
				dgettext("storiqone-job-check-archive", "Worker #%u : data integrity of volume '%s' is correct"),
				worker->i_worker, vol->media->name);
		else
			soj_job_add_record(worker->job, worker->db_connect, so_log_level_error, so_job_record_notif_important,
				dgettext("storiqone-job-check-archive", "Worker #%u : data integrity of volume '%s' is not correct"),
				worker->i_worker, vol->media->name);

		reader->ops->free(reader);
	}

	worker->status = worker->stop_request ? so_job_status_stopped : so_job_status_finished;
}

