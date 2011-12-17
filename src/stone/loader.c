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
*  Last modified: Sat, 17 Dec 2011 17:19:43 +0100                         *
\*************************************************************************/

#define _GNU_SOURCE
// pthread_mutex_lock, pthread_mutex_unlock,
#include <pthread.h>
// dlclose, dlerror, dlopen
#include <dlfcn.h>
// glob
#include <glob.h>
// snprintf
#include <stdio.h>
// access
#include <unistd.h>

#include <stone/log.h>

#include "config.h"
#include "loader.h"

static void * sa_loader_load_file(const char * filename);

static short sa_loader_loaded = 0;


void * sa_loader_load(const char * module, const char * name) {
	if (!module || !name)
		return 0;

	char path[256];
	snprintf(path, 256, "%s/%s/lib%s.so", MODULE_PATH, module, name);

	return sa_loader_load_file(path);
}

void * sa_loader_load_file(const char * filename) {
	if (access(filename, R_OK | X_OK)) {
		return 0;
	}

	static pthread_mutex_t lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
	pthread_mutex_lock(&lock);

	sa_loader_loaded = 0;

	void * cookie = dlopen(filename, RTLD_NOW);
	if (!cookie) {
		sa_log_write_all(sa_log_level_debug, "Loader: failed to load '%s' because %s", filename, dlerror());
	} else if (!sa_loader_loaded) {
		dlclose(cookie);
		cookie = 0;
	}

	pthread_mutex_unlock(&lock);
	return cookie;
}

void sa_loader_register_ok() {
	sa_loader_loaded = 1;
}

