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
*  Last modified: Mon, 21 Nov 2011 13:43:18 +0100                         *
\*************************************************************************/

#ifndef __STORIQARCHIVER_DATABASE_H__
#define __STORIQARCHIVER_DATABASE_H__

#include <sys/time.h>

struct sa_database_connection;
struct sa_hashtable;
struct job;
struct library_changer;

struct sa_database {
	char * name;
	struct sa_database_ops {
		struct sa_database_connection * (*connect)(struct sa_database * db, struct sa_database_connection * connection);
		int (*ping)(struct sa_database * db);
		int (*setup)(struct sa_database * db, struct sa_hashtable * params);
	} * ops;
	void * data;

	// reserved for server
	void * cookie;
};

struct sa_database_connection {
	unsigned int id;
	struct sa_database * driver;
	struct sa_database_connection_ops {
		/**
		 * \brief close \a db connection
		 * \param db : a database connection
		 * \return a value which correspond to
		 * \li 0 if ok
		 * \li < 0 if error
		 */
		int (*close)(struct sa_database_connection * db);
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
		int (*free)(struct sa_database_connection * db);

		/**
		 * \brief rool back a transaction
		 * \param db : a database connection
		 * \li 0 if ok
		 * \li 1 if noop
		 * \li < 0 if error
		 */
		int (*cancelTransaction)(struct sa_database_connection * db);
		/**
		 * \brief finish a transaction
		 * \param db : a database connection
		 * \return a value which correspond to
		 * \li 0 if ok
		 * \li 1 if noop
		 * \li < 0 if error
		 */
		int (*finishTransaction)(struct sa_database_connection * db);
		/**
		 * \brief starts a transaction
		 * \param db : a database connection
		 * \param readOnly : is a read only transaction
		 * \return a value which correspond to
		 * \li 0 if ok
		 * \li 1 if noop
		 * \li < 0 if error
		 */
		int (*startTransaction)(struct sa_database_connection * db, short readOnly);

		struct library_changer * (*getChanger)(struct sa_database_connection * db, const char * hostName);
		int (*getNbChanger)(struct sa_database_connection * db, const char * hostName);

		/**
		 * \brief add a new job
		 * \param db : a database connection
		 * \param job : a job
		 * \param index : n new job
		 */
		struct job * (*addJob)(struct sa_database_connection * db, struct job * job, unsigned int index, time_t since);
		/**
		 * \brief checks jobs modified between \a since and \a to
		 * \param db : a database connection
		 * \param since : start time
		 * \return a value which correspond to
		 * \li > 0 is the number of modified jobs
		 * \li 0 if no changes
		 * \li < 0 if error
		 */
		int (*jobModified)(struct sa_database_connection * db, time_t since);
		/**
		 * \brief checks new jobs created between \a since and \a to
		 * \param db : a database connection
		 * \param since : start time
		 * \return a value which correspond to
		 * \li > 0 is the number of created jobs
		 * \li 0 if there is no new jobs
		 * \li < 0 if error
		 */
		int (*newJobs)(struct sa_database_connection * db, time_t since);
		/**
		 * \brief update modified jobs
		 * \param db : a database connection
		 * \param job : update this job
		 * \return a value which correspond to
		 * \li > 0 if this job has been updated
		 * \li 0 if no update
		 * \li < 0 if error
		 */
		int (*updateJob)(struct sa_database_connection * db, struct job * job);
	} * ops;
	void * data;
};


struct sa_database * sa_db_get_default_db(void);

/**
 * \brief get a database driver
 * \param db : database name
 * \return 0 if failed
 * \note if \a db is not loaded then we try to load it
 */
struct sa_database * sa_db_get_db(const char * db);

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
void sa_db_register_db(struct sa_database * db);

void sa_db_set_default_dB(struct sa_database * db);

#endif

