/*************************************************************************\
*        ______           ____     ___           __   _                   *
*       / __/ /____  ____/  _/__ _/ _ | ________/ /  (_)  _____ ____      *
*      _\ \/ __/ _ \/ __// // _ `/ __ |/ __/ __/ _ \/ / |/ / -_) __/      *
*     /___/\__/\___/_/ /___/\_, /_/ |_/_/  \__/_//_/_/|___/\__/_/         *
*                            /_/                                          *
*  ---------------------------------------------------------------------  *
*  This file is a part of StorIqArchiver                                  *
*                                                                         *
*  StorIqArchiver is free software; you can redistribute it and/or        *
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
*  Last modified: Tue, 22 Nov 2011 10:06:12 +0100                         *
\*************************************************************************/

// pthread_create
#include <pthread.h>
// malloc, realloc
#include <stdlib.h>
// snprintf
#include <stdio.h>
// strcmp
#include <string.h>
// pipe, read, write
#include <unistd.h>

#include <storiqArchiver/checksum.h>
#include <storiqArchiver/log.h>

#include "loader.h"

struct sa_checksum_helper_private {
	pthread_t thread;
	struct sa_checksum * checksum;
	char * digest;

	int fd_in;
	int fd_out;

	pthread_mutex_t lock;
	pthread_cond_t wait;
};

static struct sa_checksum * sa_checksum_helper_clone(struct sa_checksum * new_checksum, struct sa_checksum * current_checksum);
static char * sa_checksum_helper_digest(struct sa_checksum * helper);
static void sa_checksum_helper_free(struct sa_checksum * helper);
static ssize_t sa_checksum_helper_update(struct sa_checksum * helper, const void * data, ssize_t length);
static void * sa_checksum_helper_work(void * param);

static struct sa_checksum_driver ** sa_checksum_drivers = 0;
static unsigned int sa_checksum_nb_drivers = 0;
static struct sa_checksum_ops sa_checksum_helper_ops = {
	.clone	= sa_checksum_helper_clone,
	.digest	= sa_checksum_helper_digest,
	.free	= sa_checksum_helper_free,
	.update	= sa_checksum_helper_update,
};

char * sa_checksum_compute(const char * checksum, const char * data, unsigned int length) {
	if (!checksum || !data || length == 0)
		return 0;

	struct sa_checksum_driver * driver = sa_checksum_get_driver(checksum);
	if (!driver) {
		sa_log_write_all(sa_log_level_error, "Checksum: Driver %s not found", checksum);
		return 0;
	}

	struct sa_checksum chck;

	driver->new_checksum(&chck);
	chck.ops->update(&chck, data, length);

	char * digest = chck.ops->digest(&chck);

	chck.ops->free(&chck);

	sa_log_write_all(sa_log_level_debug, "Checksum: compute %s checksum => %s", checksum, digest);

	return digest;
}

void sa_checksum_convert_to_hex(unsigned char * digest, int length, char * hexDigest) {
	if (!digest || length < 1 || !hexDigest) {
		sa_log_write_all(sa_log_level_error, "Checksum: while sa_checksum_convert_to_hex");
		if (!digest)
			sa_log_write_all(sa_log_level_error, "Checksum: error because digest is null");
		if (length < 1)
			sa_log_write_all(sa_log_level_error, "Checksum: error because length is lower than 1 (length=%d)", length);
		if (!hexDigest)
			sa_log_write_all(sa_log_level_error, "Checksum: error because hexDigest is null");
		return;
	}

	int i;
	for (i = 0; i < length; i++)
		snprintf(hexDigest + (i << 1), 3, "%02x", digest[i]);
	hexDigest[i << 1] = '\0';

	sa_log_write_all(sa_log_level_debug, "Checksum: computed => %s", hexDigest);
}

struct sa_checksum_driver * sa_checksum_get_driver(const char * driver) {
	unsigned int i;
	for (i = 0; i < sa_checksum_nb_drivers; i++)
		if (!strcmp(driver, sa_checksum_drivers[i]->name))
			return sa_checksum_drivers[i];
	void * cookie = sa_loader_load("checksum", driver);
	if (!cookie) {
		sa_log_write_all(sa_log_level_error, "Checksum: Failed to load driver %s", driver);
		return 0;
	}
	for (i = 0; i < sa_checksum_nb_drivers; i++)
		if (!strcmp(driver, sa_checksum_drivers[i]->name)) {
			struct sa_checksum_driver * driver = sa_checksum_drivers[i];
			driver->cookie = cookie;
			return driver;
		}
	sa_log_write_all(sa_log_level_error, "Checksum: Driver %s not found", driver);
	return 0;
}

struct sa_checksum * sa_checksum_get_helper(struct sa_checksum * h, struct sa_checksum * checksum) {
	if (!h)
		h = malloc(sizeof(struct sa_checksum));
	h->ops = &sa_checksum_helper_ops;
	h->driver = checksum->driver;

	struct sa_checksum_helper_private * hp = h->data = malloc(sizeof(struct sa_checksum_helper_private));
	hp->checksum = checksum;
	hp->digest = 0;

	int fds[2];
	pipe(fds);
	hp->fd_in = fds[0];
	hp->fd_out = fds[1];

	pthread_mutex_init(&hp->lock, 0);
	pthread_cond_init(&hp->wait, 0);

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	pthread_create(&hp->thread, &attr, sa_checksum_helper_work, h);

	pthread_attr_destroy(&attr);

	return h;
}

struct sa_checksum * sa_checksum_helper_clone(struct sa_checksum * new_checksum, struct sa_checksum * current_checksum) {
	if (!current_checksum)
		return 0;

	struct sa_checksum_helper_private * hp = current_checksum->data;
	struct sa_checksum * clone = hp->checksum->ops->clone(0, hp->checksum);

	return sa_checksum_get_helper(new_checksum, clone);
}

char * sa_checksum_helper_digest(struct sa_checksum * helper) {
	struct sa_checksum_helper_private * hp = helper->data;

	if (hp->fd_out > -1) {
		pthread_mutex_lock(&hp->lock);
		if (hp->fd_out > -1) {
			close(hp->fd_out);
			hp->fd_out = -1;
			pthread_cond_wait(&hp->wait, &hp->lock);
		}
		pthread_mutex_unlock(&hp->lock);
	}

	return hp->digest;
}

void sa_checksum_helper_free(struct sa_checksum * helper) {
	struct sa_checksum_helper_private * hp = helper->data;

	if (hp->fd_out > -1)
		sa_checksum_helper_digest(helper);

	hp->checksum->ops->free(hp->checksum);
	free(hp->checksum);

	pthread_mutex_destroy(&hp->lock);
	pthread_cond_destroy(&hp->wait);

	free(hp);
	free(helper);
}

ssize_t sa_checksum_helper_update(struct sa_checksum * helper, const void * data, ssize_t length) {
	struct sa_checksum_helper_private * hp = helper->data;

	return write(hp->fd_out, data, length);
}

void * sa_checksum_helper_work(void * param) {
	struct sa_checksum * h = param;
	struct sa_checksum_helper_private * hp = h->data;

	ssize_t nb_read;
	char buffer[4096];
	while ((nb_read = read(hp->fd_in, buffer, 4096)) > 0)
		hp->checksum->ops->update(hp->checksum, buffer, nb_read);

	close(hp->fd_in);
	hp->fd_in = -1;

	pthread_mutex_lock(&hp->lock);

	hp->digest = hp->checksum->ops->digest(hp->checksum);

	pthread_cond_signal(&hp->wait);
	pthread_mutex_unlock(&hp->lock);

	return 0;
}

void sa_checksum_register_driver(struct sa_checksum_driver * driver) {
	if (!driver) {
		sa_log_write_all(sa_log_level_error, "Checksum: Try to register with driver=0");
		return;
	}

	if (driver->api_version != STORIQARCHIVER_CHECKSUM_APIVERSION) {
		sa_log_write_all(sa_log_level_error, "Checksum: Driver(%s) has not the correct api version (current: %d, expected: %d)", driver->name, driver->api_version, STORIQARCHIVER_CHECKSUM_APIVERSION);
		return;
	}

	unsigned int i;
	for (i = 0; i < sa_checksum_nb_drivers; i++)
		if (sa_checksum_drivers[i] == driver || !strcmp(driver->name, sa_checksum_drivers[i]->name)) {
			sa_log_write_all(sa_log_level_error, "Checksum: Driver(%s) is already registred", driver->name);
			return;
		}

	sa_checksum_drivers = realloc(sa_checksum_drivers, (sa_checksum_nb_drivers + 1) * sizeof(struct sa_checksum_driver *));
	sa_checksum_drivers[sa_checksum_nb_drivers] = driver;
	sa_checksum_nb_drivers++;

	sa_loader_register_ok();

	sa_log_write_all(sa_log_level_info, "Checksum: Driver(%s) is now registred", driver->name);
}

