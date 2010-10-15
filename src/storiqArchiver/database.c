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
*  Last modified: Fri, 01 Oct 2010 16:35:20 +0200                       *
\***********************************************************************/

// dlerror, dlopen
#include <dlfcn.h>
// strerror
#include <errno.h>
// realloc
#include <malloc.h>
// pthread_mutex_init, pthread_mutex_lock, pthread_mutex_unlock,
// pthread_mutexattr_destroy, pthread_mutexattr_init, pthread_mutexattr_settype
#define __USE_UNIX98
#include <pthread.h>
// strcmp
#include <string.h>
// access
#include <unistd.h>

#include <storiqArchiver/database.h>
#include <storiqArchiver/log.h>

#include "config.h"

static struct database ** db_databases = 0;
static unsigned int db_nbDatabases = 0;
static struct database * db_defaultDb = 0;
static pthread_mutex_t db_lock;


struct database * db_getDefaultDB() {
	return db_defaultDb;
}

struct database * db_getDb(const char * db) {
	if (!db) {
		log_writeAll(Log_level_error, "Db, getDb: db is null");
		return 0;
	}

	pthread_mutex_lock(&db_lock);

	unsigned int i;
	struct database * data = 0;

	for (i = 0; i < db_nbDatabases && !data; i++)
		if (!strcmp(db_databases[i]->driverName, db))
			data = db_databases[i];

	if (!data && !db_loadDb(db))
		data = db_getDb(db);

	pthread_mutex_unlock(&db_lock);

	return data;
}

__attribute__((constructor))
static void db_init() {
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

	pthread_mutex_init(&db_lock, &attr);

	pthread_mutexattr_destroy(&attr);
}

int db_loadDb(const char * db) {
	if (!db) {
		log_writeAll(Log_level_error, "Db, loadDb: db is null");
		return 3;
	}

	char path[128];
	snprintf(path, 128, "%s/lib%s.so", DB_DIRNAME, db);

	pthread_mutex_lock(&db_lock);

	// check if db is already loaded
	unsigned int i;
	for (i = 0; i < db_nbDatabases; i++)
		if (!strcmp(db_databases[i]->driverName, db)) {
			pthread_mutex_unlock(&db_lock);
			log_writeAll(Log_level_info, "Database: module '%s' already loaded", db);
			return 0;
		}

	// check if you can db
	if (access(path, R_OK | X_OK)) {
		pthread_mutex_unlock(&db_lock);
		log_writeAll(Log_level_error, "Database: error, can load '%s' because %s", path, strerror(errno));
		return 1;
	}

	log_writeAll(Log_level_debug, "Database: loading '%s' ...", db);

	// load db
	void * cookie = dlopen(path, RTLD_NOW);

	if (!cookie) {
		log_writeAll(Log_level_error, "Database: error while loading '%s' because %s", path, dlerror());
		pthread_mutex_unlock(&db_lock);
		return 2;
	} else if (db_nbDatabases > 0 && !strcmp(db_databases[db_nbDatabases - 1]->driverName, db)) {
		db_databases[db_nbDatabases - 1]->cookie = cookie;
		pthread_mutex_unlock(&db_lock);
		log_writeAll(Log_level_info, "Database: module '%s' loaded", db);
		return 0;
	} else {
		// module didn't call log_registerModule so we unload it
		dlclose(cookie);
		pthread_mutex_unlock(&db_lock);
		log_writeAll(Log_level_warning, "Database: module '%s' miss to call db_registerModule", db);
		return 2;
	}
}

void db_registerDb(struct database * db) {
	db_databases = realloc(db_databases, (db_nbDatabases + 1) * sizeof(struct database *));

	db_databases[db_nbDatabases] = db;
	db_nbDatabases++;
}

void db_setDefaultDB(struct database * db) {
	db_defaultDb = db;
	log_writeAll(Log_level_debug, "Database: set default database to '%s'", db->driverName);
}

