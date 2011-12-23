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
*  Copyright (C) 2011, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Fri, 23 Dec 2011 22:38:02 +0100                         *
\*************************************************************************/

#define _GNU_SOURCE
// pthread_attr_destroy, pthread_attr_init, pthread_attr_setdetachstate,
// pthread_cond_destroy, pthread_cond_init, pthread_cond_signal,
// pthread_cond_wait, pthread_create, pthread_mutex_destroy,
// pthread_mutex_init, pthread_mutex_lock, pthread_mutex_unlock,
// pthread_setcancelstate
#include <pthread.h>
// free, malloc, realloc
#include <stdlib.h>
// snprintf
#include <stdio.h>
// strcmp, strdup
#include <string.h>
// pipe, read, write
#include <unistd.h>

#include <stone/checksum.h>
#include <stone/log.h>

#include "loader.h"

struct st_checksum_helper_private {
	pthread_t thread;
	struct st_checksum * checksum;
	char * digest;
	volatile int run;

	struct st_helper_list {
		void * buffer;
		ssize_t length;

		struct st_helper_list * next;
	} * first, * last;

	unsigned long nb_read;
	unsigned long nb_write;

	pthread_mutex_t lock;
	pthread_cond_t wait;
};

static struct st_checksum * st_checksum_helper_clone(struct st_checksum * new_checksum, struct st_checksum * current_checksum);
static char * st_checksum_helper_digest(struct st_checksum * helper);
static void st_checksum_helper_free(struct st_checksum * helper);
static ssize_t st_checksum_helper_update(struct st_checksum * helper, const void * data, ssize_t length);
static void * st_checksum_helper_work(void * param);

static struct st_checksum_driver ** st_checksum_drivers = 0;
static unsigned int st_checksum_nb_drivers = 0;

static struct st_checksum_ops st_checksum_helper_ops = {
	.clone	= st_checksum_helper_clone,
	.digest	= st_checksum_helper_digest,
	.free	= st_checksum_helper_free,
	.update	= st_checksum_helper_update,
};


char * st_checksum_compute(const char * checksum, const char * data, ssize_t length) {
	if (!checksum || !data || length == 0) {
		st_log_write_all(st_log_level_error, st_log_type_checksum, "Checksum: compute error");
		if (!checksum)
			st_log_write_all(st_log_level_error, st_log_type_checksum, "Checksum: because checksum is null");
		if (!checksum)
			st_log_write_all(st_log_level_error, st_log_type_checksum, "Checksum: because data is null");
		if (length < 1)
			st_log_write_all(st_log_level_error, st_log_type_checksum, "Checksum: because length is lower than 1 (length=%zd)", length);
		return 0;
	}

	struct st_checksum_driver * driver = st_checksum_get_driver(checksum);
	if (!driver) {
		st_log_write_all(st_log_level_error, st_log_type_checksum, "Checksum: Driver '%s' not found", checksum);
		return 0;
	}

	struct st_checksum chck;
	driver->new_checksum(&chck);
	chck.ops->update(&chck, data, length);

	char * digest = chck.ops->digest(&chck);
	chck.ops->free(&chck);

	st_log_write_all(st_log_level_debug, st_log_type_checksum, "Checksum: compute %s checksum => '%s'", checksum, digest);

	return digest;
}

void st_checksum_convert_to_hex(unsigned char * digest, ssize_t length, char * hexDigest) {
	if (!digest || length < 1 || !hexDigest) {
		st_log_write_all(st_log_level_error, st_log_type_checksum, "Checksum: st_checksum_convert_to_hex error");
		if (!digest)
			st_log_write_all(st_log_level_error, st_log_type_checksum, "Checksum: because digest is null");
		if (length < 1)
			st_log_write_all(st_log_level_error, st_log_type_checksum, "Checksum: because length is lower than 1 (length=%zd)", length);
		if (!hexDigest)
			st_log_write_all(st_log_level_error, st_log_type_checksum, "Checksum: because hexDigest is null");
		return;
	}

	int i;
	for (i = 0; i < length; i++)
		snprintf(hexDigest + (i << 1), 3, "%02x", digest[i]);
	hexDigest[i << 1] = '\0';

	st_log_write_all(st_log_level_debug, st_log_type_checksum, "Checksum: computed => '%s'", hexDigest);
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
		st_log_write_all(st_log_level_error, st_log_type_checksum, "Checksum: Failed to load driver '%s'", driver);
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
		st_log_write_all(st_log_level_error, st_log_type_checksum, "Checksum: Driver '%s' not found", driver);

	pthread_mutex_unlock(&lock);
	if (old_state == PTHREAD_CANCEL_DISABLE)
		pthread_setcancelstate(old_state, 0);

	return dr;
}

struct st_checksum * st_checksum_get_helper(struct st_checksum * h, struct st_checksum * checksum) {
	if (!checksum) {
		st_log_write_all(st_log_level_error, st_log_type_checksum, "Checksum: get_helper require that checksum is not null");
		return 0;
	}

	st_log_write_all(st_log_level_debug, st_log_type_checksum, "Checksum: create new thread helper for checksum = %p", checksum);

	if (!h)
		h = malloc(sizeof(struct st_checksum));
	h->ops = &st_checksum_helper_ops;
	h->driver = checksum->driver;

	struct st_checksum_helper_private * hp = h->data = malloc(sizeof(struct st_checksum_helper_private));
	hp->checksum = checksum;
	hp->digest = 0;

	hp->first = hp->last = 0;

	hp->nb_read = hp->nb_write = 0;

	pthread_mutex_init(&hp->lock, 0);
	pthread_cond_init(&hp->wait, 0);

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	pthread_create(&hp->thread, &attr, st_checksum_helper_work, h);

	pthread_attr_destroy(&attr);

	return h;
}

struct st_checksum * st_checksum_helper_clone(struct st_checksum * new_checksum, struct st_checksum * current_checksum) {
	if (!current_checksum)
		return 0;

	struct st_checksum_helper_private * hp = current_checksum->data;
	struct st_checksum * clone = hp->checksum->ops->clone(0, hp->checksum);

	return st_checksum_get_helper(new_checksum, clone);
}

char * st_checksum_helper_digest(struct st_checksum * helper) {
	struct st_checksum_helper_private * hp = helper->data;

	pthread_mutex_lock(&hp->lock);
	if (hp->first)
		pthread_cond_wait(&hp->wait, &hp->lock);
	hp->run = 0;
	pthread_cond_signal(&hp->wait);
	pthread_cond_wait(&hp->wait, &hp->lock);
	pthread_mutex_unlock(&hp->lock);

	return strdup(hp->digest);
}

void st_checksum_helper_free(struct st_checksum * helper) {
	struct st_checksum_helper_private * hp = helper->data;

	if (hp->first)
		st_checksum_helper_digest(helper);

	hp->checksum->ops->free(hp->checksum);
	free(hp->digest);

	pthread_mutex_destroy(&hp->lock);
	pthread_cond_destroy(&hp->wait);

	free(hp);
}

ssize_t st_checksum_helper_update(struct st_checksum * helper, const void * data, ssize_t length) {
	if (!helper || !data || length < 1)
		return 1;

	struct st_checksum_helper_private * hp = helper->data;

	if (hp->digest)
		return -2;

	struct st_helper_list * elt = malloc(sizeof(struct st_helper_list));
	elt->buffer = malloc(length);
	elt->length = length;
	elt->next = 0;
	memcpy(elt->buffer, data, length);

	pthread_mutex_lock(&hp->lock);
	if (!hp->first)
		hp->first = hp->last = elt;
	else if (hp->first == hp->last)
		hp->first->next = hp->last = elt;
	else
		hp->last = hp->last->next = elt;

	pthread_cond_signal(&hp->wait);
	pthread_mutex_unlock(&hp->lock);

	hp->nb_read++;

	return length;
}

void * st_checksum_helper_work(void * param) {
	struct st_checksum * h = param;
	struct st_checksum_helper_private * hp = h->data;

	st_log_write_all(st_log_level_debug, "Checksum: Starting helper for checksum(%p)", hp->checksum);

	for (hp->run = 1;;) {
		pthread_mutex_lock(&hp->lock);
		if (!hp->first && hp->run) {
			pthread_cond_signal(&hp->wait);
			pthread_cond_wait(&hp->wait, &hp->lock);
		}

		if (!hp->first && !hp->run)
			break;

		struct st_helper_list * elt = hp->first;
		hp->first = elt->next;
		if (!hp->first)
			hp->last = 0;
		elt->next = 0;
		pthread_mutex_unlock(&hp->lock);

		hp->checksum->ops->update(hp->checksum, elt->buffer, elt->length);

		free(elt->buffer);
		free(elt);

		hp->nb_write++;
	}

	hp->digest = hp->checksum->ops->digest(hp->checksum);

	pthread_cond_signal(&hp->wait);
	pthread_mutex_unlock(&hp->lock);

	st_log_write_all(st_log_level_debug, st_log_type_checksum, "Checksum: Thread helper for checksum(%p) terminated", hp->checksum);

	return 0;
}

void st_checksum_register_driver(struct st_checksum_driver * driver) {
	if (!driver) {
		st_log_write_all(st_log_level_error, st_log_type_checksum, "Checksum: Try to register with driver=0");
		return;
	}

	if (driver->api_version != STONE_CHECKSUM_APIVERSION) {
		st_log_write_all(st_log_level_error, st_log_type_checksum, "Checksum: Driver(%s) has not the correct api version (current: %d, expected: %d)", driver->name, driver->api_version, STONE_CHECKSUM_APIVERSION);
		return;
	}

	unsigned int i;
	for (i = 0; i < st_checksum_nb_drivers; i++)
		if (st_checksum_drivers[i] == driver || !strcmp(driver->name, st_checksum_drivers[i]->name)) {
			st_log_write_all(st_log_level_warning, st_log_type_checksum, "Checksum: Driver(%s) is already registred", driver->name);
			return;
		}

	st_checksum_drivers = realloc(st_checksum_drivers, (st_checksum_nb_drivers + 1) * sizeof(struct st_checksum_driver *));
	st_checksum_drivers[st_checksum_nb_drivers] = driver;
	st_checksum_nb_drivers++;

	st_loader_register_ok();

	st_log_write_all(st_log_level_info, st_log_type_checksum, "Checksum: Driver(%s) is now registred", driver->name);
}

