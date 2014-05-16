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
*  Copyright (C) 2014, Clercin guillaume <gclercin@intellique.com>           *
\****************************************************************************/

#define _GNU_SOURCE
// open
#include <fcntl.h>
// pthread_mutex_lock, pthread_mutex_unlock,
#include <pthread.h>
// asprintf, snprintf, sscanf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// strlen
#include <string.h>
// open
#include <sys/stat.h>
// open
#include <sys/types.h>
// close, read
#include <unistd.h>

#include <libstone/log.h>
#include <libstone/value.h>

#include "checksum.h"
#include "loader.h"


static struct st_value * st_checksum_drivers = NULL;
static pthread_mutex_t st_checksum_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

static void st_checksum_exit(void) __attribute__((destructor));
static void st_checksum_init(void) __attribute__((constructor));


__asm__(".symver st_checksum_compute_v1, st_checksum_compute@@LIBSTONE_1.2");
char * st_checksum_compute_v1(const char * checksum, const void * data, ssize_t length) {
	if (checksum == NULL || (data == NULL && length > 0) || length < 0)
		return NULL;

	struct st_checksum_driver_v1 * driver = st_checksum_get_driver_v1(checksum);
	if (driver == NULL)
		return NULL;

	struct st_checksum_v1 * chck = driver->new_checksum();
	if (length > 0)
		chck->ops->update(chck, data, length);

	char * digest = chck->ops->digest(chck);
	chck->ops->free(chck);

	return digest;
}

__asm__(".symver st_checksum_convert_to_hex_v1, st_checksum_convert_to_hex@@LIBSTONE_1.2");
void st_checksum_convert_to_hex_v1(unsigned char * digest, ssize_t length, char * hex_digest) {
	if (digest == NULL || length < 1 || hex_digest == NULL)
		return;

	int i;
	for (i = 0; i < length; i++)
		snprintf(hex_digest + (i << 1), 3, "%02x", digest[i]);
	hex_digest[i << 1] = '\0';
}

static void st_checksum_exit() {
	st_value_free(st_checksum_drivers);
	st_checksum_drivers = NULL;
}

__asm__(".symver st_checksum_gen_salt_v1, st_checksum_gen_salt@@LIBSTONE_1.2");
char * st_checksum_gen_salt_v1(const char * checksum, size_t length) {
	if (length < 8)
		return NULL;

	int fd = open("/dev/urandom", O_RDONLY);
	if (fd < 0)
		return NULL;

	size_t half_len = length / 2;
	unsigned char * buffer = malloc(half_len);
	read(fd, buffer, half_len);
	close(fd);

	char * result;
	if (checksum != NULL)
		result = st_checksum_compute_v1(checksum, buffer, half_len);
	else {
		result = malloc(length + 1);
		st_checksum_convert_to_hex_v1(buffer, half_len, result);
	}

	free(buffer);
	return result;
}

__asm__(".symver st_checksum_get_driver_v1, st_checksum_get_driver@@LIBSTONE_1.2");
struct st_checksum_driver_v1 * st_checksum_get_driver_v1(const char * driver) {
	if (driver == NULL)
		return NULL;

	pthread_mutex_lock(&st_checksum_lock);

	void * cookie = NULL;
	if (!st_value_hashtable_has_key2(st_checksum_drivers, driver)) {
		cookie = st_loader_load("checksum", driver);

		if (cookie == NULL) {
			pthread_mutex_unlock(&st_checksum_lock);
			st_log_write(st_log_level_error, "Failed to load checksum driver '%s'", driver);
			return NULL;
		}

		if (!st_value_hashtable_has_key2(st_checksum_drivers, driver)) {
			pthread_mutex_unlock(&st_checksum_lock);
			st_log_write(st_log_level_warning, "Driver '%s' did not call 'register_driver'", driver);
			return NULL;
		}
	}

	struct st_value * vdriver = st_value_hashtable_get2(st_checksum_drivers, driver, false, false);
	struct st_checksum_driver_v1 * dr = st_value_custom_get(vdriver);

	if (cookie != NULL)
		dr->cookie = cookie;

	pthread_mutex_unlock(&st_checksum_lock);

	return dr;
}

static void st_checksum_init() {
	st_checksum_drivers = st_value_new_hashtable2();
}

__asm__(".symver st_checksum_register_driver_v1, st_checksum_register_driver@@LIBSTONE_1.2");
void st_checksum_register_driver_v1(struct st_checksum_driver_v1 * driver) {
	if (driver == NULL) {
		st_log_write(st_log_level_error, "Try to register with null driver");
		return;
	}

	pthread_mutex_lock(&st_checksum_lock);

	if (st_value_hashtable_has_key2(st_checksum_drivers, driver->name)) {
		pthread_mutex_unlock(&st_checksum_lock);
		st_log_write(st_log_level_warning, "Checksum driver '%s' is already registred", driver->name);
		return;
	}

	st_value_hashtable_put2(st_checksum_drivers, driver->name, st_value_new_custom(driver, NULL), true);
	driver->api_level = 1;
	st_loader_register_ok();

	pthread_mutex_unlock(&st_checksum_lock);

	st_log_write(st_log_level_info, "Checksum driver '%s' is now registred", driver->name);
}

__asm__(".symver st_checksum_salt_password_v1, st_checksum_salt_password@@LIBSTONE_1.2");
char * st_checksum_salt_password_v1(const char * checksum, const char * password, const char * salt) {
	char * pw_salt;
	asprintf(&pw_salt, "%s%s", password, salt);

	char * result = st_checksum_compute_v1(checksum, pw_salt, strlen(pw_salt));
	free(pw_salt);

	return result;
}


/*
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
*/

