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
// gettext
#include <libintl.h>
// bool
#include <stdbool.h>
// pthread_mutex_lock, pthread_mutex_unlock,
#include <pthread.h>
// dlclose, dlerror, dlopen
#include <dlfcn.h>
// glob
#include <glob.h>
// asprintf, snprintf
#include <stdio.h>
// free
#include <stdlib.h>
// access
#include <unistd.h>

#include <libstone/log.h>

#include "config.h"
#include "loader.h"

static void * st_loader_load_file(const char * filename);

static bool st_loader_loading = false;
static bool st_loader_loaded = false;


void * st_loader_load(const char * module, const char * name) {
	if (module == NULL || name == NULL)
		return NULL;

	char * path;
	asprintf(&path, MODULE_PATH "/lib%s-%s.so", module, name);

	void * cookie = st_loader_load_file(path);

	free(path);

	return cookie;
}

static void * st_loader_load_file(const char * filename) {
	if (access(filename, R_OK | X_OK)) {
		st_log_write(st_log_level_debug, gettext("libstone: loader: failed to access to file '%s' because %m"), filename);
		return NULL;
	}

	static pthread_mutex_t lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
	pthread_mutex_lock(&lock);

	st_loader_loading = true;
	st_loader_loaded = false;

	void * cookie = dlopen(filename, RTLD_NOW);
	if (cookie == NULL) {
		st_log_write(st_log_level_debug, gettext("libstone: loader: failed to load '%s' because %s"), filename, dlerror());
	} else if (!st_loader_loaded) {
		dlclose(cookie);
		cookie = NULL;
	}

	st_loader_loading = false;

	pthread_mutex_unlock(&lock);
	return cookie;
}

void st_loader_register_ok(void) {
	if (st_loader_loading)
		st_loader_loaded = true;
}

