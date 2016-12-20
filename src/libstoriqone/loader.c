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
*  Copyright (C) 2013-2016, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#define _GNU_SOURCE
// dgettext
#include <libintl.h>
// bool
#include <stdbool.h>
// pthread_mutex_lock, pthread_mutex_unlock
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

#include <libstoriqone/log.h>

#include "loader.h"

#include "config.h"

static void * so_loader_load_file(const char * filename);

static volatile bool so_loader_loading = false;
static volatile bool so_loader_loaded = false;


void * so_loader_load(const char * module, const char * name) {
	if (module == NULL || name == NULL)
		return NULL;

	char * path = NULL;
	int size = asprintf(&path, MODULE_PATH "/lib%s-%s.so", module, name);

	if (size < 0)
		return NULL;

	void * cookie = so_loader_load_file(path);

	free(path);

	return cookie;
}

static void * so_loader_load_file(const char * filename) {
	if (access(filename, R_OK | X_OK)) {
		so_log_write(so_log_level_debug,
			dgettext("libstoriqone", "libstoriqone: loader: failed to access file '%s' because %m"),
			filename);
		return NULL;
	}

	static pthread_mutex_t lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
	pthread_mutex_lock(&lock);

	so_loader_loading = true;
	so_loader_loaded = false;

	void * cookie = dlopen(filename, RTLD_NOW);
	if (cookie == NULL) {
		so_log_write(so_log_level_debug,
			dgettext("libstoriqone", "libstoriqone: loader: failed to load '%s' because %s"),
			filename, dlerror());
	} else if (!so_loader_loaded) {
		dlclose(cookie);
		cookie = NULL;
	}

	so_loader_loading = false;

	pthread_mutex_unlock(&lock);
	return cookie;
}

void so_loader_register_ok(void) {
	if (so_loader_loading)
		so_loader_loaded = true;
}

