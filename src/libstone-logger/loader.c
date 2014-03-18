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
// bool
#include <stdbool.h>
// pthread_mutex_lock, pthread_mutex_unlock,
#include <pthread.h>
// dlclose, dlerror, dlopen
#include <dlfcn.h>
// glob
#include <glob.h>
// snprintf
#include <stdio.h>
// free
#include <stdlib.h>
// access
#include <unistd.h>

#include "config.h"
#include "loader.h"

static void * lgr_loader_load_file(const char * filename);

static volatile bool lgr_loader_loading = false;
static volatile bool lgr_loader_loaded = false;


void * lgr_loader_load(const char * module, const char * name) {
	if (module == NULL || name == NULL)
		return NULL;

	char * path;
	asprintf(&path, MODULE_PATH "/lib%s-%s.so", module, name);

	void * cookie = lgr_loader_load_file(path);

	free(path);

	return cookie;
}

static void * lgr_loader_load_file(const char * filename) {
	if (access(filename, R_OK | X_OK)) {
		// lgr_log_write_all(lgr_log_level_debug, lgr_log_type_daemon, "Loader: access to file %s failed because %m", filename);
		return NULL;
	}

	static pthread_mutex_t lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
	pthread_mutex_lock(&lock);

	lgr_loader_loading = true;
	lgr_loader_loaded = false;

	void * cookie = dlopen(filename, RTLD_NOW);
	if (cookie == NULL) {
		// lgr_log_write_all(lgr_log_level_debug, lgr_log_type_daemon, "Loader: failed to load '%s' because %s", filename, dlerror());
	} else if (!lgr_loader_loaded) {
		dlclose(cookie);
		cookie = NULL;
	}

	lgr_loader_loading = false;

	pthread_mutex_unlock(&lock);
	return cookie;
}

void lgr_loader_register_ok(void) {
	if (lgr_loader_loading)
		lgr_loader_loaded = true;
}

