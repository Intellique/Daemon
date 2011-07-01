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
*  Last modified: Fri, 01 Jul 2011 14:51:34 +0200                       *
\***********************************************************************/

// va_end, va_start
#include <stdarg.h>
// realloc
#include <stdlib.h>
// snprintf
#include <stdio.h>
// strcasecmp, strcmp
#include <string.h>

#include <storiqArchiver/log.h>

#include "loader.h"

static int _sa_log_flushMessage(void);
static void _sa_log_storeMessage(char * who, enum sa_log_level level, char * message);

static struct sa_log_driver ** _sa_log_drivers = 0;
static unsigned int _sa_log_nb_drivers = 0;
static struct _sa_log_message_unsent {
	char * who;
	enum sa_log_level level;
	char * message;
	char sent;
} * _sa_log_message_unsent = 0;
static unsigned int _sa_log_nb_message_unsent = 0;
static unsigned int _sa_log_nb_message_unsent_to = 0;

static struct _sa_log_level {
	enum sa_log_level level;
	const char * name;
} _sa_log_levels[] = {
	{ sa_log_level_debug,   "Debug" },
	{ sa_log_level_error,   "Error" },
	{ sa_log_level_info,    "Info" },
	{ sa_log_level_warning, "Warning" },

	{ sa_log_level_unknown, "Unknown" },
};


int _sa_log_flushMessage() {
	unsigned int mes, ok = 0;
	for (mes = 0; mes < _sa_log_nb_message_unsent; mes++) {
		unsigned int dr;
		for (dr = 0; dr < _sa_log_nb_drivers; dr++) {
			unsigned int mod;
			for (mod = 0; mod < _sa_log_drivers[dr]->nbModules; mod++) {
				if (_sa_log_message_unsent[mes].who && strcmp(_sa_log_message_unsent[mes].who, _sa_log_drivers[dr]->modules[mod].alias))
					continue;
				if (_sa_log_drivers[dr]->modules[mod].level <= _sa_log_message_unsent[mes].level) {
					_sa_log_drivers[dr]->modules[mod].ops->write(_sa_log_drivers[dr]->modules + mod, _sa_log_message_unsent[mes].level, _sa_log_message_unsent[mes].message);
					_sa_log_message_unsent[mes].sent = 1;
					ok = 1;
				}
			}
		}
	}

	if (ok) {
		for (mes = 0; mes < _sa_log_nb_message_unsent; mes++) {
			if (_sa_log_message_unsent[mes].sent) {
				if (_sa_log_message_unsent[mes].who) {
					free(_sa_log_message_unsent[mes].who);
					_sa_log_nb_message_unsent_to--;
				}
				if (_sa_log_message_unsent[mes].message)
					free(_sa_log_message_unsent[mes].message);

				if (mes + 1 < _sa_log_nb_message_unsent)
					memmove(_sa_log_message_unsent + mes, _sa_log_message_unsent + mes + 1, (_sa_log_nb_message_unsent - mes - 1) * sizeof(struct _sa_log_message_unsent));
				_sa_log_nb_message_unsent--, mes--;
				if (_sa_log_nb_message_unsent > 0)
					_sa_log_message_unsent = realloc(_sa_log_message_unsent, _sa_log_nb_message_unsent * sizeof(struct _sa_log_message_unsent));
			}
		}

		if (_sa_log_nb_message_unsent == 0) {
			free(_sa_log_message_unsent);
			_sa_log_message_unsent = 0;
			ok = 2;
		}
	}

	return ok;
}

struct sa_log_driver * sa_log_get_driver(const char * driver) {
	unsigned int i;
	for (i = 0; i < _sa_log_nb_drivers; i++)
		if (!strcmp(driver, _sa_log_drivers[i]->name))
			return _sa_log_drivers[i];
	if (sa_loader_load("log", driver))
		return 0;
	for (i = 0; i < _sa_log_nb_drivers; i++)
		if (!strcmp(driver, _sa_log_drivers[i]->name))
			return _sa_log_drivers[i];
	return 0;
}

const char * sa_log_level_to_string(enum sa_log_level level) {
	struct _sa_log_level * ptr;
	for (ptr = _sa_log_levels; ptr->level != sa_log_level_unknown; ptr++)
		if (ptr->level == level)
			return ptr->name;
	return "Unknown";
}

void sa_log_register_driver(struct sa_log_driver * driver) {
	if (!driver)
		return;

	_sa_log_drivers = realloc(_sa_log_drivers, (_sa_log_nb_drivers + 1) * sizeof(struct sa_log_driver *));
	_sa_log_drivers[_sa_log_nb_drivers] = driver;
	_sa_log_nb_drivers++;

	sa_loader_register_ok();
}

void _sa_log_storeMessage(char * who, enum sa_log_level level, char * message) {
	_sa_log_message_unsent = realloc(_sa_log_message_unsent, (_sa_log_nb_message_unsent + 1) * sizeof(struct _sa_log_message_unsent));
	_sa_log_message_unsent[_sa_log_nb_message_unsent].who = who;
	_sa_log_message_unsent[_sa_log_nb_message_unsent].level = level;
	_sa_log_message_unsent[_sa_log_nb_message_unsent].message = message;
	_sa_log_message_unsent[_sa_log_nb_message_unsent].sent = 0;
	_sa_log_nb_message_unsent++;
	if (who)
		_sa_log_nb_message_unsent_to++;
}

enum sa_log_level sa_log_string_to_level(const char * string) {
	struct _sa_log_level * ptr;
	for (ptr = _sa_log_levels; ptr->level != sa_log_level_unknown; ptr++)
		if (!strcasecmp(ptr->name, string))
			return ptr->level;
	return sa_log_level_unknown;
}

void sa_log_write_all(enum sa_log_level level, const char * format, ...) {
	char * message = malloc(256);

	va_list va;
	va_start(va, format);
	vsnprintf(message, 256, format, va);
	va_end(va);

	if (_sa_log_nb_drivers == 0 || (_sa_log_message_unsent && !_sa_log_flushMessage())) {
		_sa_log_storeMessage(0, level, message);
		return;
	}
}


/*

// dlclose, dlerror, dlopen
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
// printf, snprintf, vsnprintf
#include <stdio.h>
// memmove, strcasecmp, strcmp, strdup, strerror
#include <string.h>
// access
#include <unistd.h>

static struct log_module ** log_modules = 0;
static unsigned int log_nbModules = 0;
static pthread_mutex_t log_lock;



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
					log_messageUnSent[mes].sent = 1;
					ok = 1;
				}
			}
		}
	}

	if (ok) {
		for (mes = 0; mes < log_nbMessageUnSent; mes++) {
			if (log_messageUnSent[mes].sent) {
				if (log_messageUnSent[mes].who) {
					free(log_messageUnSent[mes].who);
					log_nbMessageUnSentTo--;
				}
				if (log_messageUnSent[mes].message)
					free(log_messageUnSent[mes].message);

				if (mes + 1 < log_nbMessageUnSent)
					memmove(log_messageUnSent + mes, log_messageUnSent + mes + 1, (log_nbMessageUnSent - mes - 1) * sizeof(struct log_messageUnSent));
				log_nbMessageUnSent--, mes--;
				if (log_nbMessageUnSent > 0)
					log_messageUnSent = realloc(log_messageUnSent, log_nbMessageUnSent * sizeof(struct log_messageUnSent));
			}
		}

		if (log_nbMessageUnSent == 0) {
			free(log_messageUnSent);
			log_messageUnSent = 0;
			ok = 2;
		}
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
	if (!module) {
		log_writeAll(Log_level_debug, "Log: get driver failed because driver is null");
		return 3;
	}

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

	log_writeAll(Log_level_debug, "Log: loading '%s' ...", module);

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
	if (!module)
		return;

	log_modules = realloc(log_modules, (log_nbModules + 1) * sizeof(struct log_module *));

	log_modules[log_nbModules] = module;
	log_nbModules++;
}

void log_storeMessage(char * who, enum Log_level level, char * message) {
	log_messageUnSent = realloc(log_messageUnSent, (log_nbMessageUnSent + 1) * sizeof(struct log_messageUnSent));
	log_messageUnSent[log_nbMessageUnSent].who = who;
	log_messageUnSent[log_nbMessageUnSent].level = level;
	log_messageUnSent[log_nbMessageUnSent].message = message;
	log_messageUnSent[log_nbMessageUnSent].sent = 0;
	log_nbMessageUnSent++;
	if (who)
		log_nbMessageUnSentTo++;
}

void log_writeAll(enum Log_level level, const char * format, ...) {
	char * message = malloc(256);

	va_list va;
	va_start(va, format);
	vsnprintf(message, 256, format, va);
	va_end(va);

	pthread_mutex_lock(&log_lock);

	if (log_nbModules == 0 || (log_nbMessageUnSent > log_nbMessageUnSentTo && !log_flushMessage())) {
		log_storeMessage(0, level, message);
		pthread_mutex_unlock(&log_lock);
		return;
	}

	unsigned int i, sent = 0;
	for (i = 0; i < log_nbModules; i++) {
		unsigned int j;
		for (j = 0; j < log_modules[i]->nbSubModules; j++) {
			if (log_modules[i]->subModules[j].level <= level) {
				log_modules[i]->subModules[j].ops->write(log_modules[i]->subModules + j, level, message);
				sent = 1;
			}
		}
	}

	if (sent == 0)
		log_storeMessage(0, level, message);

	pthread_mutex_unlock(&log_lock);

	if (sent)
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

	unsigned int i, sent = 0;
	for (i = 0; i < log_nbModules; i++) {
		unsigned int j;
		for (j = 0; j < log_modules[i]->nbSubModules; j++) {
			if (!strcmp(log_modules[i]->subModules[j].alias, alias) && log_modules[i]->subModules[j].level <= level) {
				log_modules[i]->subModules[j].ops->write(log_modules[i]->subModules + j, level, message);
				sent = 1;
			}
		}
	}

	if (sent == 0)
		log_storeMessage(0, level, message);

	pthread_mutex_unlock(&log_lock);

	if (sent)
		free(message);
}

*/
