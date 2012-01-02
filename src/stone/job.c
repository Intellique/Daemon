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
*  Last modified: Fri, 30 Dec 2011 20:32:08 +0100                         *
\*************************************************************************/

#define _GNU_SOURCE
// pthread_attr_destroy, pthread_attr_init, pthread_attr_setdetachstate,
// pthread_cond_destroy, pthread_cond_init, pthread_cond_signal,
// pthread_cond_wait, pthread_create, pthread_mutex_destroy,
// pthread_mutex_init, pthread_mutex_lock, pthread_mutex_unlock,
// pthread_setcancelstate
#include <pthread.h>
// free, malloc, realloc
#include <stdlib.h>
// strcmp, strdup
#include <string.h>

#include <stone/log.h>

#include "job.h"
#include "loader.h"


static struct st_job_driver ** st_job_drivers = 0;
static unsigned int st_job_nb_drivers = 0;
static const struct st_job_status2 {
	char * name;
	enum st_job_status status;
} st_job_status[] = {
	{ "disable", st_job_status_disable },
	{ "error",   st_job_status_error },
	{ "idle",    st_job_status_idle },
	{ "pause",   st_job_status_pause },
	{ "running", st_job_status_running },

	{ "unknown", st_job_status_unknown },
};


struct st_job_driver * st_job_get_driver(const char * driver) {
	static pthread_mutex_t lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

	int old_state;
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &old_state);
	pthread_mutex_lock(&lock);

	unsigned int i;
	struct st_job_driver * dr = 0;
	for (i = 0; i < st_job_nb_drivers && !dr; i++)
		if (!strcmp(driver, st_job_drivers[i]->name))
			dr = st_job_drivers[i];

	void * cookie = 0;
	if (!dr)
		cookie = st_loader_load("job", driver);

	if (!dr && !cookie) {
		st_log_write_all(st_log_level_error, st_log_type_job, "Failed to load driver '%s'", driver);
		pthread_mutex_unlock(&lock);
		if (old_state == PTHREAD_CANCEL_DISABLE)
			pthread_setcancelstate(old_state, 0);
		return 0;
	}

	for (i = 0; i < st_job_nb_drivers && !dr; i++)
		if (!strcmp(driver, st_job_drivers[i]->name)) {
			dr = st_job_drivers[i];
			dr->cookie = cookie;
		}

	if (!dr)
		st_log_write_all(st_log_level_error, st_log_type_job, "Driver '%s' not found", driver);

	pthread_mutex_unlock(&lock);
	if (old_state == PTHREAD_CANCEL_DISABLE)
		pthread_setcancelstate(old_state, 0);

	return dr;
}

void st_job_register_driver(struct st_job_driver * driver) {
	if (!driver) {
		st_log_write_all(st_log_level_error, st_log_type_job, "Try to register with driver=0");
		return;
	}

	if (driver->api_version != STONE_JOB_APIVERSION) {
		st_log_write_all(st_log_level_error, st_log_type_job, "Driver(%s) has not the correct api version (current: %d, expected: %d)", driver->name, driver->api_version, STONE_JOB_APIVERSION);
		return;
	}

	unsigned int i;
	for (i = 0; i < st_job_nb_drivers; i++)
		if (st_job_drivers[i] == driver || !strcmp(driver->name, st_job_drivers[i]->name)) {
			st_log_write_all(st_log_level_warning, st_log_type_job, "Driver(%s) is already registred", driver->name);
			return;
		}

	st_job_drivers = realloc(st_job_drivers, (st_job_nb_drivers + 1) * sizeof(struct st_job_driver *));
	st_job_drivers[st_job_nb_drivers] = driver;
	st_job_nb_drivers++;

	st_loader_register_ok();

	st_log_write_all(st_log_level_info, st_log_type_job, "Driver(%s) is now registred", driver->name);
}

const char * st_job_status_to_string(enum st_job_status status) {
	unsigned int i;
	for (i = 0; st_job_status[i].status != st_job_status_unknown; i++)
		if (st_job_status[i].status == status)
			return st_job_status[i].name;
	return st_job_status[i].name;
}

enum st_job_status st_job_string_to_status(const char * status) {
	unsigned int i;
	for (i = 0; st_job_status[i].status != st_job_status_unknown; i++)
		if (!strcmp(status, st_job_status[i].name))
			return st_job_status[i].status;
	return st_job_status[i].status;
}

