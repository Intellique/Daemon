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
*  Last modified: Sun, 25 Nov 2012 15:18:48 +0100                         *
\*************************************************************************/

#ifndef __STONE_DATABASE_H__
#define __STONE_DATABASE_H__

// ssize_t
#include <sys/types.h>

#include "plugin.h"

struct st_changer;
struct st_drive;
struct st_hashtable;
struct st_job;
struct st_job_selected_path;
struct st_media;
struct st_media_format;
enum st_media_format_mode;
struct st_pool;
struct st_stream_reader;
struct st_user;


/**
 * \struct st_database_connection
 * \brief A database connection
 */
struct st_database_connection {
	/**
	 * \struct st_database_connection_ops
	 * \brief Operations on a database connection
	 *
	 * \var ops
	 * \brief Operations
	 */
	struct st_database_connection_ops {
		/**
		 * \brief close \a db connection
		 *
		 * \param[in] db : a database connection
		 * \return a value which correspond to
		 * \li 0 if ok
		 * \li < 0 if error
		 */
		int (*close)(struct st_database_connection * connect);
		/**
		 * \brief free memory associated with database connection
		 *
		 * \param[in] db : a database connection
		 * \return a value which correspond to
		 * \li 0 if ok
		 * \li < 0 if error
		 *
		 * \warning Implementation of this function SHOULD NOT call
		 * \code
		 * free(db);
		 * \endcode
		 */
		int (*free)(struct st_database_connection * connect);
		/**
		 * \brief check if the connection to database is closed
		 *
		 * \param[in] db : a database connection
		 * \return 0 if the connection is not closed
		 */
		int (*is_connection_closed)(struct st_database_connection * connect);

		/**
		 * \brief Rool back a transaction
		 *
		 * \param[in] db : a database connection
		 * \li 0 if ok
		 * \li 1 if noop
		 * \li < 0 if error
		 */
		int (*cancel_transaction)(struct st_database_connection * connect);
		/**
		 * \brief Finish a transaction
		 *
		 * \param[in] db : a database connection
		 * \return a value which correspond to
		 * \li 0 if ok
		 * \li 1 if noop
		 * \li < 0 if error
		 */
		int (*finish_transaction)(struct st_database_connection * connect);
		/**
		 * \brief Starts a transaction
		 *
		 * \param[in] connection : a database connection
		 * \param[in] readOnly : is a read only transaction
		 * \return a value which correspond to
		 * \li 0 if ok
		 * \li 1 if noop
		 * \li < 0 if error
		 */
		int (*start_transaction)(struct st_database_connection * connect);

		/**
		 * \brief Synchronise checksum plugin with database
		 *
		 * \param[in] connection : a database connection
		 * \param[in] name : name of plugin
		 * \returns 0 if ok
		 */
		int (*sync_plugin_checksum)(struct st_database_connection * connect, const char * name);
		/**
		 * \brief Synchronise job plugin with database
		 *
		 * \param[in] connection : a database connection
		 * \param[in] name : name of plugin
		 * \returns 0 if ok
		 */
		int (*sync_plugin_job)(struct st_database_connection * connect, const char * plugin);

		/**
		 * \brief Check if \a drive is a part of \a changer
		 *
		 * \param[in] connect : a database connection
		 * \param[in] changer : a \a changer
		 * \param[in] drive : a \a drive
		 * \returns 1 if \a true
		 */
		int (*is_changer_contain_drive)(struct st_database_connection * connect, struct st_changer * changer, struct st_drive * drive);
		/**
		 * \brief Synchronize \a changer with database
		 *
		 * \param[in] connect : a database connection
		 * \param[in] changer : a \a changer
		 * \returns 0 if OK
		 */
		int (*sync_changer)(struct st_database_connection * connect, struct st_changer * changer);
		/**
		 * \brief Synchronize \a drive with database
		 *
		 * \param[in] connect : a database connection
		 * \param[in] drive : a \a drive
		 * \returns 0 if OK
		 */
		int (*sync_drive)(struct st_database_connection * connect, struct st_drive * drive);

		ssize_t (*get_available_size_of_offline_media_from_pool)(struct st_database_connection * connect, struct st_pool * pool);
		struct st_media * (*get_media)(struct st_database_connection * connect, struct st_job * job, const char * uuid, const char * medium_serial_number, const char * label);
		int (*get_media_format)(struct st_database_connection * connect, struct st_media_format * media_format, unsigned char density_code, enum st_media_format_mode mode);
		struct st_pool * (*get_pool)(struct st_database_connection * connect, struct st_job * job, const char * uuid);

		int (*add_job_record)(struct st_database_connection * connect, struct st_job * job, const char * message);
		struct st_job_selected_path * (*get_selected_paths)(struct st_database_connection * connect, struct st_job * job, unsigned int * nb_paths);
		int (*sync_job)(struct st_database_connection * connect, struct st_job *** jobs, unsigned int * nb_jobs);

		int (*get_user)(struct st_database_connection * connect, struct st_user * user, const char * login);
		int (*sync_user)(struct st_database_connection * connect, struct st_user * user);



		/*
		int (*create_pool)(struct st_database_connection * db, struct st_pool * pool);
		int (*get_nb_new_jobs)(struct st_database_connection * db, long * nb_new_jobs, time_t since, long last_max_jobs);
		int (*get_new_jobs)(struct st_database_connection * db, struct st_job ** jobs, unsigned int nb_jobs, time_t since, long last_max_jobs);
		int (*get_pool)(struct st_database_connection * db, struct st_pool * pool, long id, const char * uuid);
		int (*get_tape)(struct st_database_connection * db, struct st_tape * tape, long id, const char * uuid, const char * label);
		int (*get_tape_format)(struct st_database_connection * db, struct st_tape_format * tape_format, long id, unsigned char density_code);
		int (*get_user)(struct st_database_connection * db, struct st_user * user, long user_id, const char * login);
		int (*refresh_job)(struct st_database_connection * db, struct st_job * job);
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
		*/
	} * ops;

	/**
	 * \brief private data
	 */
	void * data;
	/**
	 * \brief Reference to a database driver
	 */
	struct st_database * driver;
	/**
	 * \brief Reference to a database configuration
	 */
	struct st_database_config * config;
};

/**
 * \struct st_database_config
 * \brief Describe a database configuration
 */
struct st_database_config {
	/**
	 * \brief name of config
	 */
	char * name;

	/**
	 * \struct st_database_config_ops
	 *
	 * \var ops
	 * \brief Database operations which require a configuration
	 */
	struct st_database_config_ops {
		/**
		 * \brief Backup
		 *
		 * \returns a stream reader which contains data of database
		 */
		struct st_stream_reader * (*backup_db)(struct st_database_config * db_config);
		/**
		 * \brief Create a new connection to database
		 *
		 * \returns a database connection
		 */
		struct st_database_connection * (*connect)(struct st_database_config * db_config);
		/**
		 * \brief Check if database is online
		 */
		int (*ping)(struct st_database_config * db_config);
	} * ops;

	/**
	 * \brief private data
	 */
	void * data;
	/**
	 * \brief Reference to a database driver
	 */
	struct st_database * driver;
};

/**
 * \struct st_database
 * \brief Database driver
 *
 * A unique instance by type of database
 */
struct st_database {
	/**
	 * \brief Name of driver
	 *
	 * \note Should be unique and equals to libdb-name.so where name is the name of driver.
	 */
	const char * name;
	/**
	 * \struct st_database_ops
	 * \brief Operations on one database driver
	 *
	 * \var ops
	 * \brief Database operation
	 */
	struct st_database_ops {
		/**
		 * \brief Add configure to this database driver
		 *
		 * \param[in] params : hashtable which contains parameters
		 * \returns \b 0 if ok
		 */
		struct st_database_config * (*add)(const struct st_hashtable * params);
		/**
		 * \brief Get default database configuration
		 */
		struct st_database_config * (*get_default_config)(void);
	} * ops;

	/**
	 * \brief Collection of database configurations
	 */
	struct st_database_config * configurations;
	/**
	 * \brief Numbers of configs associated to this driver
	 */
	unsigned int nb_configurations;

	/**
	 * \brief cookie
	 *
	 * Is a value returns by dlopen and should not be used nor released
	 */
	void * cookie;
	/**
	 * \brief api level supported by this driver
	 *
	 * Should be define by using STONE_DATABASE_API_LEVEL only
	 */
	const struct st_plugin api_level;
};

/**
 * \def STONE_DATABASE_API_LEVEL
 * \brief Current api level
 *
 * Will increment with new version of struct st_database or struct st_database_connection
 */
#define STONE_DATABASE_API_LEVEL 1


/**
 * \brief Get a database configuration by his name
 *
 * This configuration should be previous loaded.
 *
 * \param[in] name : name of config
 * \returns \b NULL if not found
 */
struct st_database_config * st_database_get_config_by_name(const char * name);

/**
 * \brief get the default database driver
 *
 * \return 0 if failed
 *
 * \note if \a database is not loaded then we try to load it
 */
struct st_database * st_database_get_default_driver(void);

/**
 * \brief get a database driver
 *
 * \param[in] database : database name
 * \return 0 if failed
 *
 * \note if \a database is not loaded then we try to load it
 */
struct st_database * st_database_get_driver(const char * database);

/**
 * \brief Each database driver should call this function only one time
 *
 * \param[in] database : a statically allocated struct st_database
 *
 * \code
 * \_\_attribute\_\_((constructor))
 * static void database_myDb_init() {
 *    st_database_register_driver(&database_myDb_module);
 * }
 * \endcode
 */
void st_database_register_driver(struct st_database * database);

#endif

