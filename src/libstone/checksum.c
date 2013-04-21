/****************************************************************************\
*                             __________                                     *
*                            / __/_  __/__  ___  ___                         *
*                           _\ \  / / / _ \/ _ \/ -_)                        *
*                          /___/ /_/  \___/_//_/\__/                         *
*                                                                            *
*  ------------------------------------------------------------------------  *
*  This file is a part of STone                                              *
*                                                                            *
*  STone is free software; you can redistribute it and/or modify             *
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
*  Copyright (C) 2013, Clercin guillaume <gclercin@intellique.com>           *
*  Last modified: Sun, 21 Apr 2013 17:05:41 +0200                            *
\****************************************************************************/

#define _GNU_SOURCE
// glob, globfree
#include <glob.h>
// pthread_mutex_lock, pthread_mutex_unlock,
#include <pthread.h>
// free, malloc, realloc
#include <stdlib.h>
// sscanf
#include <stdio.h>
// strcmp, strdup
#include <string.h>
// pipe, read, write
#include <unistd.h>

#include <libstone/checksum.h>
#include <libstone/database.h>
#include <libstone/log.h>
#include <libstone/util/debug.h>

#include "config.h"
#include "loader.h"


static struct st_checksum_driver ** st_checksum_drivers = NULL;
static unsigned int st_checksum_nb_drivers = 0;

static void st_checksum_exit(void) __attribute__((destructor));


char * st_checksum_compute(const char * checksum, const void * data, ssize_t length) {
	if (checksum == NULL || (data == NULL && length > 0) || length < 0) {
		st_log_write_all(st_log_level_error, st_log_type_checksum, "compute error");
		if (checksum == NULL)
			st_log_write_all(st_log_level_error, st_log_type_checksum, "because checksum is null");
		if (data == NULL && length > 0)
			st_log_write_all(st_log_level_error, st_log_type_checksum, "because data is null");
		if (length < 0)
			st_log_write_all(st_log_level_error, st_log_type_checksum, "because length is lower than 0 (length=%zd)", length);

		st_debug_log_stack(16);
		return NULL;
	}

	struct st_checksum_driver * driver = st_checksum_get_driver(checksum);
	if (driver == NULL) {
		st_log_write_all(st_log_level_error, st_log_type_checksum, "Driver '%s' not found", checksum);
		return NULL;
	}

	struct st_checksum * chck = driver->new_checksum();
	if (length > 0)
		chck->ops->update(chck, data, length);

	char * digest = chck->ops->digest(chck);
	chck->ops->free(chck);

	st_log_write_all(st_log_level_debug, st_log_type_checksum, "compute %s checksum => '%s'", checksum, digest);

	return digest;
}

void st_checksum_convert_to_hex(unsigned char * digest, ssize_t length, char * hex_digest) {
	if (digest == NULL || length < 1 || hex_digest == NULL) {
		st_log_write_all(st_log_level_error, st_log_type_checksum, "st_checksum_convert_to_hex error");
		if (digest == NULL)
			st_log_write_all(st_log_level_error, st_log_type_checksum, "because digest is null");
		if (length < 1)
			st_log_write_all(st_log_level_error, st_log_type_checksum, "because length is lower than 1 (length=%zd)", length);
		if (hex_digest == NULL)
			st_log_write_all(st_log_level_error, st_log_type_checksum, "because hexDigest is null");

		st_debug_log_stack(16);
		return;
	}

	int i;
	for (i = 0; i < length; i++)
		snprintf(hex_digest + (i << 1), 3, "%02x", digest[i]);
	hex_digest[i << 1] = '\0';

	st_log_write_all(st_log_level_debug, st_log_type_checksum, "computed => '%s'", hex_digest);
}

static void st_checksum_exit() {
	free(st_checksum_drivers);
	st_checksum_drivers = NULL;
	st_checksum_nb_drivers = 0;
}

struct st_checksum_driver * st_checksum_get_driver(const char * driver) {
	if (driver == NULL) {
		st_log_write_all(st_log_level_error, st_log_type_checksum, "Get checksum driver with driver's name is NULL");
		st_debug_log_stack(16);
		return NULL;
	}

	static pthread_mutex_t lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
	pthread_mutex_lock(&lock);

	unsigned int i;
	struct st_checksum_driver * dr = NULL;
	for (i = 0; i < st_checksum_nb_drivers && dr == NULL; i++)
		if (!strcmp(driver, st_checksum_drivers[i]->name))
			dr = st_checksum_drivers[i];

	if (dr == NULL) {
		void * cookie = st_loader_load("checksum", driver);

		if (cookie == NULL) {
			st_log_write_all(st_log_level_error, st_log_type_checksum, "Failed to load checksum driver '%s'", driver);
			pthread_mutex_unlock(&lock);
			return NULL;
		}

		for (i = 0; i < st_checksum_nb_drivers; i++)
			if (!strcmp(driver, st_checksum_drivers[i]->name)) {
				dr = st_checksum_drivers[i];
				dr->cookie = cookie;

				st_log_write_all(st_log_level_debug, st_log_type_checksum, "Checksum driver '%s' is now registred, src checksum: %s", driver, dr->src_checksum);

				break;
			}

		if (dr == NULL)
			st_log_write_all(st_log_level_error, st_log_type_checksum, "Checksum driver '%s' not found", driver);
	}

	pthread_mutex_unlock(&lock);

	return dr;
}

void st_checksum_register_driver(struct st_checksum_driver * driver) {
	if (driver == NULL) {
		st_log_write_all(st_log_level_error, st_log_type_checksum, "Try to register with null driver");
		st_debug_log_stack(16);
		return;
	}

	if (st_plugin_check(&driver->api_level) == false) {
		st_log_write_all(st_log_level_error, st_log_type_checksum, "Driver '%s' has not the correct api level", driver->name);
		return;
	}

	unsigned int i;
	for (i = 0; i < st_checksum_nb_drivers; i++)
		if (st_checksum_drivers[i] == driver || !strcmp(driver->name, st_checksum_drivers[i]->name)) {
			st_log_write_all(st_log_level_warning, st_log_type_checksum, "Checksum driver '%s' is already registred", driver->name);
			st_debug_log_stack(16);
			return;
		}

	void * new_addr = realloc(st_checksum_drivers, (st_checksum_nb_drivers + 1) * sizeof(struct st_checksum_driver *));
	if (new_addr == NULL) {
		st_log_write_all(st_log_level_error, st_log_type_checksum, "Checksum driver '%s' cannot be registred because there is not enough memory", driver->name);
		st_debug_log_stack(16);
		return;
	}

	st_checksum_drivers = new_addr;
	st_checksum_drivers[st_checksum_nb_drivers] = driver;
	st_checksum_nb_drivers++;

	st_loader_register_ok();

	st_log_write_all(st_log_level_info, st_log_type_checksum, "Checksum driver '%s' is now registred", driver->name);
}

void st_checksum_sync_plugins(struct st_database_connection * connection) {
	if (connection == NULL) {
		st_log_write_all(st_log_level_error, st_log_type_checksum, "Try to synchronize checksum's plugins without database connection");
		st_debug_log_stack(16);
		return;
	}

	if (connection->ops->is_connection_closed(connection)) {
		st_log_write_all(st_log_level_error, st_log_type_checksum, "Try to synchronize checksum's plugins with closed connection to database");
		st_debug_log_stack(16);
		return;
	}

	glob_t gl;
	glob(MODULE_PATH "/libchecksum-*.so", 0, NULL, &gl);

	unsigned int i;
	for (i = 0; i < gl.gl_pathc; i++) {
		char * ptr = strrchr(gl.gl_pathv[i], '/') + 1;

		char plugin[64];
		sscanf(ptr, "libchecksum-%64[^.].so", plugin);

		if (connection->ops->sync_plugin_checksum(connection, plugin))
			st_log_write_all(st_log_level_error, st_log_type_checksum, "Failed to synchronize plugin '%s'", plugin);
	}

	globfree(&gl);
}

