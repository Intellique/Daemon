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
*  Copyright (C) 2013-2019, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// open
#include <fcntl.h>
// dgettext
#include <libintl.h>
// pthread_mutex_lock, pthread_mutex_unlock
#include <pthread.h>
// malloc
#include <stdlib.h>
// bzero
#include <strings.h>
// open
#include <sys/stat.h>
// open
#include <sys/types.h>
// time
#include <time.h>
// read
#include <unistd.h>

#include <libstoriqone/archive.h>
#include <libstoriqone/database.h>
#include <libstoriqone/io.h>
#include <libstoriqone/log.h>
#include <libstoriqone/string.h>
#include <libstoriqone/thread_pool.h>
#include <libstoriqone/value.h>
#include <libstoriqone-job/job.h>

#include "common.h"

struct soj_restorearchive_check_worker_private {
	unsigned long long hash;

	struct so_archive * archive;
	struct so_archive_file * file;

	unsigned int nb_volumes_done;
	unsigned int nb_total_volumes;

	struct soj_restorearchive_check_worker_private * next;
};

static void soj_restorearchive_check_worker_check(struct so_job * job, struct soj_restorearchive_check_worker_private * ptr, struct so_database_connection * db_connect);
static void soj_restorearchive_check_worker_do(void * arg);

static struct so_database_config * check_worker_db_config = NULL;
static struct soj_restorearchive_check_worker_private * first_file = NULL, * last_file = NULL;
static pthread_mutex_t check_worker_lock = PTHREAD_MUTEX_INITIALIZER;
static volatile bool check_worker_running = false;
static volatile bool check_worker_stop = false;
static pthread_cond_t check_worker_wait = PTHREAD_COND_INITIALIZER;


void soj_restorearchive_check_worker_add(struct so_archive * archive, struct so_archive_file * file) {
	const unsigned long long hash = so_string_compute_hash2(file->path);

	pthread_mutex_lock(&check_worker_lock);

	struct soj_restorearchive_check_worker_private * ptr;
	for (ptr = first_file; ptr != NULL; ptr = ptr->next)
		if (hash == ptr->hash)
			break;

	if (ptr == NULL) {
		ptr = malloc(sizeof(struct soj_restorearchive_check_worker_private));
		bzero(ptr, sizeof(struct soj_restorearchive_check_worker_private));

		ptr->hash = hash;
		ptr->archive = archive;
		ptr->file = file;
		ptr->nb_volumes_done = 1;
		ptr->nb_total_volumes = 0;
		ptr->next = NULL;

		if (last_file == NULL)
			first_file = last_file = ptr;
		else
			last_file = last_file->next = ptr;

		pthread_cond_signal(&check_worker_wait);
	} else
		ptr->nb_volumes_done++;

	pthread_mutex_unlock(&check_worker_lock);
}

static void soj_restorearchive_check_worker_check(struct so_job * job, struct soj_restorearchive_check_worker_private * ptr, struct so_database_connection * db_connect) {
	struct so_value * checksums = so_value_hashtable_keys(ptr->file->digests);
	if (so_value_list_get_length(checksums) == 0) {
		so_value_free(checksums);

		soj_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_normal, dgettext("storiqone-job-restore-archive", "Skipping verifying '%s' because there are no checksums availables"), ptr->file->restored_to);
		return;
	}

	char * filename = so_string_unescape(ptr->file->restored_to);

	int fd_in = open(filename, O_RDONLY);
	if (fd_in < 0) {
		so_value_free(checksums);

		soj_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_normal,
			dgettext("storiqone-job-restore-archive", "Failed to open '%s' because %m"),
			ptr->file->restored_to);

		return;
	}

	free(filename);

	struct so_stream_writer * checksum_writer = so_io_checksum_writer_new(NULL, checksums, false);

	static char buffer[65536];
	ssize_t nb_read;
	while (nb_read = read(fd_in, buffer, 65536), nb_read > 0)
		checksum_writer->ops->write(checksum_writer, buffer, nb_read);

	close(fd_in);

	checksum_writer->ops->close(checksum_writer);

	struct so_value * digests = so_io_checksum_writer_get_checksums(checksum_writer);
	ptr->file->check_ok = so_value_equals(ptr->file->digests, digests);
	ptr->file->check_time = time(NULL);
	so_value_free(digests);

	checksum_writer->ops->free(checksum_writer);

	db_connect->ops->check_archive_file(db_connect, ptr->archive, ptr->file);

	if (ptr->file->check_ok)
		soj_job_add_record(job, db_connect, so_log_level_notice, so_job_record_notif_normal,
			dgettext("storiqone-job-restore-archive", "data integrity of file '%s' is correct"),
			ptr->file->restored_to);
	else
		soj_job_add_record(job, db_connect, so_log_level_warning, so_job_record_notif_important,
			dgettext("storiqone-job-restore-archive", "data integrity of file '%s' is not correct"),
			ptr->file->restored_to);
}

static void soj_restorearchive_check_worker_do(void * arg __attribute__((unused))) {
	check_worker_running = true;

	struct so_job * job = soj_job_get();
	struct so_database_connection * db_connect = check_worker_db_config->ops->connect(check_worker_db_config);

	pthread_mutex_lock(&check_worker_lock);
	pthread_cond_signal(&check_worker_wait);

	for (;;) {
		if (check_worker_stop && first_file == NULL)
			break;

		if (first_file == NULL)
			pthread_cond_wait(&check_worker_wait, &check_worker_lock);

		struct soj_restorearchive_check_worker_private * ptr, * last;
		for (ptr = first_file, last = NULL; ptr != NULL; last = ptr, ptr = ptr->next) {
			pthread_mutex_unlock(&check_worker_lock);

			if (ptr->nb_total_volumes == 0)
				ptr->nb_total_volumes = db_connect->ops->get_nb_volumes_of_file(db_connect, ptr->archive, ptr->file);

			if (ptr->nb_volumes_done >= ptr->nb_total_volumes) {
				pthread_mutex_lock(&check_worker_lock);

				if (last != NULL)
					last->next = ptr->next;
				else
					first_file = ptr->next;

				if (first_file == NULL)
					last_file = NULL;

				pthread_mutex_unlock(&check_worker_lock);

				soj_restorearchive_check_worker_check(job, ptr, db_connect);
				free(ptr);

				pthread_mutex_lock(&check_worker_lock);
				break;
			}
		}
	}

	pthread_cond_signal(&check_worker_wait);
	pthread_mutex_unlock(&check_worker_lock);

	check_worker_running = false;

	db_connect->ops->free(db_connect);
}

void soj_restorearchive_check_worker_start(struct so_database_config * db_config) {
	if (!check_worker_running) {
		pthread_mutex_lock(&check_worker_lock);

		check_worker_db_config = db_config;

		so_thread_pool_run2("check worker", soj_restorearchive_check_worker_do, NULL, 8);

		pthread_cond_wait(&check_worker_wait, &check_worker_lock);
		pthread_mutex_unlock(&check_worker_lock);
	}
}

void soj_restorearchive_check_worker_stop() {
	pthread_mutex_lock(&check_worker_lock);
	check_worker_stop = true;
	pthread_cond_signal(&check_worker_wait);
	if (check_worker_running)
		pthread_cond_wait(&check_worker_wait, &check_worker_lock);
	pthread_mutex_unlock(&check_worker_lock);
}
