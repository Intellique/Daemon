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
*  Copyright (C) 2015, Guillaume Clercin <gclercin@intellique.com>           *
\****************************************************************************/

#define _GNU_SOURCE
// dgettext
#include <libintl.h>
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

#include <logger/log.h>

#include "config.h"
#include "loader.h"

static void * solgr_loader_load_file(const char * filename);

static volatile bool solgr_loader_loading = false;
static volatile bool solgr_loader_loaded = false;


void * solgr_loader_load(const char * module, const char * name) {
	if (module == NULL || name == NULL)
		return NULL;

	char * path;
	asprintf(&path, MODULE_PATH "/lib%s-%s.so", module, name);

	void * cookie = solgr_loader_load_file(path);

	free(path);

	return cookie;
}

static void * solgr_loader_load_file(const char * filename) {
	if (access(filename, R_OK | X_OK)) {
		solgr_log_write2(so_log_level_debug, so_log_type_logger, dgettext("libstoriqone-logger", "Loader: access to file '%s' failed because %m"), filename);
		return NULL;
	}

	static pthread_mutex_t lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
	pthread_mutex_lock(&lock);

	solgr_loader_loading = true;
	solgr_loader_loaded = false;

	void * cookie = dlopen(filename, RTLD_NOW);
	if (cookie == NULL) {
		solgr_log_write2(so_log_level_debug, so_log_type_logger, dgettext("libstoriqone-logger", "Loader: failed to load '%s' because %s"), filename, dlerror());
	} else if (!solgr_loader_loaded) {
		dlclose(cookie);
		cookie = NULL;
	}

	solgr_loader_loading = false;

	pthread_mutex_unlock(&lock);
	return cookie;
}

void solgr_loader_register_ok(void) {
	if (solgr_loader_loading)
		solgr_loader_loaded = true;
}

