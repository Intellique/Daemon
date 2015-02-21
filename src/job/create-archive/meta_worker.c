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

// pthread_mutex_*
#include <pthread.h>
// bool
#include <stdbool.h>
// free, malloc
#include <stdlib.h>
// strdup
#include <string.h>

#include <libstoriqone/thread_pool.h>
#include <libstoriqone/value.h>

#include "meta_worker.h"

struct meta_worker_list {
	char * filename;
	struct meta_worker_list * next;
};

static void soj_create_archive_meta_worker_do(void * arg);

static volatile bool meta_worker_do = false;
static volatile bool meta_worker_stop = false;
static struct meta_worker_list * file_first = NULL, * file_last = NULL;
static struct so_value * files = NULL;

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t wait = PTHREAD_COND_INITIALIZER;


void soj_create_archive_meta_worker_add_file(const char * filename) {
	struct meta_worker_list * file = malloc(sizeof(struct meta_worker_list));
	file->filename = strdup(filename);
	file->next = NULL;

	pthread_mutex_lock(&lock);
	if (file_first == NULL)
		file_first = file_last = file;
	else
		file_last = file_last->next = file;
	pthread_cond_signal(&wait);
	pthread_mutex_unlock(&lock);
}

static void soj_create_archive_meta_worker_do(void * arg __attribute__((unused))) {
	meta_worker_do = true;
	files = so_value_new_hashtable2();

	while (!meta_worker_stop) {
		pthread_mutex_lock(&lock);
		if (meta_worker_stop && file_first == NULL)
			break;

		if (file_first == NULL) {
			pthread_cond_signal(&wait);
			pthread_cond_wait(&wait, &lock);
		}

		struct meta_worker_list * files = file_first;
		file_first = file_last = NULL;
		pthread_mutex_unlock(&lock);

		while (files != NULL) {

			struct meta_worker_list * next = files->next;
			free(files->filename);
			free(files);
			files = next;
		}
	}

	pthread_cond_signal(&wait);
	pthread_mutex_unlock(&lock);
}

void soj_create_archive_meta_worker_start() {
	if (!meta_worker_do) {
		so_thread_pool_run2("meta worker", soj_create_archive_meta_worker_do, NULL, 8);
	}
}

void soj_create_archive_meta_worker_wait() {
	pthread_mutex_lock(&lock);
	meta_worker_stop = true;
	pthread_cond_signal(&wait);
	pthread_cond_wait(&wait, &lock);
	pthread_mutex_unlock(&lock);
}

