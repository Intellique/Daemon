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
*  Last modified: Thu, 24 Feb 2011 09:15:16 +0100                       *
\***********************************************************************/

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

#include "config.h"

#include <storiqArchiver/io.h>
#include <storiqArchiver/log.h>

static struct stream_io_driver ** io_drivers = 0;
static unsigned int io_nbDrivers = 0;
static pthread_mutex_t io_lock;


struct stream_io_driver * io_getDriver(const char * driver) {
	if (!driver) {
		log_writeAll(Log_level_error, "IO: get driver failed because driver is null");
		return 0;
	}

	pthread_mutex_lock(&io_lock);

	struct stream_io_driver * dr = 0;
	unsigned int i;

	for (i = 0; i < io_nbDrivers && !dr; i++)
		if (!strcmp(io_drivers[i]->name, driver))
			dr = io_drivers[i];

	if (!dr && !io_loadDriver(driver))
		dr = io_getDriver(driver);

	pthread_mutex_unlock(&io_lock);

	log_writeAll(Log_level_debug, "IO: get driver [%s] => %p", driver, (void *) dr);

	return dr;
}

__attribute__((constructor))
static void io_init() {
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

	pthread_mutex_init(&io_lock, &attr);

	pthread_mutexattr_destroy(&attr);
}

int io_loadDriver(const char * driver) {
	if (!driver) {
		log_writeAll(Log_level_error, "IO: get driver failed because driver is null");
		return 3;
	}

	char path[128];
	snprintf(path, 128, "%s/lib%s.so", IO_DIRNAME, driver);

	pthread_mutex_lock(&io_lock);

	// check if module is already loaded
	unsigned int i;
	for (i = 0; i < io_nbDrivers; i++)
		if (!strcmp(io_drivers[i]->name, driver)) {
			pthread_mutex_unlock(&io_lock);
			log_writeAll(Log_level_info, "Checksum: driver '%s' already loaded", driver);
			return 0;
		}

	// check if you can load module
	if (access(path, R_OK | X_OK)) {
		log_writeAll(Log_level_error, "Checksum: error, can load '%s' because %s", path, strerror(errno));
		pthread_mutex_unlock(&io_lock);
		return 1;
	}

	log_writeAll(Log_level_debug, "IO: loading '%s' ...", driver);

	// load module
	void * cookie = dlopen(path, RTLD_NOW);

	if (!cookie) {
		log_writeAll(Log_level_error, "IO: error while loading '%s' because %s", path, dlerror());
		pthread_mutex_unlock(&io_lock);
		return 2;
	} else if (io_nbDrivers > 0 && !strcmp(io_drivers[io_nbDrivers - 1]->name, driver)) {
		io_drivers[io_nbDrivers - 1]->cookie = cookie;
		log_writeAll(Log_level_info, "IO: driver '%s' loaded", driver);
		pthread_mutex_unlock(&io_lock);
		return 0;
	} else {
		// module didn't call log_registerModule so we unload it
		dlclose(cookie);
		log_writeAll(Log_level_warning, "IO: driver '%s' miss to call checksum_registerModule", driver);
		pthread_mutex_unlock(&io_lock);
		return 2;
	}
}

void io_registerDriver(struct stream_io_driver * driver) {
	io_drivers = realloc(io_drivers, (io_nbDrivers + 1) * sizeof(struct io_driver *));

	io_drivers[io_nbDrivers] = driver;
	io_nbDrivers++;
}

