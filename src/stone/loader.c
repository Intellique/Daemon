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
*  Last modified: Sun, 15 Jan 2012 19:11:30 +0100                         *
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

static void * st_loader_load_file(const char * filename);

static short st_loader_loaded = 0;


void * st_loader_load(const char * module, const char * name) {
	if (!module || !name)
		return 0;

	char path[256];
	snprintf(path, 256, "%s/lib%s-%s.so", MODULE_PATH, module, name);

	return st_loader_load_file(path);
}

void * st_loader_load_file(const char * filename) {
	if (access(filename, R_OK | X_OK)) {
		return 0;
	}

	static pthread_mutex_t lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
	pthread_mutex_lock(&lock);

	st_loader_loaded = 0;

	void * cookie = dlopen(filename, RTLD_NOW);
	if (!cookie) {
		st_log_write_all(st_log_level_debug, st_log_type_daemon, "Loader: failed to load '%s' because %s", filename, dlerror());
	} else if (!st_loader_loaded) {
		dlclose(cookie);
		cookie = 0;
	}

	pthread_mutex_unlock(&lock);
	return cookie;
}

void st_loader_register_ok() {
	st_loader_loaded = 1;
}

