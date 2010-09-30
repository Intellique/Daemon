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
*  Last modified: Wed, 29 Sep 2010 11:45:21 +0200                       *
\***********************************************************************/

// dlerror, dlopen
#include <dlfcn.h>
// realloc
#include <malloc.h>
// va_end, va_start
#include <stdarg.h>
// printf, snprintf
#include <stdio.h>
// strcasecmp, strcmp
#include <string.h>
// access
#include <unistd.h>

#include <storiqArchiver/log.h>

#include "config.h"

static struct log_module ** log_modules = 0;
static unsigned int log_nbModules = 0;

static struct log_level {
	enum Log_level level;
	const char * name;
} log_levels[] = {
	{ Log_level_debug,		"Debug" },
	{ Log_level_error,		"Error" },
	{ Log_level_info,		"Info" },
	{ Log_level_warning,	"Warning" },

	{ Log_level_unknown,	"Unknown" },
};


const char * log_levelToString(enum Log_level level) {
	struct log_level * ptr;
	for (ptr = log_levels; ptr->level != Log_level_unknown; ptr++)
		if (ptr->level == level)
			return ptr->name;
	return 0;
}

enum Log_level log_stringTolevel(const char * string) {
	struct log_level * ptr;
	for (ptr = log_levels; ptr->level != Log_level_unknown; ptr++)
		if (!strcasecmp(ptr->name, string))
			return ptr->level;
	return Log_level_unknown;
}


struct log_module * log_getModule(const char * module) {
	unsigned int i;
	for (i = 0; i < log_nbModules; i++)
		if (!strcmp(log_modules[i]->moduleName, module))
			return log_modules[i];
	if (!log_loadModule(module))
		return log_getModule(module);
	return 0;
}

int log_loadModule(const char * module) {
	char path[128];
	snprintf(path, 128, "%s/lib%s.so", LOG_DIRNAME, module);

	// check if module is already loaded
	unsigned int i;
	for (i = 0; i < log_nbModules; i++)
		if (!strcmp(log_modules[i]->moduleName, module))
			return 0;

	// check if you can load module
	if (access(path, R_OK | X_OK))
		return 1;

	// load module
	void * cookie = dlopen(path, RTLD_NOW);

	if (!cookie) {
		printf("Error while loading %s => %s\n", path, dlerror());
		return 2;
	} else if (log_nbModules > 0 && !strcmp(log_modules[log_nbModules - 1]->moduleName, module)) {
		log_modules[log_nbModules - 1]->cookie = cookie;
	} else {
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

void log_writeAll(enum Log_level level, const char * format, ...) {
	char * message = malloc(256);

	va_list va;
	va_start(va, format);
	vsnprintf(message, 256, format, va);
	va_end(va);

	unsigned int i;
	for (i = 0; i < log_nbModules; i++) {
		unsigned int j;
		for (j = 0; j < log_modules[i]->nbSubModules; j++) {
			if (log_modules[i]->subModules[j].level <= level)
				log_modules[i]->subModules[j].ops->write(log_modules[i]->subModules + j, level, message);
		}
	}

	free(message);
}

void log_writeTo(const char * alias, enum Log_level level, const char * format, ...) {
	char * message = malloc(256);

	va_list va;
	va_start(va, format);
	vsnprintf(message, 256, format, va);
	va_end(va);

	unsigned int i;
	for (i = 0; i < log_nbModules; i++) {
		unsigned int j;
		for (j = 0; j < log_modules[i]->nbSubModules; j++) {
			if (!strcmp(log_modules[i]->subModules[j].alias, alias) && log_modules[i]->subModules[j].level <= level) {
				log_modules[i]->subModules[j].ops->write(log_modules[i]->subModules + j, level, message);
				return;
			}
		}
	}

	free(message);
}

