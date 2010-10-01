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
*  Last modified: Fri, 01 Oct 2010 16:12:27 +0200                       *
\***********************************************************************/

// dlerror, dlopen
#include <dlfcn.h>
// strerror
#include <errno.h>
// free, realloc
#include <malloc.h>
// pthread_mutex_init, pthread_mutex_lock, pthread_mutex_unlock,
// pthread_mutexattr_destroy, pthread_mutexattr_init, pthread_mutexattr_settype
#define __USE_UNIX98
#include <pthread.h>
// va_end, va_start
#include <stdarg.h>
// dprintf, printf, snprintf
#include <stdio.h>
// strcasecmp, strcmp, strdup, strerror
#include <string.h>
// access
#include <unistd.h>

#include <storiqArchiver/log.h>

#include "config.h"

static struct log_module ** log_modules = 0;
static unsigned int log_nbModules = 0;
static struct log_messageUnSent {
	char * who;
	enum Log_level level;
	char * message;
} * log_messageUnSent = 0;
static unsigned int log_nbMessageUnSent = 0;
static pthread_mutex_t log_lock;

static int log_flushMessage(void);
static void log_storeMessage(char * who, enum Log_level level, char * message);


static struct log_level {
	enum Log_level level;
	const char * name;
} log_levels[] = {
	{ Log_level_debug,   "Debug" },
	{ Log_level_error,   "Error" },
	{ Log_level_info,    "Info" },
	{ Log_level_warning, "Warning" },

	{ Log_level_unknown, "Unknown" },
};


const char * log_levelToString(enum Log_level level) {
	struct log_level * ptr;
	for (ptr = log_levels; ptr->level != Log_level_unknown; ptr++)
		if (ptr->level == level)
			return ptr->name;
	return "Unknown";
}

enum Log_level log_stringTolevel(const char * string) {
	struct log_level * ptr;
	for (ptr = log_levels; ptr->level != Log_level_unknown; ptr++)
		if (!strcasecmp(ptr->name, string))
			return ptr->level;
	return Log_level_unknown;
}


__attribute__((destructor))
static void log_exit() {
	if (log_messageUnSent) {
		unsigned int mes;
		for (mes = 0; mes < log_nbMessageUnSent; mes++)
			dprintf(1, "! [%s] %s !\n", log_levelToString(log_messageUnSent[mes].level), log_messageUnSent[mes].message);
	}
}

int log_flushMessage() {
	unsigned int mes, ok = 0;
	for (mes = 0; mes < log_nbMessageUnSent; mes++) {
		unsigned int mod;
		for (mod = 0; mod < log_nbModules; mod++) {
			unsigned int submod;
			for (submod = 0; submod < log_modules[mod]->nbSubModules; submod++) {
				if (log_messageUnSent[mes].who && strcmp(log_messageUnSent[mes].who, log_modules[mod]->subModules[submod].alias))
					continue;
				if (log_modules[mod]->subModules[submod].level <= log_messageUnSent[mes].level) {
					log_modules[mod]->subModules[submod].ops->write(log_modules[mod]->subModules + submod, log_messageUnSent[mes].level, log_messageUnSent[mes].message);
					ok = 1;
				}
			}
		}
	}

	if (ok) {
		for (mes = 0; mes < log_nbMessageUnSent; mes++) {
			if (log_messageUnSent[mes].who)
				free(log_messageUnSent[mes].who);
			log_messageUnSent[mes].who = 0;
			free(log_messageUnSent[mes].message);
			log_messageUnSent[mes].message = 0;
		}
		free(log_messageUnSent);
		log_messageUnSent = 0;
	}

	return ok;
}

struct log_module * log_getModule(const char * module) {
	pthread_mutex_lock(&log_lock);

	struct log_module * mod = 0;
	unsigned int i;

	for (i = 0; i < log_nbModules && !mod; i++)
		if (!strcmp(log_modules[i]->moduleName, module))
			mod = log_modules[i];

	if (!mod && !log_loadModule(module))
		mod = log_getModule(module);

	pthread_mutex_unlock(&log_lock);

	return mod;
}

__attribute__((constructor))
static void log_init() {
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

	pthread_mutex_init(&log_lock, &attr);

	pthread_mutexattr_destroy(&attr);
}

int log_loadModule(const char * module) {
	char path[128];
	snprintf(path, 128, "%s/lib%s.so", LOG_DIRNAME, module);

	pthread_mutex_lock(&log_lock);

	// check if module is already loaded
	unsigned int i;
	for (i = 0; i < log_nbModules; i++)
		if (!strcmp(log_modules[i]->moduleName, module)) {
			log_writeAll(Log_level_info, "Log: module '%s' already loaded", module);
			pthread_mutex_unlock(&log_lock);
			return 0;
		}

	// check if you can load module
	if (access(path, R_OK | X_OK)) {
		log_writeAll(Log_level_error, "Log: error, can load '%s' because %s", path, strerror(errno));
		pthread_mutex_unlock(&log_lock);
		return 1;
	}

	// load module
	void * cookie = dlopen(path, RTLD_NOW);

	if (!cookie) {
		log_writeAll(Log_level_error, "Log: error while loading '%s' because %s", path, dlerror());
		pthread_mutex_unlock(&log_lock);
		return 2;
	} else if (log_nbModules > 0 && !strcmp(log_modules[log_nbModules - 1]->moduleName, module)) {
		log_modules[log_nbModules - 1]->cookie = cookie;
		log_writeAll(Log_level_info, "Log: module '%s' loaded", module);
		pthread_mutex_unlock(&log_lock);
		return 0;
	} else {
		// module didn't call log_registerModule so we unload it
		dlclose(cookie);
		log_writeAll(Log_level_warning, "Log: module '%s' miss to call log_registerModule", module);
		pthread_mutex_unlock(&log_lock);
		return 2;
	}
}

void log_registerModule(struct log_module * module) {
	log_modules = realloc(log_modules, (log_nbModules + 1) * sizeof(struct log_module *));

	log_modules[log_nbModules] = module;
	log_nbModules++;
}

void log_storeMessage(char * who, enum Log_level level, char * message) {
	log_messageUnSent = realloc(log_messageUnSent, (log_nbMessageUnSent + 1) * sizeof(struct log_messageUnSent));
	log_messageUnSent[log_nbMessageUnSent].who = who;
	log_messageUnSent[log_nbMessageUnSent].level = level;
	log_messageUnSent[log_nbMessageUnSent].message = message;
	log_nbMessageUnSent++;
}

void log_writeAll(enum Log_level level, const char * format, ...) {
	char * message = malloc(256);

	va_list va;
	va_start(va, format);
	vsnprintf(message, 256, format, va);
	va_end(va);

	pthread_mutex_lock(&log_lock);

	if (log_nbModules == 0 || (log_messageUnSent && !log_flushMessage())) {
		log_storeMessage(0, level, message);
		pthread_mutex_unlock(&log_lock);
		return;
	}

	unsigned int i;
	for (i = 0; i < log_nbModules; i++) {
		unsigned int j;
		for (j = 0; j < log_modules[i]->nbSubModules; j++) {
			if (log_modules[i]->subModules[j].level <= level)
				log_modules[i]->subModules[j].ops->write(log_modules[i]->subModules + j, level, message);
		}
	}

	pthread_mutex_unlock(&log_lock);

	free(message);
}

void log_writeTo(const char * alias, enum Log_level level, const char * format, ...) {
	char * message = malloc(256);

	va_list va;
	va_start(va, format);
	vsnprintf(message, 256, format, va);
	va_end(va);

	pthread_mutex_lock(&log_lock);

	if (log_nbModules == 0 || (log_messageUnSent && !log_flushMessage())) {
		log_storeMessage(strdup(alias), level, message);
		pthread_mutex_unlock(&log_lock);
		return;
	}

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

	pthread_mutex_unlock(&log_lock);

	free(message);
}

