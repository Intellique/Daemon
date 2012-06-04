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
*  Copyright (C) 2012, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Mon, 04 Jun 2012 12:04:03 +0200                         *
\*************************************************************************/

#ifndef __STONE_DATABASE_H__
#define __STONE_DATABASE_H__

// time_t
#include <sys/time.h>

struct st_archive;
struct st_archive_file;
struct st_archive_volume;
struct st_changer;
struct st_database_connection;
struct st_drive;
struct st_job;
struct st_hashtable;
struct st_pool;
struct st_stream_reader;
struct st_tape;
struct st_tape_format;
struct st_user;

struct st_database {
	char * name;
	struct st_database_ops {
		struct st_stream_reader * (*backup_db)(struct st_database * db);
		struct st_database_connection * (*connect)(struct st_database * db, struct st_database_connection * connection);
		int (*ping)(struct st_database * db);
		int (*setup)(struct st_database * db, struct st_hashtable * params);
	} * ops;
	void * data;

	// used by server only
	// should not be modified
	void * cookie;
	int api_version;
};

/**
 * \brief Current api version
 *
 * Will increment from new version of struct st_database or struct st_database_connection
 */
#define STONE_DATABASE_APIVERSION 1

struct st_database_connection {
	long id;
	struct st_database * driver;
	struct st_database_connection_ops {
		/**
		 * \brief close \a db connection
		 * \param db : a database connection
		 * \return a value which correspond to
		 * \li 0 if ok
		 * \li < 0 if error
		 */
		int (*close)(struct st_database_connection * db);
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
		int (*free)(struct st_database_connection * db);

		/**
		 * \brief rool back a transaction
		 * \param db : a database connection
		 * \li 0 if ok
		 * \li 1 if noop
		 * \li < 0 if error
		 */
		int (*cancel_transaction)(struct st_database_connection * db);
		/**
		 * \brief finish a transaction
		 * \param db : a database connection
		 * \return a value which correspond to
		 * \li 0 if ok
		 * \li 1 if noop
		 * \li < 0 if error
		 */
		int (*finish_transaction)(struct st_database_connection * db);
		/**
		 * \brief starts a transaction
		 * \param db : a database connection
		 * \param readOnly : is a read only transaction
		 * \return a value which correspond to
		 * \li 0 if ok
		 * \li 1 if noop
		 * \li < 0 if error
		 */
		int (*start_transaction)(struct st_database_connection * db);

		int (*add_job_record)(struct st_database_connection * db, struct st_job * job, const char * message);
		int (*create_pool)(struct st_database_connection * db, struct st_pool * pool);
		int (*get_nb_new_jobs)(struct st_database_connection * db, long * nb_new_jobs, time_t since, long last_max_jobs);
		int (*get_new_jobs)(struct st_database_connection * db, struct st_job ** jobs, unsigned int nb_jobs, time_t since, long last_max_jobs);
		int (*get_pool)(struct st_database_connection * db, struct st_pool * pool, long id, const char * uuid);
		int (*get_tape)(struct st_database_connection * db, struct st_tape * tape, long id, const char * uuid, const char * label);
		int (*get_tape_format)(struct st_database_connection * db, struct st_tape_format * tape_format, long id, unsigned char density_code);
		int (*get_user)(struct st_database_connection * db, struct st_user * user, long user_id, const char * login);
		int (*is_changer_contain_drive)(struct st_database_connection * db, struct st_changer * changer, struct st_drive * drive);
		int (*refresh_job)(struct st_database_connection * db, struct st_job * job);
		int (*sync_changer)(struct st_database_connection * db, struct st_changer * changer);
		int (*sync_drive)(struct st_database_connection * db, struct st_drive * drive);
		int (*sync_plugin_checksum)(struct st_database_connection * db, const char * plugin);
		int (*sync_plugin_job)(struct st_database_connection * db, const char * plugin);
		int (*sync_pool)(struct st_database_connection * db, struct st_pool * pool);
		int (*sync_tape)(struct st_database_connection * db, struct st_tape * tape);
		int (*sync_user)(struct st_database_connection * db, struct st_user * user);
		int (*update_job)(struct st_database_connection * db, struct st_job * job);

		int (*file_add_checksum)(struct st_database_connection * db, struct st_archive_file * file);
		int (*file_link_to_volume)(struct st_database_connection * db, struct st_archive_file * file, struct st_archive_volume * volume);
		int (*new_archive)(struct st_database_connection * db, struct st_archive * archive);
		int (*new_file)(struct st_database_connection * db, struct st_archive_file * file);
		int (*new_volume)(struct st_database_connection * db, struct st_archive_volume * volume);
		int (*update_archive)(struct st_database_connection * db, struct st_archive * archive);
		int (*update_volume)(struct st_database_connection * db, struct st_archive_volume * volume);
	} * ops;
	void * data;
};


struct st_database * st_db_get_default_db(void);

/**
 * \brief get a database driver
 * \param db : database name
 * \return 0 if failed
 * \note if \a db is not loaded then we try to load it
 */
struct st_database * st_db_get_db(const char * db);

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
void st_db_register_db(struct st_database * db);

void st_db_set_default_db(struct st_database * db);

#endif

