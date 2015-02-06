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

#define _GNU_SOURCE
// open
#include <fcntl.h>
// dgettext
#include <libintl.h>
// pthread_mutex_lock, pthread_mutex_unlock
#include <pthread.h>
// asprintf, snprintf
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

#include <libstoriqone/checksum.h>
#include <libstoriqone/log.h>
#include <libstoriqone/value.h>

#include "loader.h"


static struct so_value * so_checksum_drivers = NULL;
static pthread_mutex_t so_checksum_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

static void so_checksum_exit(void) __attribute__((destructor));
static void so_checksum_init(void) __attribute__((constructor));


char * so_checksum_compute(const char * checksum, const void * data, ssize_t length) {
	if (checksum == NULL || (data == NULL && length > 0) || length < 0) {
		so_log_write(so_log_level_error, dgettext("libstoriqone", "so_checksum_compute: invalid parameters (checksum: %p, data: %p, length: %zd)"), checksum, data, length);
		return NULL;
	}

	struct so_checksum_driver * driver = so_checksum_get_driver(checksum);
	if (driver == NULL) {
		so_log_write(so_log_level_error, dgettext("libstoriqone", "so_checksum_compute: failed to load checksum plugin '%s'"), checksum);
		return NULL;
	}

	struct so_checksum * chck = driver->new_checksum();
	if (length > 0)
		chck->ops->update(chck, data, length);

	char * digest = chck->ops->digest(chck);
	chck->ops->free(chck);

	return digest;
}

void so_checksum_convert_to_hex(unsigned char * digest, ssize_t length, char * hex_digest) {
	if (digest == NULL || length < 1 || hex_digest == NULL) {
		so_log_write(so_log_level_error, dgettext("libstoriqone", "so_checksum_convert_to_hex: invalid parameters (digest: %p, length: %zd, hex_digest: %s)"), digest, length, hex_digest);
		return;
	}

	int i, j;
	for (i = 0, j = 0; i < length; i++, j += 2)
		snprintf(hex_digest + j, 3, "%02x", digest[i]);
	hex_digest[j] = '\0';
}

static void so_checksum_exit() {
	so_value_free(so_checksum_drivers);
	so_checksum_drivers = NULL;
}

char * so_checksum_gen_salt(const char * checksum, size_t length) {
	if (length < 8) {
		so_log_write(so_log_level_error, dgettext("libstoriqone", "so_checksum_gen_salt: parameter 'length' should be greater than 8 (instead of  %zd)"), length);
		return NULL;
	}

	int fd = open("/dev/urandom", O_RDONLY);
	if (fd < 0) {
		so_log_write(so_log_level_error, dgettext("libstoriqone", "so_checksum_gen_salt: failed to open \"/dev/urandom\" to generate salt parce %m"));
		return NULL;
	}

	ssize_t half_len = length / 2;
	unsigned char * buffer = malloc(half_len);
	ssize_t nb_read = read(fd, buffer, half_len);
	int failed = close(fd);

	if (failed != 0)
		so_log_write(so_log_level_warning, dgettext("libstoriqone", "so_checksum_gen_salt: warning, failed to close \"/dev/urandom\" (fd: %d) because %m"), fd);

	if (nb_read < 0) {
		free(buffer);
		so_log_write(so_log_level_error, dgettext("libstoriqone", "so_checksum_gen_salt: error while reading from \"/dev/urandom\" because %m"));
		return NULL;
	}

	if (nb_read < half_len) {
		free(buffer);
		so_log_write(so_log_level_error, dgettext("libstoriqone", "so_checksum_gen_salt: read less than expected from \"/dev/urandom\" (nb read: %zd, read expected: %zd)"), nb_read, half_len);
		return NULL;
	}

	char * result;
	if (checksum != NULL)
		result = so_checksum_compute(checksum, buffer, half_len);
	else {
		result = malloc(length + 1);
		so_checksum_convert_to_hex(buffer, half_len, result);
	}

	free(buffer);
	return result;
}

struct so_checksum_driver * so_checksum_get_driver(const char * driver) {
	if (driver == NULL) {
		so_log_write(so_log_level_error, dgettext("libstoriqone", "so_checksum_get_driver: invalide parameter, 'driver' should not be NULL"));
		return NULL;
	}

	pthread_mutex_lock(&so_checksum_lock);

	void * cookie = NULL;
	if (!so_value_hashtable_has_key2(so_checksum_drivers, driver)) {
		cookie = so_loader_load("checksum", driver);

		if (cookie == NULL) {
			pthread_mutex_unlock(&so_checksum_lock);
			so_log_write(so_log_level_error, dgettext("libstoriqone", "so_checksum_get_driver: failed to load checksum driver '%s'"), driver);
			return NULL;
		}

		if (!so_value_hashtable_has_key2(so_checksum_drivers, driver)) {
			pthread_mutex_unlock(&so_checksum_lock);
			so_log_write(so_log_level_warning, dgettext("libstoriqone", "so_checksum_get_driver: driver '%s' did not call 'so_checksum_register_driver'"), driver);
			return NULL;
		}
	}

	struct so_value * vdriver = so_value_hashtable_get2(so_checksum_drivers, driver, false, false);
	struct so_checksum_driver * dr = so_value_custom_get(vdriver);

	if (cookie != NULL)
		dr->cookie = cookie;

	pthread_mutex_unlock(&so_checksum_lock);

	return dr;
}

static void so_checksum_init() {
	so_checksum_drivers = so_value_new_hashtable2();
}

void so_checksum_register_driver(struct so_checksum_driver * driver) {
	if (driver == NULL) {
		so_log_write(so_log_level_error, dgettext("libstoriqone", "so_checksum_register_driver: try to register with NULL driver"));
		return;
	}

	pthread_mutex_lock(&so_checksum_lock);

	if (so_value_hashtable_has_key2(so_checksum_drivers, driver->name)) {
		pthread_mutex_unlock(&so_checksum_lock);
		so_log_write(so_log_level_warning, dgettext("libstoriqone", "so_checksum_register_driver: checksum driver '%s' is already registred"), driver->name);
		return;
	}

	so_value_hashtable_put2(so_checksum_drivers, driver->name, so_value_new_custom(driver, NULL), true);
	so_loader_register_ok();

	pthread_mutex_unlock(&so_checksum_lock);

	so_log_write(so_log_level_info, dgettext("libstoriqone", "so_checksum_register_driver: checksum driver '%s' is now registred"), driver->name);
}

char * so_checksum_salt_password(const char * checksum, const char * password, const char * salt) {
	if (password == NULL || salt == NULL) {
		so_log_write(so_log_level_error, dgettext("libstoriqone", "so_checksum_salt_password: invalid parameters (password: %p, salt: %p)"), password, salt);
		return NULL;
	}

	char * pw_salt;
	asprintf(&pw_salt, "%s%s", password, salt);

	char * result = so_checksum_compute(checksum, pw_salt, strlen(pw_salt));
	free(pw_salt);

	return result;
}

