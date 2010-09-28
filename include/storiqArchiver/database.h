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
*  Last modified: Tue, 28 Sep 2010 17:54:47 +0200                       *
\***********************************************************************/

#ifndef __STORIQARCHIVER_DATABASE_H__
#define __STORIQARCHIVER_DATABASE_H__

struct hashtable;
struct database;
struct database_connection;

struct database_ops {
	struct database_connection * (*connect)(struct database * db);
	int (*ping)(struct database * db);
	int (*setup)(struct database * db, struct hashtable * params);
};

struct database {
	char driverName[32];
	struct database_ops * ops;
	void * data;

	// reserved for server
	void * cookie;
};

struct database_connection_ops {
	void * foo;
};

struct database_connection {
	unsigned int id;
	struct database * driver;
	struct database_connection_ops * ops;
	void * data;
};


/**
 * \brief get a database driver
 * \param db : database name
 * \return 0 if failed
 * \note if \a db is not loaded then we try to load it
 */
struct database * db_getDb(const char * db);

/**
 * \brief try to load a database driver
 * \param db : database name
 * \retrun 0 if ok
 */
int db_loadDb(const char * db);

/**
 * \brief Each db module should call this function only one time
 * \param db : a statically allocated struct database
 * \code
 * __attribute__((constructor))
 * static void db_myDb_init() {
 *    db_registerDb(&db_myDb_module);
 * }
 * \endcode
 */
void db_registerDb(struct database * db);

#endif

