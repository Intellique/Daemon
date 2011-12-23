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
*  Last modified: Fri, 23 Dec 2011 22:42:52 +0100                         *
\*************************************************************************/

#define _GNU_SOURCE
// pthread_mutex_lock, pthread_mutex_unlock, pthread_setcancelstate
#include <pthread.h>
// realloc
#include <stdlib.h>
// strcmp
#include <string.h>

#include <stone/database.h>
#include <stone/log.h>

#include "loader.h"

static struct st_database ** st_db_databases = 0;
static unsigned int st_db_nb_databases = 0;
static struct st_database * st_db_default_db = 0;


struct st_database * st_db_get_default_db() {
	return st_db_default_db;
}

struct st_database * st_db_get_db(const char * driver) {
	static pthread_mutex_t lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

	int old_state;
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &old_state);
	pthread_mutex_lock(&lock);

	unsigned int i;
	struct st_database * dr = 0;
	for (i = 0; i < st_db_nb_databases && !dr; i++)
		if (!strcmp(driver, st_db_databases[i]->name))
			dr = st_db_databases[i];

	void * cookie = 0;
	if (!dr)
		cookie = st_loader_load("db", driver);

	if (!dr && !cookie) {
		st_log_write_all(st_log_level_error, st_log_type_database, "Db: Failed to load driver %s", driver);
		pthread_mutex_unlock(&lock);
		if (old_state == PTHREAD_CANCEL_DISABLE)
			pthread_setcancelstate(old_state, 0);
		return 0;
	}

	for (i = 0; i < st_db_nb_databases && !dr; i++)
		if (!strcmp(driver, st_db_databases[i]->name)) {
			dr = st_db_databases[i];
			dr->cookie = cookie;
		}

	if (!dr)
		st_log_write_all(st_log_level_error, st_log_type_database, "Db: Driver %s not found", driver);

	pthread_mutex_unlock(&lock);
	if (old_state == PTHREAD_CANCEL_DISABLE)
		pthread_setcancelstate(old_state, 0);

	return dr;
}

void st_db_register_db(struct st_database * db) {
	if (!db) {
		st_log_write_all(st_log_level_error, st_log_type_database, "Db: Try to register with driver=0");
		return;
	}

	if (db->api_version != STONE_DATABASE_APIVERSION) {
		st_log_write_all(st_log_level_error, st_log_type_database, "Db: Driver(%s) has not the correct api version (current: %d, expected: %d)", db->name, db->api_version, STONE_DATABASE_APIVERSION);
		return;
	}

	st_db_databases = realloc(st_db_databases, (st_db_nb_databases + 1) * sizeof(struct st_database *));
	st_db_databases[st_db_nb_databases] = db;
	st_db_nb_databases++;

	st_loader_register_ok();

	st_log_write_all(st_log_level_info, st_log_type_database, "Db: Driver(%s) is now registred", db->name);
}

void st_db_set_default_db(struct st_database * db) {
	if (st_db_default_db)
		st_log_write_all(st_log_level_debug, st_log_type_database, "Db: set new default database from %s to %s", st_db_default_db->name, db->name);
	else
		st_log_write_all(st_log_level_debug, st_log_type_database, "Db: set new default database to %s", db->name);
	if (db)
		st_db_default_db = db;
}

