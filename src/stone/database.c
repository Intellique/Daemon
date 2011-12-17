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
*  Last modified: Sat, 17 Dec 2011 17:31:44 +0100                         *
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

static struct sa_database ** sa_db_databases = 0;
static unsigned int sa_db_nb_databases = 0;
static struct sa_database * sa_db_default_db = 0;


struct sa_database * sa_db_get_default_db() {
	return sa_db_default_db;
}

struct sa_database * sa_db_get_db(const char * driver) {
	static pthread_mutex_t lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

	int old_state;
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &old_state);
	pthread_mutex_lock(&lock);

	unsigned int i;
	struct sa_database * dr = 0;
	for (i = 0; i < sa_db_nb_databases && !dr; i++)
		if (!strcmp(driver, sa_db_databases[i]->name))
			dr = sa_db_databases[i];

	void * cookie = 0;
	if (!dr)
		cookie = sa_loader_load("db", driver);

	if (!dr && !cookie) {
		sa_log_write_all(sa_log_level_error, "Db: Failed to load driver %s", driver);
		pthread_mutex_unlock(&lock);
		if (old_state == PTHREAD_CANCEL_DISABLE)
			pthread_setcancelstate(old_state, 0);
		return 0;
	}

	for (i = 0; i < sa_db_nb_databases && !dr; i++)
		if (!strcmp(driver, sa_db_databases[i]->name)) {
			dr = sa_db_databases[i];
			dr->cookie = cookie;
		}

	if (!dr)
		sa_log_write_all(sa_log_level_error, "Db: Driver %s not found", driver);

	pthread_mutex_unlock(&lock);
	if (old_state == PTHREAD_CANCEL_DISABLE)
		pthread_setcancelstate(old_state, 0);

	return dr;
}

void sa_db_register_db(struct sa_database * db) {
	if (!db) {
		sa_log_write_all(sa_log_level_error, "Db: Try to register with driver=0");
		return;
	}

	if (db->api_version != STORIQARCHIVER_DATABASE_APIVERSION) {
		sa_log_write_all(sa_log_level_error, "Db: Driver(%s) has not the correct api version (current: %d, expected: %d)", db->name, db->api_version, STORIQARCHIVER_DATABASE_APIVERSION);
		return;
	}

	sa_db_databases = realloc(sa_db_databases, (sa_db_nb_databases + 1) * sizeof(struct sa_database *));
	sa_db_databases[sa_db_nb_databases] = db;
	sa_db_nb_databases++;

	sa_loader_register_ok();

	sa_log_write_all(sa_log_level_info, "Db: Driver(%s) is now registred", db->name);
}

void sa_db_set_default_db(struct sa_database * db) {
	if (sa_db_default_db)
		sa_log_write_all(sa_log_level_debug, "Db: set new default database from %s to %s", sa_db_default_db->name, db->name);
	else
		sa_log_write_all(sa_log_level_debug, "Db: set new default database to %s", db->name);
	if (db)
		sa_db_default_db = db;
}

