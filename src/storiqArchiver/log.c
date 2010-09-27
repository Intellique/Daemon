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
*  Copyright (C) 2010, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Mon, 27 Sep 2010 18:31:04 +0200                       *
\***********************************************************************/

// dlopen
#include <dlfcn.h>
// realloc
#include <malloc.h>
// snprintf
#include <stdio.h>
// strcmp
#include <string.h>
// access
#include <unistd.h>

#include "config.h"
#include "storiqArchiver/log.h"

static struct log_module ** log_modules = 0;
static unsigned int log_nbModules = 0;


int log_loadModule(const char * module) {
	char path[128];
	snprintf(path, 128, "%s/lib%s.so", LOG_DIRNAME, module);

	// check if module is already loaded
	unsigned int i;
	for (i = 0; i < log_nbModules; i++) {
		if (!strcmp(log_modules[i]->moduleName, module))
			return 0;
	}

	// check if you can load module
	if (access(path, R_OK | X_OK))
		return 1;

	// load module
	void * cookie = dlopen(path, RTLD_NOW);

	if (log_nbModules > 0 && !strcmp(log_modules[log_nbModules - 1]->moduleName, module))
		log_modules[log_nbModules - 1]->cookie = cookie;
	else {
		// module didn't call log_registerModule so we unload it
		dlclose(cookie);
		return 2;
	}

	return 0;
}

void log_registerModule(struct log_module * module) {
	log_modules = realloc(log_modules, (log_nbModules + 1) * sizeof(struct log_module *));

	log_modules[log_nbModules] = module;
	log_nbModules++;
}

