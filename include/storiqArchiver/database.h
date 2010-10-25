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
*  Last modified: Mon, 25 Oct 2010 16:23:29 +0200                       *
\***********************************************************************/

#ifndef __STORIQARCHIVER_DATABASE_H__
#define __STORIQARCHIVER_DATABASE_H__

#include <sys/time.h>

struct database;
struct database_connection;
struct hashtable;
struct job;
struct library_changer;

struct database_ops {
	struct database_connection * (*connect)(struct database * db, struct database_connection * connection);
	int (*ping)(struct database * db);
	int (*setup)(struct database * db, struct hashtable * params);
};

struct database {
	char * driverName;
	struct database_ops * ops;
	void * data;

	// reserved for server
	void * cookie;
};

struct database_connection_ops {
	/**
	 * \brief close \a db connection
	 * \param db : a database connection
	 * \return a value which correspond to
	 * \li 0 if ok
	 * \li < 0 if error
	 */
	int (*close)(struct database_connection * db);
	/**
	 * \brief free memory associated with database connection
	 * \param db : a database connection
	 * \return a value which correspond to
	 * \li 0 if ok
	 * \li < 0 if error
	 * \warning implementation of this function SHOULD NOT call
	 * \code
	 * free(db);
	 * \endcode
	 */
	int (*free)(struct database_connection * db);

	/**
	 * \brief rool back a transaction
	 * \param db : a database connection
	 * \li 0 if ok
	 * \li 1 if noop
	 * \li < 0 if error
	 */
	int (*cancelTransaction)(struct database_connection * db);
	/**
	 * \brief finish a transaction
	 * \param db : a database connection
	 * \return a value which correspond to
	 * \li 0 if ok
	 * \li 1 if noop
	 * \li < 0 if error
	 */
	int (*finishTransaction)(struct database_connection * db);
	/**
	 * \brief starts a transaction
	 * \param db : a database connection
	 * \param readOnly : is a read only transaction
	 * \return a value which correspond to
	 * \li 0 if ok
	 * \li 1 if noop
	 * \li < 0 if error
	 */
	int (*startTransaction)(struct database_connection * db, short readOnly);

	struct library_changer * (*getChanger)(struct database_connection * db, struct library_changer * changer, int index);
	int (*getNbChanger)(struct database_connection * db);

	/**
	 * \brief add a new job
	 * \param db : a database connection
	 * \param job : a job
	 * \param index : n new job
	 */
	struct job * (*addJob)(struct database_connection * db, struct job * job, unsigned int index, time_t since);
	/**
	 * \brief checks jobs modified between \a since and \a to
	 * \param db : a database connection
	 * \param since : start time
	 * \return a value which correspond to
	 * \li > 0 is the number of modified jobs
	 * \li 0 if no changes
	 * \li < 0 if error
	 */
	int (*jobModified)(struct database_connection * db, time_t since);
	/**
	 * \brief checks new jobs created between \a since and \a to
	 * \param db : a database connection
	 * \param since : start time
	 * \return a value which correspond to
	 * \li > 0 is the number of created jobs
	 * \li 0 if there is no new jobs
	 * \li < 0 if error
	 */
	int (*newJobs)(struct database_connection * db, time_t since);
	/**
	 * \brief update modified jobs
	 * \param db : a database connection
	 * \param job : update this job
	 * \return a value which correspond to
	 * \li > 0 if this job has been updated
	 * \li 0 if no update
	 * \li < 0 if error
	 */
	int (*updateJob)(struct database_connection * db, struct job * job);
};

struct database_connection {
	unsigned int id;
	struct database * driver;
	struct database_connection_ops * ops;
	void * data;
};


struct database * db_getDefaultDB(void);

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
 * \return a value which correspond to
 * \li 0 if ok
 * \li 1 if permission error
 * \li 2 if module didn't call db_registerDb
 * \li 3 if db is null
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

void db_setDefaultDB(struct database * db);

#endif

