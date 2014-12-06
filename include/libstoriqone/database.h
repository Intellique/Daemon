/****************************************************************************\
*                    ______           _      ____                            *
*                   / __/ /____  ____(_)__ _/ __ \___  ___                   *
*                  _\ \/ __/ _ \/ __/ / _ `/ /_/ / _ \/ -_)                  *
*                 /___/\__/\___/_/ /_/\_, /\____/_//_/\__/                   *
*                                      /_/                                   *
*  ------------------------------------------------------------------------  *
*  This file is a part of Storiq One                                         *
*                                                                            *
*  Storiq One is free software; you can redistribute it and/or modify        *
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

#ifndef __LIBSTORIQONE_DATABASE_H__
#define __LIBSTORIQONE_DATABASE_H__

// bool
#include <stdbool.h>

struct so_changer;
struct so_checksum_driver;
struct so_database;
struct so_database_config;
struct so_drive;
struct so_host;
struct so_job;
enum so_job_record_notif;
enum so_log_level;
struct so_media;
struct so_media_format;
enum so_media_format_mode;
struct so_pool;
enum so_script_type;
struct so_value;

enum so_database_sync_method {
	so_database_sync_default,
	so_database_sync_init,
	so_database_sync_id_only,
};

/**
 * \struct so_database_connection
 * \brief A database connection
 */
struct so_database_connection {
	/**
	 * \struct so_database_connection_ops
	 * \brief Operations on a database connection
	 *
	 * \var ops
	 * \brief Operations
	 */
	struct so_database_connection_ops {
		/**
		 * \brief close \a db connection
		 *
		 * \param[in] db a database connection
		 * \return a value which correspond to
		 * \li 0 if ok
		 * \li < 0 if error
		 */
		int (*close)(struct so_database_connection * connect) __attribute__((nonnull));
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
		int (*free)(struct so_database_connection * connect) __attribute__((nonnull));
		/**
		 * \brief check if the connection to database is closed
		 *
		 * \param[in] db a database connection
		 * \return 0 if the connection is not closed
		 */
		bool (*is_connected)(struct so_database_connection * connect) __attribute__((nonnull));

		/**
		 * \brief Rool back a transaction
		 *
		 * \param[in] db a database connection
		 * \li 0 if ok
		 * \li 1 if noop
		 * \li < 0 if error
		 */
		int (*cancel_transaction)(struct so_database_connection * connect) __attribute__((nonnull));
		/**
		 * \brief Finish a transaction
		 *
		 * \param[in] db a database connection
		 * \return a value which correspond to
		 * \li 0 if ok
		 * \li 1 if noop
		 * \li < 0 if error
		 */
		int (*finish_transaction)(struct so_database_connection * connect) __attribute__((nonnull));
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
		int (*start_transaction)(struct so_database_connection * connect) __attribute__((nonnull));

		int (*add_host)(struct so_database_connection * connect, const char * uuid, const char * name, const char * domaine, const char * description) __attribute__((nonnull(1,2,3)));
		bool (*find_host)(struct so_database_connection * connect, const char * uuid, const char * hostname) __attribute__((nonnull(1)));
		int (*get_host_by_name)(struct so_database_connection * connect, struct so_host * host, const char * name) __attribute__((nonnull));
		int (*update_host)(struct so_database_connection * connect, const char * uuid) __attribute__((nonnull));

		struct so_value * (*get_changers)(struct so_database_connection * connect) __attribute__((nonnull));
		struct so_value * (*get_checksums_from_pool)(struct so_database_connection * connect, struct so_pool * pool) __attribute__((nonnull,warn_unused_result));
		struct so_value * (*get_free_medias)(struct so_database_connection * connect, struct so_media_format * media_format, bool online) __attribute__((nonnull,warn_unused_result));
		struct so_media * (*get_media)(struct so_database_connection * connect, const char * medium_serial_number, const char * label, struct so_job * job) __attribute__((nonnull(1),warn_unused_result));
		struct so_value * (*get_medias_of_pool)(struct so_database_connection * connect, struct so_pool * pool) __attribute__((nonnull,warn_unused_result));
		struct so_media_format * (*get_media_format)(struct so_database_connection * connect, unsigned int density_code, enum so_media_format_mode mode);
		struct so_pool * (*get_pool)(struct so_database_connection * connect, const char * uuid, struct so_job * job) __attribute__((nonnull(1),warn_unused_result));;
		struct so_value * (*get_standalone_drives)(struct so_database_connection * connect) __attribute__((nonnull));
		struct so_value * (*get_vtls)(struct so_database_connection * connect) __attribute__((nonnull));
		int (*sync_changer)(struct so_database_connection * connect, struct so_changer * changer, enum so_database_sync_method method) __attribute__((nonnull));
		int (*sync_drive)(struct so_database_connection * connect, struct so_drive * drive, bool sync_media, enum so_database_sync_method method) __attribute__((nonnull));
		int (*sync_media)(struct so_database_connection * connnect, struct so_media * media, enum so_database_sync_method method) __attribute__((nonnull));

		int (*add_job_record)(struct so_database_connection * connect, struct so_job * job, enum so_log_level level, enum so_job_record_notif notif, const char * message);
		int (*start_job)(struct so_database_connection * connect, struct so_job * job) __attribute__((nonnull));
		int (*stop_job)(struct so_database_connection * connect, struct so_job * job) __attribute__((nonnull));
		int (*sync_job)(struct so_database_connection * connect, struct so_job * job) __attribute__((nonnull));
		int (*sync_jobs)(struct so_database_connection * connect, struct so_value * jobs) __attribute__((nonnull));

		int (*get_nb_scripts)(struct so_database_connection * connect, const char * job_type, enum so_script_type type, struct so_pool * pool);
		char * (*get_script)(struct so_database_connection * connect, const char * job_type, unsigned int sequence, enum so_script_type type, struct so_pool * pool);

		bool (*find_plugin_checksum)(struct so_database_connection * connect, const char * checksum) __attribute__((nonnull));
		int (*sync_plugin_checksum)(struct so_database_connection * connect, struct so_checksum_driver * driver) __attribute__((nonnull));
		int (*sync_plugin_job)(struct so_database_connection * connect, const char * job) __attribute__((nonnull));
	} * ops;

	/**
	 * \brief private data
	 */
	void * data;
	/**
	 * \brief Reference to a database driver
	 */
	struct so_database * driver;
	/**
	 * \brief Reference to a database configuration
	 */
	struct so_database_config * config;
};

/**
 * \struct so_database_config
 * \brief Describe a database configuration
 */
struct so_database_config {
	/**
	 * \brief name of config
	 */
	char * name;

	/**
	 * \struct so_database_config_ops
	 *
	 * \var ops
	 * \brief Database operations which require a configuration
	 */
	struct so_database_config_ops {
		/**
		 * \brief Backup
		 *
		 * \returns a stream reader which contains data of database
		 */
		struct so_stream_reader * (*backup_db)(struct so_database_config * db_config);
		/**
		 * \brief Create a new connection to database
		 *
		 * \returns a database connection
		 */
		struct so_database_connection * (*connect)(struct so_database_config * db_config) __attribute__((nonnull,warn_unused_result));
		/**
		 * \brief Check if database is online
		 */
		int (*ping)(struct so_database_config * db_config) __attribute__((nonnull));
	} * ops;

	/**
	 * \brief private data
	 */
	void * data;
	/**
	 * \brief Reference to a database driver
	 */
	struct so_database * driver;
};

/**
 * \struct so_database
 * \brief Database driver
 *
 * A unique instance by type of database
 */
struct so_database {
	/**
	 * \brief Name of driver
	 *
	 * \note Should be unique and equals to libdb-name.so where name is the name of driver.
	 */
	const char * name;
	/**
	 * \struct so_database_ops
	 * \brief Operations on one database driver
	 *
	 * \var ops
	 * \brief Database operation
	 */
	struct so_database_ops {
		/**
		 * \brief Add configure to this database driver
		 *
		 * \param[in] params hashtable which contains parameters
		 * \returns \b 0 if ok
		 */
		struct so_database_config * (*add)(struct so_value * params) __attribute__((nonnull));
		/**
		 * \brief Get default database configuration
		 */
		struct so_database_config * (*get_default_config)(void);
	} * ops;

	/**
	 * \brief Collection of database configurations
	 */
	struct so_value * configurations;

	/**
	 * \brief cookie
	 *
	 * Is a value returns by dlopen and should not be used nor released
	 */
	void * cookie;
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
struct so_database * so_database_get_default_driver(void);

/**
 * \brief get a database driver
 *
 * \param[in] database database name
 * \return NULL if failed
 *
 * \note if this driver is not loaded, this function will load it
 * \warning the returned value <b>SHALL NOT BE RELEASE</b> with \a free
 */
struct so_database * so_database_get_driver(const char * driver) __attribute__((nonnull));

void so_database_load_config(struct so_value * config) __attribute__((nonnull));

/**
 * \brief Register a database driver
 *
 * \param[in] database a statically allocated struct so_database
 *
 * \note Each database driver should call this function only one time
 * \code
 * static void database_myDb_init() __attribute__((constructor)) {
 *    so_database_register_driver(&database_myDb_module);
 * }
 * \endcode
 */
void so_database_register_driver(struct so_database * driver) __attribute__((nonnull));

#endif

