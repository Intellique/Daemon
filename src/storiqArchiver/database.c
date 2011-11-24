/*************************************************************************\
*        ______           ____     ___           __   _                   *
*       / __/ /____  ____/  _/__ _/ _ | ________/ /  (_)  _____ ____      *
*      _\ \/ __/ _ \/ __// // _ `/ __ |/ __/ __/ _ \/ / |/ / -_) __/      *
*     /___/\__/\___/_/ /___/\_, /_/ |_/_/  \__/_//_/_/|___/\__/_/         *
*                            /_/                                          *
*  ---------------------------------------------------------------------  *
*  This file is a part of StorIqArchiver                                  *
*                                                                         *
*  StorIqArchiver is free software; you can redistribute it and/or        *
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
*  Last modified: Thu, 24 Nov 2011 11:34:50 +0100                         *
\*************************************************************************/

// realloc
#include <stdlib.h>
// strcmp
#include <string.h>

#include <storiqArchiver/database.h>
#include <storiqArchiver/log.h>

#include "loader.h"

static struct sa_database ** sa_db_databases = 0;
static unsigned int sa_db_nb_databases = 0;
static struct sa_database * sa_db_default_db = 0;


struct sa_database * sa_db_get_default_db() {
	return sa_db_default_db;
}

struct sa_database * sa_db_get_db(const char * driver) {
	unsigned int i;
	for (i = 0; i < sa_db_nb_databases; i++)
		if (!strcmp(driver, sa_db_databases[i]->name))
			return sa_db_databases[i];
	void * cookie = sa_loader_load("db", driver);
	if (!cookie) {
		sa_log_write_all(sa_log_level_error, "Db: Failed to load driver %s", driver);
		return 0;
	}
	for (i = 0; i < sa_db_nb_databases; i++)
		if (!strcmp(driver, sa_db_databases[i]->name)) {
			struct sa_database * dr = sa_db_databases[i];
			dr->cookie = cookie;
			return dr;
		}
	sa_log_write_all(sa_log_level_error, "Db: Driver %s not found", driver);
	return 0;
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

