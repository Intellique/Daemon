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
*  Copyright (C) 2012, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Tue, 13 Mar 2012 18:56:10 +0100                         *
\*************************************************************************/

#define _GNU_SOURCE
// glob
#include <glob.h>
// pthread_attr_destroy, pthread_attr_init, pthread_attr_setdetachstate,
// pthread_cond_destroy, pthread_cond_init, pthread_cond_signal,
// pthread_cond_wait, pthread_create, pthread_mutex_destroy,
// pthread_mutex_init, pthread_mutex_lock, pthread_mutex_unlock,
// pthread_setcancelstate
#include <pthread.h>
// sem_init, sem_post, sem_wait
#include <semaphore.h>
// free, malloc, realloc
#include <stdlib.h>
// snprintf
#include <stdio.h>
// strcmp, strdup
#include <string.h>
// pipe, read, write
#include <unistd.h>

#include <stone/database.h>
#include <stone/log.h>
#include <stone/threadpool.h>

#include "checksum.h"
#include "config.h"
#include "loader.h"

#define NB_BUFFERS 32

struct st_checksum_helper_private {
	struct st_checksum * checksum;
	char * digest;
	volatile int run;

	struct st_helper_list {
		void * buffer;
		ssize_t length;
	} buffers[NB_BUFFERS];
	volatile struct st_helper_list * reader;
	volatile struct st_helper_list * writer;

	unsigned long nb_read;
	unsigned long nb_write;

	pthread_mutex_t lock;
	pthread_cond_t wait;
	sem_t ressources;
};

static char * st_checksum_helper_digest(struct st_checksum * helper);
static void st_checksum_helper_free(struct st_checksum * helper);
static ssize_t st_checksum_helper_update(struct st_checksum * helper, const void * data, ssize_t length);
static void st_checksum_helper_work(void * param);

static struct st_checksum_driver ** st_checksum_drivers = 0;
static unsigned int st_checksum_nb_drivers = 0;

static struct st_checksum_ops st_checksum_helper_ops = {
	.digest	= st_checksum_helper_digest,
	.free	= st_checksum_helper_free,
	.update	= st_checksum_helper_update,
};


char * st_checksum_compute(const char * checksum, const char * data, ssize_t length) {
	if (!checksum || !data || length == 0) {
		st_log_write_all(st_log_level_error, st_log_type_checksum, "compute error");
		if (!checksum)
			st_log_write_all(st_log_level_error, st_log_type_checksum, "because checksum is null");
		if (!checksum)
			st_log_write_all(st_log_level_error, st_log_type_checksum, "because data is null");
		if (length < 1)
			st_log_write_all(st_log_level_error, st_log_type_checksum, "because length is lower than 1 (length=%zd)", length);
		return 0;
	}

	struct st_checksum_driver * driver = st_checksum_get_driver(checksum);
	if (!driver) {
		st_log_write_all(st_log_level_error, st_log_type_checksum, "Driver '%s' not found", checksum);
		return 0;
	}

	struct st_checksum chck;
	driver->new_checksum(&chck);
	chck.ops->update(&chck, data, length);

	char * digest = chck.ops->digest(&chck);
	chck.ops->free(&chck);

	st_log_write_all(st_log_level_debug, st_log_type_checksum, "compute %s checksum => '%s'", checksum, digest);

	return digest;
}

void st_checksum_convert_to_hex(unsigned char * digest, ssize_t length, char * hexDigest) {
	if (!digest || length < 1 || !hexDigest) {
		st_log_write_all(st_log_level_error, st_log_type_checksum, "st_checksum_convert_to_hex error");
		if (!digest)
			st_log_write_all(st_log_level_error, st_log_type_checksum, "because digest is null");
		if (length < 1)
			st_log_write_all(st_log_level_error, st_log_type_checksum, "because length is lower than 1 (length=%zd)", length);
		if (!hexDigest)
			st_log_write_all(st_log_level_error, st_log_type_checksum, "because hexDigest is null");
		return;
	}

	int i;
	for (i = 0; i < length; i++)
		snprintf(hexDigest + (i << 1), 3, "%02x", digest[i]);
	hexDigest[i << 1] = '\0';

	st_log_write_all(st_log_level_debug, st_log_type_checksum, "computed => '%s'", hexDigest);
}

struct st_checksum_driver * st_checksum_get_driver(const char * driver) {
	static pthread_mutex_t lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

	int old_state;
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &old_state);
	pthread_mutex_lock(&lock);

	unsigned int i;
	struct st_checksum_driver * dr = 0;
	for (i = 0; i < st_checksum_nb_drivers && !dr; i++)
		if (!strcmp(driver, st_checksum_drivers[i]->name))
			dr = st_checksum_drivers[i];

	void * cookie = 0;
	if (!dr)
		cookie = st_loader_load("checksum", driver);

	if (!dr && !cookie) {
		st_log_write_all(st_log_level_error, st_log_type_checksum, "Failed to load driver '%s'", driver);
		pthread_mutex_unlock(&lock);
		if (old_state == PTHREAD_CANCEL_DISABLE)
			pthread_setcancelstate(old_state, 0);
		return 0;
	}

	for (i = 0; i < st_checksum_nb_drivers && !dr; i++)
		if (!strcmp(driver, st_checksum_drivers[i]->name)) {
			dr = st_checksum_drivers[i];
			dr->cookie = cookie;
		}

	if (!dr)
		st_log_write_all(st_log_level_error, st_log_type_checksum, "Driver '%s' not found", driver);

	pthread_mutex_unlock(&lock);
	if (old_state == PTHREAD_CANCEL_DISABLE)
		pthread_setcancelstate(old_state, 0);

	return dr;
}

struct st_checksum * st_checksum_get_helper(struct st_checksum * h, struct st_checksum * checksum) {
	if (!checksum) {
		st_log_write_all(st_log_level_error, st_log_type_checksum, "get_helper require that checksum is not null");
		return 0;
	}

	st_log_write_all(st_log_level_debug, st_log_type_checksum, "create new thread helper for checksum = %p", checksum);

	if (!h)
		h = malloc(sizeof(struct st_checksum));
	h->ops = &st_checksum_helper_ops;
	h->driver = checksum->driver;

	struct st_checksum_helper_private * hp = h->data = malloc(sizeof(struct st_checksum_helper_private));
	hp->checksum = checksum;
	hp->digest = 0;
	hp->run = 1;

	unsigned int i;
	for (i = 0; i < NB_BUFFERS; i++) {
		hp->buffers[i].buffer = 0;
		hp->buffers[i].length = 0;
	}
	hp->reader = hp->writer = hp->buffers;

	hp->nb_read = hp->nb_write = 0;

	pthread_mutex_init(&hp->lock, 0);
	pthread_cond_init(&hp->wait, 0);
	sem_init(&hp->ressources, 0, NB_BUFFERS);

	st_threadpool_run(st_checksum_helper_work, h);

	return h;
}

char * st_checksum_helper_digest(struct st_checksum * helper) {
	struct st_checksum_helper_private * hp = helper->data;

	pthread_mutex_lock(&hp->lock);
	hp->run = 0;
	pthread_cond_signal(&hp->wait);
	pthread_cond_wait(&hp->wait, &hp->lock);
	pthread_mutex_unlock(&hp->lock);

	return strdup(hp->digest);
}

void st_checksum_helper_free(struct st_checksum * helper) {
	struct st_checksum_helper_private * hp = helper->data;

	if (!hp->digest) {
		char * digest = st_checksum_helper_digest(helper);
		free(digest);
	}

	hp->checksum->ops->free(hp->checksum);
	free(hp->digest);

	pthread_mutex_destroy(&hp->lock);
	pthread_cond_destroy(&hp->wait);
	sem_destroy(&hp->ressources);

	free(hp);
}

ssize_t st_checksum_helper_update(struct st_checksum * helper, const void * data, ssize_t length) {
	if (!helper || !data || length < 1)
		return -1;

	struct st_checksum_helper_private * hp = helper->data;

	if (hp->digest)
		return -2;

	void * buffer = malloc(length);
	memcpy(buffer, data, length);

	sem_wait(&hp->ressources);

	pthread_mutex_lock(&hp->lock);

	hp->writer->buffer = buffer;
	hp->writer->length = length;

	hp->writer++;
	if (hp->writer - hp->buffers == NB_BUFFERS)
		hp->writer = hp->buffers;
	pthread_cond_signal(&hp->wait);
	pthread_mutex_unlock(&hp->lock);

	hp->nb_write++;

	return length;
}

void st_checksum_helper_work(void * param) {
	struct st_checksum * h = param;
	struct st_checksum_helper_private * hp = h->data;

	st_log_write_all(st_log_level_debug, st_log_type_checksum, "Starting thread helper for checksum(%p)", hp->checksum);

	for (;;) {
		pthread_mutex_lock(&hp->lock);

		if (!hp->reader->buffer && !hp->run)
			break;
		if (!hp->reader->buffer)
			pthread_cond_wait(&hp->wait, &hp->lock);
		if (!hp->reader->buffer && !hp->run)
			break;

		void * buffer = hp->reader->buffer;
		ssize_t length = hp->reader->length;

		hp->reader->buffer = 0;
		hp->reader++;
		if (hp->reader - hp->buffers == NB_BUFFERS)
			hp->reader = hp->buffers;
		pthread_mutex_unlock(&hp->lock);

		sem_post(&hp->ressources);

		hp->nb_read++;

		hp->checksum->ops->update(hp->checksum, buffer, length);

		free(buffer);
	}

	hp->digest = hp->checksum->ops->digest(hp->checksum);

	pthread_cond_signal(&hp->wait);
	pthread_mutex_unlock(&hp->lock);

	st_log_write_all(st_log_level_debug, st_log_type_checksum, "Thread helper for checksum(%p) terminated", hp->checksum);
}

void st_checksum_register_driver(struct st_checksum_driver * driver) {
	if (!driver) {
		st_log_write_all(st_log_level_error, st_log_type_checksum, "Try to register with driver=0");
		return;
	}

	if (driver->api_version != STONE_CHECKSUM_APIVERSION) {
		st_log_write_all(st_log_level_error, st_log_type_checksum, "Driver(%s) has not the correct api version (current: %d, expected: %d)", driver->name, driver->api_version, STONE_CHECKSUM_APIVERSION);
		return;
	}

	unsigned int i;
	for (i = 0; i < st_checksum_nb_drivers; i++)
		if (st_checksum_drivers[i] == driver || !strcmp(driver->name, st_checksum_drivers[i]->name)) {
			st_log_write_all(st_log_level_warning, st_log_type_checksum, "Driver(%s) is already registred", driver->name);
			return;
		}

	st_checksum_drivers = realloc(st_checksum_drivers, (st_checksum_nb_drivers + 1) * sizeof(struct st_checksum_driver *));
	st_checksum_drivers[st_checksum_nb_drivers] = driver;
	st_checksum_nb_drivers++;

	st_loader_register_ok();

	st_log_write_all(st_log_level_info, st_log_type_checksum, "Driver(%s) is now registred", driver->name);
}

void st_checksum_sync_plugins() {
	char path[256];
	snprintf(path, 256, "%s/libchecksum-*.so", MODULE_PATH);

	glob_t gl;
	gl.gl_offs = 0;
	glob(path, GLOB_DOOFFS, 0, &gl);

	struct st_database * db = st_db_get_default_db();
	struct st_database_connection * con = db->ops->connect(db, 0);

	unsigned int i;
	for (i = 0; i < gl.gl_pathc; i++) {
		char * ptr = strrchr(gl.gl_pathv[i], '/') + 1;

		char plugin[64];
		sscanf(ptr, "libchecksum-%64[^.].so", plugin);

		if (con->ops->sync_plugin_checksum(con, plugin))
			st_log_write_all(st_log_level_error, st_log_type_checksum, "Failed to synchronize plugin (%s)", plugin);
	}

	con->ops->close(con);
	con->ops->free(con);
	free(con);
}

