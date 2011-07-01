/***********************************************************************\
*       ______           ____     ___           __   _                  *
*      / __/ /____  ____/  _/__ _/ _ | ________/ /  (_)  _____ ____     *
*     _\ \/ __/ _ \/ __// // _ `/ __ |/ __/ __/ _ \/ / |/ / -_) __/     *
*    /___/\__/\___/_/ /___/\_, /_/ |_/_/  \__/_//_/_/|___/\__/_/        *
*                           /_/                                         *
*  -------------------------------------------------------------------  *
*  This file is a part of StorIqArchiver                                *
*                                                                       *
*  StorIqArchiver is free software; you can redistribute it and/or      *
*  modify it under the terms of the GNU General Public License          *
*  as published by the Free Software Foundation; either version 3       *
*  of the License, or (at your option) any later version.               *
*                                                                       *
*  This program is distributed in the hope that it will be useful,      *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of       *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
*  GNU General Public License for more details.                         *
*                                                                       *
*  You should have received a copy of the GNU General Public License    *
*  along with this program; if not, write to the Free Software          *
*  Foundation, Inc., 51 Franklin Street, Fifth Floor,                   *
*  Boston, MA  02110-1301, USA.                                         *
*                                                                       *
*  -------------------------------------------------------------------  *
*  Copyright (C) 2011, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Fri, 01 Jul 2011 19:01:15 +0200                       *
\***********************************************************************/

// realloc
#include <stdlib.h>
// snprintf
#include <stdio.h>
// strcmp
#include <string.h>

#include <storiqArchiver/checksum.h>
#include <storiqArchiver/log.h>

#include "loader.h"

static struct sa_checksum_driver ** _sa_checksum_drivers = 0;
static unsigned int _sa_checksum_nb_drivers = 0;

char * sa_checksum_compute(const char * checksum, const char * data, unsigned int length) {
	if (!checksum || !data || length == 0)
		return 0;

	struct sa_checksum_driver * driver = sa_checksum_get_driver(checksum);
	if (!driver)
		return 0;

	struct sa_checksum chck;

	driver->new_checksum(&chck);
	chck.ops->update(&chck, data, length);

	char * digest = chck.ops->digest(&chck);

	chck.ops->free(&chck);

	return digest;
}

void sa_checksum_convert_to_hex(unsigned char * digest, int length, char * hexDigest) {
	if (!digest || length < 1 || !hexDigest) {
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
	for (i = 0; i < _sa_checksum_nb_drivers; i++)
		if (!strcmp(driver, _sa_checksum_drivers[i]->name))
			return _sa_checksum_drivers[i];
	if (sa_loader_load("checksum", driver))
		return 0;
	for (i = 0; i < _sa_checksum_nb_drivers; i++)
		if (!strcmp(driver, _sa_checksum_drivers[i]->name))
			return _sa_checksum_drivers[i];
	return 0;
}

void sa_checksum_register_driver(struct sa_checksum_driver * driver) {
	if (!driver)
		return;

	_sa_checksum_drivers = realloc(_sa_checksum_drivers, (_sa_checksum_nb_drivers + 1) * sizeof(struct sa_checksum_driver *));
	_sa_checksum_drivers[_sa_checksum_nb_drivers] = driver;
	_sa_checksum_nb_drivers++;

	sa_loader_register_ok();
}


/*
// dlclose, dlerror, dlopen
#include <dlfcn.h>
// strerror
#include <errno.h>
// realloc
#include <malloc.h>
// pthread_mutex_init, pthread_mutex_lock, pthread_mutex_unlock,
// pthread_mutexattr_destroy, pthread_mutexattr_init, pthread_mutexattr_settype
#define __USE_UNIX98
#include <pthread.h>
// snprintf
#include <stdio.h>
// strcasecmp, strcmp, strerror
#include <string.h>
// access
#include <unistd.h>

#include <storiqArchiver/checksum.h>
#include <storiqArchiver/log.h>

#include "config.h"

static struct checksum_driver ** checksum_drivers = 0;
static unsigned int checksum_nbDrivers = 0;
static pthread_mutex_t checksum_lock;


char * checksum_compute(const char * checksum, const char * data, unsigned int length) {
	if (!checksum || !data || length == 0)
		return 0;

	struct checksum_driver * driver = checksum_getDriver(checksum);
	if (!driver)
		return 0;

	struct checksum chck;

	driver->new_checksum(&chck);
	chck.ops->update(&chck, data, length);

	char * digest = chck.ops->digest(&chck);

	chck.ops->free(&chck);

	return digest;
}

void checksum_convert2Hex(unsigned char * digest, int length, char * hexDigest) {
	if (!digest || length < 1 || !hexDigest) {
		if (!digest)
			log_writeAll(Log_level_error, "Checksum: error because digest is null");
		if (length < 1)
			log_writeAll(Log_level_error, "Checksum: error because length is lower than 1 (length=%d)", length);
		if (!hexDigest)
			log_writeAll(Log_level_error, "Checksum: error because hexDigest is null");
		return;
	}

	int i;
	for (i = 0; i < length; i++)
		snprintf(hexDigest + (i << 1), 3, "%02x", digest[i]);
	hexDigest[i << 1] = '\0';

	log_writeAll(Log_level_debug, "Checksum: computed => %s", hexDigest);
}

struct checksum_driver * checksum_getDriver(const char * driver) {
	if (!driver) {
		log_writeAll(Log_level_error, "Checksum: get driver failed because driver is null");
		return 0;
	}

	pthread_mutex_lock(&checksum_lock);

	struct checksum_driver * dr = 0;
	unsigned int i;

	for (i = 0; i < checksum_nbDrivers && !dr; i++)
		if (!strcmp(checksum_drivers[i]->name, driver))
			dr = checksum_drivers[i];

	if (!dr && !checksum_loadDriver(driver))
		dr = checksum_getDriver(driver);

	pthread_mutex_unlock(&checksum_lock);

	log_writeAll(Log_level_debug, "Checksum: get driver [%s] => %p", driver, (void *) dr);

	return dr;
}

__attribute__((constructor))
static void checksum_init() {
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

	pthread_mutex_init(&checksum_lock, &attr);

	pthread_mutexattr_destroy(&attr);
}

int checksum_loadDriver(const char * driver) {
	if (!driver) {
		log_writeAll(Log_level_error, "Checksum: get driver failed because driver is null");
		return 3;
	}

	char path[128];
	snprintf(path, 128, "%s/lib%s.so", CHECKSUM_DIRNAME, driver);

	pthread_mutex_lock(&checksum_lock);

	// check if module is already loaded
	unsigned int i;
	for (i = 0; i < checksum_nbDrivers; i++)
		if (!strcmp(checksum_drivers[i]->name, driver)) {
			pthread_mutex_unlock(&checksum_lock);
			log_writeAll(Log_level_info, "Checksum: driver '%s' already loaded", driver);
			return 0;
		}

	// check if you can load module
	if (access(path, R_OK | X_OK)) {
		log_writeAll(Log_level_error, "Checksum: error, can load '%s' because %s", path, strerror(errno));
		pthread_mutex_unlock(&checksum_lock);
		return 1;
	}

	log_writeAll(Log_level_debug, "Checksum: loading '%s' ...", driver);

	// load module
	void * cookie = dlopen(path, RTLD_NOW);

	if (!cookie) {
		log_writeAll(Log_level_error, "Checksum: error while loading '%s' because %s", path, dlerror());
		pthread_mutex_unlock(&checksum_lock);
		return 2;
	} else if (checksum_nbDrivers > 0 && !strcmp(checksum_drivers[checksum_nbDrivers - 1]->name, driver)) {
		checksum_drivers[checksum_nbDrivers - 1]->cookie = cookie;
		log_writeAll(Log_level_info, "Checksum: driver '%s' loaded", driver);
		pthread_mutex_unlock(&checksum_lock);
		return 0;
	} else {
		// module didn't call log_registerModule so we unload it
		dlclose(cookie);
		log_writeAll(Log_level_warning, "Checksum: driver '%s' miss to call checksum_registerModule", driver);
		pthread_mutex_unlock(&checksum_lock);
		return 2;
	}
}

void checksum_registerDriver(struct checksum_driver * driver) {
	checksum_drivers = realloc(checksum_drivers, (checksum_nbDrivers + 1) * sizeof(struct checksum_driver *));

	checksum_drivers[checksum_nbDrivers] = driver;
	checksum_nbDrivers++;
}
*/
