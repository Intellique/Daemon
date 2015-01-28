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
*  Copyright (C) 2013-2015, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Thu, 16 Jan 2014 15:56:22 +0100                            *
\****************************************************************************/

#define _GNU_SOURCE
// glob, globfree
#include <glob.h>
// pthread_attr_destroy, pthread_attr_init, pthread_attr_setdetachstate,
// pthread_cond_destroy, pthread_cond_init, pthread_cond_signal,
// pthread_cond_wait, pthread_create, pthread_mutex_destroy,
// pthread_mutex_init, pthread_mutex_lock, pthread_mutex_unlock,
// pthread_setcancelstate
#include <pthread.h>
// va_end, va_start
#include <stdarg.h>
// free, malloc, realloc
#include <stdlib.h>
// globfree
#include <stdio.h>
// strcasecmp, strcmp, strdup
#include <string.h>

#include <libstone/database.h>
#include <libstone/job.h>
#include <libstone/log.h>

#include "config.h"
#include "loader.h"


static struct st_job_driver ** st_job_drivers = NULL;
static unsigned int st_job_nb_drivers = 0;
static const struct st_job_status2 {
	char * name;
	enum st_job_status status;
} st_job_status[] = {
	{ "disable",   st_job_status_disable },
	{ "error",     st_job_status_error },
	{ "finished",  st_job_status_finished },
	{ "pause",     st_job_status_pause },
	{ "running",   st_job_status_running },
	{ "scheduled", st_job_status_scheduled },
	{ "stopped",   st_job_status_stopped },
	{ "waiting",   st_job_status_waiting },

	{ "unknown", st_job_status_unknown },
};

static void st_job_exit(void) __attribute__((destructor));


void st_job_add_record(struct st_database_connection * connect, enum st_log_level level, struct st_job * job, enum st_job_record_notif notif, const char * format, ...) {
	char * message = NULL;

	va_list va;
	va_start(va, format);
	vasprintf(&message, format, va);
	va_end(va);

	st_log_write_all2(level, st_log_type_job, job->user, message);

	connect->ops->add_job_record(connect, job, message, notif);

	free(message);
}

static void st_job_exit() {
	free(st_job_drivers);
	st_job_drivers = NULL;
	st_job_nb_drivers = 0;
}

struct st_job_driver * st_job_get_driver(const char * driver) {
	if (!driver) {
		st_log_write_all(st_log_level_error, st_log_type_job, "Failed to load driver '%s'", driver);
		return NULL;
	}

	static pthread_mutex_t lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
	pthread_mutex_lock(&lock);

	unsigned int i;
	struct st_job_driver * dr = NULL;
	for (i = 0; i < st_job_nb_drivers && dr == NULL; i++)
		if (!strcmp(driver, st_job_drivers[i]->name))
			dr = st_job_drivers[i];

	if (dr == NULL) {
		void * cookie = st_loader_load("job", driver);

		if (cookie == NULL) {
			st_log_write_all(st_log_level_error, st_log_type_job, "Failed to load driver '%s'", driver);
			pthread_mutex_unlock(&lock);
			return NULL;
		}

		for (i = 0; i < st_job_nb_drivers && dr == NULL; i++)
			if (!strcmp(driver, st_job_drivers[i]->name)) {
				dr = st_job_drivers[i];
				dr->cookie = cookie;

				st_log_write_all(st_log_level_debug, st_log_type_checksum, "Driver '%s' is now registred, src checksum: %s", driver, dr->src_checksum);
			}

		if (dr == NULL)
			st_log_write_all(st_log_level_error, st_log_type_job, "Driver '%s' not found", driver);
	}

	pthread_mutex_unlock(&lock);

	return dr;
}

void st_job_register_driver(struct st_job_driver * driver) {
	if (driver == NULL) {
		st_log_write_all(st_log_level_error, st_log_type_job, "Try to register with driver is NULL");
		return;
	}

	if (st_plugin_check(&driver->api_level) == false) {
		st_log_write_all(st_log_level_error, st_log_type_job, "Driver '%s' has not the correct api version", driver->name);
		return;
	}

	unsigned int i;
	for (i = 0; i < st_job_nb_drivers; i++)
		if (st_job_drivers[i] == driver || !strcmp(driver->name, st_job_drivers[i]->name)) {
			st_log_write_all(st_log_level_warning, st_log_type_job, "Driver '%s' is already registred", driver->name);
			return;
		}

	void * new_addr = realloc(st_job_drivers, (st_job_nb_drivers + 1) * sizeof(struct st_job_driver *));
	if (new_addr == NULL) {
		st_log_write_all(st_log_level_error, st_log_type_checksum, "Driver '%s' cannot be registred because there is not enough memory", driver->name);
		return;
	}

	st_job_drivers = new_addr;
	st_job_drivers[st_job_nb_drivers] = driver;
	st_job_nb_drivers++;

	st_loader_register_ok();

	st_log_write_all(st_log_level_info, st_log_type_job, "Driver '%s' is now registred", driver->name);
}

const char * st_job_status_to_string(enum st_job_status status) {
	unsigned int i;
	for (i = 0; st_job_status[i].status != st_job_status_unknown; i++)
		if (st_job_status[i].status == status)
			return st_job_status[i].name;

	return st_job_status[i].name;
}

enum st_job_status st_job_string_to_status(const char * status) {
	if (status == NULL)
		return st_job_status_unknown;

	unsigned int i;
	for (i = 0; st_job_status[i].status != st_job_status_unknown; i++)
		if (!strcasecmp(status, st_job_status[i].name))
			return st_job_status[i].status;

	return st_job_status[i].status;
}

void st_job_sync_plugins(struct st_database_connection * connection) {
	if (connection == NULL)
		return;

	glob_t gl;
	glob(MODULE_PATH "/libjob-*.so", 0, NULL, &gl);

	unsigned int i;
	for (i = 0; i < gl.gl_pathc; i++) {
		char * ptr = strrchr(gl.gl_pathv[i], '/') + 1;

		char plugin[64];
		sscanf(ptr, "libjob-%63[^.].so", plugin);

		if (connection->ops->sync_plugin_job(connection, plugin))
			st_log_write_all(st_log_level_error, st_log_type_job, "Failed to synchronize plugin '%s'", plugin);
	}

	globfree(&gl);
}

