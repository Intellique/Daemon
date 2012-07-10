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
*  Last modified: Tue, 10 Jul 2012 09:41:15 +0200                         *
\*************************************************************************/

#define _GNU_SOURCE
// glob
#include <glob.h>
// pthread_mutex_lock, pthread_mutex_unlock,
#include <pthread.h>
// free, malloc, realloc
#include <stdlib.h>
// snprintf
#include <stdio.h>
// strcmp, strdup
#include <string.h>
// pipe, read, write
#include <unistd.h>

#include <libstone/checksum.h>
#include <libstone/database.h>
#include <libstone/log.h>

#include "config.h"
#include "loader.h"


static struct st_checksum_driver ** st_checksum_drivers = 0;
static unsigned int st_checksum_nb_drivers = 0;


char * st_checksum_compute(const char * checksum, const char * data, ssize_t length) {
	if (!checksum || !data || length < 0) {
		st_log_write_all(st_log_level_error, st_log_type_checksum, "compute error");
		if (!checksum)
			st_log_write_all(st_log_level_error, st_log_type_checksum, "because checksum is null");
		if (!checksum)
			st_log_write_all(st_log_level_error, st_log_type_checksum, "because data is null");
		if (length < 0)
			st_log_write_all(st_log_level_error, st_log_type_checksum, "because length is lower than 0 (length=%zd)", length);
		return 0;
	}

	struct st_checksum_driver * driver = st_checksum_get_driver(checksum);
	if (!driver) {
		st_log_write_all(st_log_level_error, st_log_type_checksum, "Driver '%s' not found", checksum);
		return 0;
	}

	struct st_checksum * chck = driver->new_checksum();
    if (length > 0)
        chck->ops->update(chck, data, length);

	char * digest = chck->ops->digest(chck);
	chck->ops->free(chck);

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
    if (!driver)
        return 0;

	static pthread_mutex_t lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
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

	return dr;
}

void st_checksum_register_driver(struct st_checksum_driver * driver) {
	if (!driver) {
		st_log_write_all(st_log_level_error, st_log_type_checksum, "Try to register with driver=0");
		return;
	}

	if (driver->api_level != STONE_CHECKSUM_API_LEVEL) {
		st_log_write_all(st_log_level_error, st_log_type_checksum, "Driver(%s) has not the correct api version (current: %d, expected: %d)", driver->name, driver->api_level, STONE_CHECKSUM_API_LEVEL);
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

void st_checksum_sync_plugins(struct st_database_connection * connection) {
	if (!connection)
		return;

	char path[256];
	snprintf(path, 256, "%s/libchecksum-*.so", MODULE_PATH);

	glob_t gl;
	gl.gl_offs = 0;
	glob(path, GLOB_DOOFFS, 0, &gl);

	unsigned int i;
	for (i = 0; i < gl.gl_pathc; i++) {
		char * ptr = strrchr(gl.gl_pathv[i], '/') + 1;

		char plugin[64];
		sscanf(ptr, "libchecksum-%64[^.].so", plugin);

		//if (connection->ops->sync_plugin_checksum(connection, plugin))
		//	st_log_write_all(st_log_level_error, st_log_type_checksum, "Failed to synchronize plugin (%s)", plugin);
	}
}

