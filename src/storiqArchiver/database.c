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
*  Last modified: Tue, 28 Sep 2010 17:04:09 +0200                       *
\***********************************************************************/

// dlerror, dlopen
#include <dlfcn.h>
// realloc
#include <malloc.h>
// strcmp
#include <string.h>
// access
#include <unistd.h>

#include <storiqArchiver/database.h>

#include "config.h"

static struct database ** db_databases = 0;
static unsigned int db_nbDatabases = 0;


struct database * db_getDb(const char * db) {
	unsigned int i;
	for (i = 0; i < db_nbDatabases; i++)
		if (!strcmp(db_databases[i]->driverName, db))
			return db_databases[i];
	if (!db_loadDb(db))
		return db_getDb(db);
	return 0;
}

int db_loadDb(const char * db) {
	char path[128];
	snprintf(path, 128, "%s/lib%s.so", DB_DIRNAME, db);

	// check if db is already loaded
	unsigned int i;
	for (i = 0; i < db_nbDatabases; i++)
		if (!strcmp(db_databases[i]->driverName, db))
			return 0;

	// check if you can db
	if (access(path, R_OK | X_OK))
		return 1;

	// load db
	void * cookie = dlopen(path, RTLD_NOW);

	if (!cookie) {
		printf("Error while loading %s => %s\n", path, dlerror());
		return 2;
	} else if (db_nbDatabases > 0 && !strcmp(db_databases[db_nbDatabases - 1]->driverName, db)) {
		db_databases[db_nbDatabases - 1]->cookie = cookie;
	} else {
		// module didn't call log_registerModule so we unload it
		dlclose(cookie);
		return 2;
	}

	return 0;
}

void db_registerDb(struct database * db) {
	db_databases = realloc(db_databases, (db_nbDatabases + 1) * sizeof(struct database *));

	db_databases[db_nbDatabases] = db;
	db_nbDatabases++;
}

