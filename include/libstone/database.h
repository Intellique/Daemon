/****************************************************************************\
*                             __________                                     *
*                            / __/_  __/__  ___  ___                         *
*                           _\ \  / / / _ \/ _ \/ -_)                        *
*                          /___/ /_/  \___/_//_/\__/                         *
*                                                                            *
*  ------------------------------------------------------------------------  *
*  This file is a part of STone                                              *
*                                                                            *
*  STone is free software; you can redistribute it and/or modify             *
*  it under the terms of the GNU Affero General Public License               *
*  as published by the Free Software Foundation; either version 3            *
*  of the License, or (at your option) any later version.                    *
*                                                                            *
*  This program is distributed in the hope that it will be useful,           *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of            *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
*  GNU Affero General Public License for more details.                       *
*                                                                            *
*  You should have received a copy of the GNU Affero General Public License  *
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.     *
*                                                                            *
*  ------------------------------------------------------------------------  *
*  Copyright (C) 2014, Clercin guillaume <gclercin@intellique.com>           *
\****************************************************************************/

#ifndef __LIBSTONE_DATABASE_H__
#define __LIBSTONE_DATABASE_H__

// bool
#include <stdbool.h>

struct st_changer;
struct st_database;
struct st_database_config;
struct st_drive;
struct st_host;
struct st_job;
enum st_job_record_notif;
enum st_log_level;
struct st_media;
enum st_media_format_mode;
enum st_script_type;
struct st_value;

enum st_database_sync_method {
	st_database_sync_default,
	st_database_sync_init,
	st_database_sync_id_only,
};

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
		 * \param[in] db a database connection
		 * \return a value which correspond to
		 * \li 0 if ok
		 * \li < 0 if error
		 */
		int (*close)(struct st_database_connection * connect) __attribute__((nonnull));
		/**
		 * \brief free memory associated with database connection
		 *
		 * \param[in] db a database connection
		 * \return a value which correspond to
		 * \li 0 if ok
		 * \li < 0 if error
		 *
		 * \warning Implementation of this function SHOULD NOT call
		 * \code
		 * free(db);
		 * \endcode
		 */
		int (*free)(struct st_database_connection * connect) __attribute__((nonnull));
		/**
		 * \brief check if the connection to database is closed
		 *
		 * \param[in] db a database connection
		 * \return 0 if the connection is not closed
		 */
		bool (*is_connected)(struct st_database_connection * connect) __attribute__((nonnull));

		/**
		 * \brief Rool back a transaction
		 *
		 * \param[in] db a database connection
		 * \li 0 if ok
		 * \li 1 if noop
		 * \li < 0 if error
		 */
		int (*cancel_transaction)(struct st_database_connection * connect) __attribute__((nonnull));
		/**
		 * \brief Finish a transaction
		 *
		 * \param[in] db a database connection
		 * \return a value which correspond to
		 * \li 0 if ok
		 * \li 1 if noop
		 * \li < 0 if error
		 */
		int (*finish_transaction)(struct st_database_connection * connect) __attribute__((nonnull));
		/**
		 * \brief Starts a transaction
		 *
		 * \param[in] connection a database connection
		 * \param[in] readOnly is a read only transaction
		 * \return a value which correspond to
		 * \li 0 if ok
		 * \li 1 if noop
		 * \li < 0 if error
		 */
		int (*start_transaction)(struct st_database_connection * connect) __attribute__((nonnull));

		int (*add_host)(struct st_database_connection * connect, const char * uuid, const char * name, const char * domaine, const char * description) __attribute__((nonnull(1,2,3)));
		bool (*find_host)(struct st_database_connection * connect, const char * uuid, const char * hostname) __attribute__((nonnull(1)));
		int (*get_host_by_name)(struct st_database_connection * connect, struct st_host * host, const char * name) __attribute__((nonnull));

		struct st_value * (*get_changers)(struct st_database_connection * connect) __attribute__((nonnull));
		struct st_media * (*get_media)(struct st_database_connection * connect, const char * medium_serial_number, const char * label, struct st_job * job) __attribute__((nonnull(1),warn_unused_result));
		struct st_media_format * (*get_media_format)(struct st_database_connection * connect, unsigned int density_code, enum st_media_format_mode mode);
		struct st_pool * (*get_pool)(struct st_database_connection * connect, const char * uuid, struct st_job * job);
		struct st_value * (*get_standalone_drives)(struct st_database_connection * connect) __attribute__((nonnull));
		struct st_value * (*get_vtls)(struct st_database_connection * connect) __attribute__((nonnull));
		int (*sync_changer)(struct st_database_connection * connect, struct st_changer * changer, enum st_database_sync_method method) __attribute__((nonnull));
		int (*sync_drive)(struct st_database_connection * connect, struct st_drive * drive, enum st_database_sync_method method) __attribute__((nonnull));
		int (*sync_media)(struct st_database_connection * connnect, struct st_media * media, enum st_database_sync_method method) __attribute__((nonnull));

		int (*add_job_record)(struct st_database_connection * connect, struct st_job * job, enum st_log_level level, enum st_job_record_notif notif, const char * message);
		int (*start_job)(struct st_database_connection * connect, struct st_job * job) __attribute__((nonnull));
		int (*stop_job)(struct st_database_connection * connect, struct st_job * job) __attribute__((nonnull));
		int (*sync_job)(struct st_database_connection * connect, struct st_job * job) __attribute__((nonnull));
		int (*sync_jobs)(struct st_database_connection * connect, struct st_value * jobs) __attribute__((nonnull));

		int (*get_nb_scripts)(struct st_database_connection * connect, const char * job_type, enum st_script_type type, struct st_pool * pool);
		char * (*get_script)(struct st_database_connection * connect, const char * job_type, unsigned int sequence, enum st_script_type type, struct st_pool * pool);

		int (*sync_plugin_job)(struct st_database_connection * connect, const char * job) __attribute__((nonnull));
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
		 * \brief Create a new connection to database
		 *
		 * \returns a database connection
		 */
		struct st_database_connection * (*connect)(struct st_database_config * db_config) __attribute__((nonnull,warn_unused_result));
		/**
		 * \brief Check if database is online
		 */
		int (*ping)(struct st_database_config * db_config) __attribute__((nonnull));
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
		 * \param[in] params hashtable which contains parameters
		 * \returns \b 0 if ok
		 */
		struct st_database_config * (*add)(struct st_value * params) __attribute__((nonnull));
		/**
		 * \brief Get default database configuration
		 */
		struct st_database_config * (*get_default_config)(void);
	} * ops;

	/**
	 * \brief Collection of database configurations
	 */
	struct st_value * configurations;

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
	unsigned int api_level;
	/**
	 * \brief Sha1 sum of plugins source code
	 */
	const char * src_checksum;
};

/**
 * \brief get the default database driver
 *
 * \return NULL if failed
 */
struct st_database * st_database_get_default_driver(void);

/**
 * \brief get a database driver
 *
 * \param[in] database database name
 * \return NULL if failed
 *
 * \note if this driver is not loaded, this function will load it
 * \warning the returned value <b>SHALL NOT BE RELEASE</b> with \a free
 */
struct st_database * st_database_get_driver(const char * driver) __attribute__((nonnull));

void st_database_load_config(struct st_value * config) __attribute__((nonnull));

/**
 * \brief Register a database driver
 *
 * \param[in] database a statically allocated struct st_database
 *
 * \note Each database driver should call this function only one time
 * \code
 * static void database_myDb_init() __attribute__((constructor)) {
 *    st_database_register_driver(&database_myDb_module);
 * }
 * \endcode
 */
void st_database_register_driver(struct st_database * driver) __attribute__((nonnull));

#endif

