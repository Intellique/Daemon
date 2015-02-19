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
*  Copyright (C) 2013-2015, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Thu, 19 Feb 2015 11:56:41 +0100                            *
\****************************************************************************/

#ifndef __STONE_DATABASE_H__
#define __STONE_DATABASE_H__

// ssize_t
#include <sys/types.h>

#include "plugin.h"

struct st_archive;
struct st_archive_volume;
struct st_archive_file;
struct st_backup;
struct st_changer;
struct st_drive;
struct st_host;
struct st_hashtable;
struct st_job;
enum st_job_record_notif;
struct st_job_selected_path;
struct st_media;
struct st_media_format;
enum st_media_format_mode;
struct st_pool;
enum st_script_type;
struct st_stream_reader;
struct st_user;
struct st_vtl_config;


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
		int (*close)(struct st_database_connection * connect);
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
		int (*free)(struct st_database_connection * connect);
		/**
		 * \brief check if the connection to database is closed
		 *
		 * \param[in] db a database connection
		 * \return 0 if the connection is not closed
		 */
		int (*is_connection_closed)(struct st_database_connection * connect);

		int (*add_backup)(struct st_database_connection * connect, struct st_backup * backup);
		/**
		 * \brief Rool back a transaction
		 *
		 * \param[in] db a database connection
		 * \li 0 if ok
		 * \li 1 if noop
		 * \li < 0 if error
		 */
		int (*cancel_transaction)(struct st_database_connection * connect);
		/**
		 * \brief Finish a transaction
		 *
		 * \param[in] db a database connection
		 * \return a value which correspond to
		 * \li 0 if ok
		 * \li 1 if noop
		 * \li < 0 if error
		 */
		int (*finish_transaction)(struct st_database_connection * connect);
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
		int (*start_transaction)(struct st_database_connection * connect);

		/**
		 * \brief Synchronise checksum plugin with database
		 *
		 * \param[in] connection a database connection
		 * \param[in] name name of plugin
		 * \returns 0 if ok
		 */
		int (*sync_plugin_checksum)(struct st_database_connection * connect, const char * name);
		/**
		 * \brief Synchronise job plugin with database
		 *
		 * \param[in] connection a database connection
		 * \param[in] name name of plugin
		 * \returns 0 if ok
		 */
		int (*sync_plugin_job)(struct st_database_connection * connect, const char * plugin);

		int (*add_host)(struct st_database_connection * connect, const char * uuid, const char * name, const char * domaine, const char * description);
		bool (*find_host)(struct st_database_connection * connect, const char * uuid, const char * hostname);
		int (*get_host_by_name)(struct st_database_connection * connect, const char * name, struct st_host * host);
		int (*update_host_timestamp)(struct st_database_connection * connect);

		int (*get_nb_scripts)(struct st_database_connection * connect, const char * job_type, enum st_script_type type, struct st_pool * pool);
		char * (*get_script)(struct st_database_connection * connect, const char * job_type, unsigned int sequence, enum st_script_type type, struct st_pool * pool);
		int (*sync_script)(struct st_database_connection * connect, const char * script_path);

		/**
		 * \brief Check if \a changer is enable
		 *
		 * \param[in] connect a database connection
		 * \param[in] changer a \a changer
		 * \return \a true if enabled
		 */
		bool (*changer_is_enabled)(struct st_database_connection * connect, struct st_changer * changer);
		/**
		 * \brief Check if \a drive is enable
		 *
		 * \param[in] connect a database connection
		 * \param[in] drive a \a drive
		 * \return \a true if enabled
		 */
		bool (*drive_is_enabled)(struct st_database_connection * connect, struct st_drive * drive);
		/**
		 * \brief Check if \a drive is a part of \a changer
		 *
		 * \param[in] connect a database connection
		 * \param[in] changer a \a changer
		 * \param[in] drive a \a drive
		 * \returns 1 if \a true
		 */
		int (*is_changer_contain_drive)(struct st_database_connection * connect, struct st_changer * changer, struct st_drive * drive);
		void (*slots_are_enabled)(struct st_database_connection * connect, struct st_changer * changer);
		/**
		 * \brief Synchronize \a changer with database
		 *
		 * \param[in] connect a database connection
		 * \param[in] changer a \a changer
		 * \returns 0 if OK
		 */
		int (*sync_changer)(struct st_database_connection * connect, struct st_changer * changer, bool force_update);
		/**
		 * \brief Synchronize \a drive with database
		 *
		 * \param[in] connect a database connection
		 * \param[in] drive a \a drive
		 * \returns 0 if OK
		 */
		int (*sync_drive)(struct st_database_connection * connect, struct st_drive * drive);

		/**
		 * \brief Get the available size of \a pool based on offline media
		 *
		 * \param[in] connect a database connection
		 * \param[in] pool a \a pool
		 * \return available size
		 */
		ssize_t (*get_available_size_of_offline_media_from_pool)(struct st_database_connection * connect, struct st_pool * pool);
		/**
		 * \brief Get checksums linked to \a pool
		 *
		 * \param[in] connect a database connection
		 * \param[in] pool a \a pool
		 * \param[out] nb_checksums number of checksums returned
		 * \return a calloc allocated array of *nb_checksums + 1 elements, *nb_checksums first elements are string mallocated and the last element is always equals to NULL
		 */
		char ** (*get_checksums_by_pool)(struct st_database_connection * connect, struct st_pool * pool, unsigned int * nb_checksums) __attribute__((warn_unused_result));
		struct st_media * (*get_media)(struct st_database_connection * connect, struct st_job * job, const char * uuid, const char * medium_serial_number, const char * label);
		int (*get_media_format)(struct st_database_connection * connect, struct st_media_format * media_format, unsigned char density_code, const char * name, enum st_media_format_mode mode);
		struct st_pool * (*get_pool)(struct st_database_connection * connect, struct st_archive * archive, struct st_job * job, const char * uuid);
		int (*sync_media)(struct st_database_connection * connnect, struct st_media * media);
		int (*sync_media_format)(struct st_database_connection * connect, struct st_media_format * format);
		int (*sync_pool)(struct st_database_connection * connect, struct st_pool * pool);

		int (*add_check_archive_job)(struct st_database_connection * connect, struct st_job * job, struct st_archive * archive, time_t starttime, bool quick_mode);
		int (*add_job_record)(struct st_database_connection * connect, struct st_job * job, const char * message, enum st_job_record_notif notif);
		int (*finish_job_run)(struct st_database_connection * connect, struct st_job * job, time_t endtime, int exitcode);
		struct st_job_selected_path * (*get_selected_paths)(struct st_database_connection * connect, struct st_job * job, unsigned int * nb_paths);
		int (*start_job_run)(struct st_database_connection * connect, struct st_job * job, time_t starttime);
		int (*sync_job)(struct st_database_connection * connect, struct st_job *** jobs, unsigned int * nb_jobs);

		int (*get_user)(struct st_database_connection * connect, struct st_user * user, const char * login);
		int (*sync_user)(struct st_database_connection * connect, struct st_user * user);

		bool (*add_report)(struct st_database_connection * connect, struct st_job * job, struct st_archive * archive, const char * report);
		bool (*check_checksums_of_archive_volume)(struct st_database_connection * connect, struct st_archive_volume * volume);
		bool (*check_checksums_of_file)(struct st_database_connection * connect, struct st_archive_file * file);
		struct st_archive * (*get_archive_by_job)(struct st_database_connection * connect, struct st_job * job);
		struct st_archive ** (*get_archive_by_media)(struct st_database_connection * connect, struct st_media * media, unsigned int * nb_archive);
		struct st_archive_file * (*get_archive_file_for_restore_directory)(struct st_database_connection * connect, struct st_job * job, unsigned int * nb_files);
		int (*get_archive_files_by_job_and_archive_volume)(struct st_database_connection * connect, struct st_job * job, struct st_archive_volume * volume);
		struct st_archive * (*get_archive_volumes_by_job)(struct st_database_connection * connect, struct st_job * job);
		unsigned int (*get_checksums_of_archive_volume)(struct st_database_connection * connect, struct st_archive_volume * volume);
		unsigned int  (*get_checksums_of_file)(struct st_database_connection * connect, struct st_archive_file * file);
		unsigned int (*get_nb_volume_of_file)(struct st_database_connection * connect, struct st_job * job, struct st_archive_file * file);
		char * (*get_restore_path_from_file)(struct st_database_connection * connect, struct st_job * job, struct st_archive_file * file);
		char * (*get_restore_path_of_job)(struct st_database_connection * connect, struct st_job * job);
		ssize_t (*get_restore_size_by_job)(struct st_database_connection * connect, struct st_job * job);
		bool (*has_restore_to_by_job)(struct st_database_connection * connect, struct st_job * job);
		int (*mark_archive_as_purged)(struct st_database_connection * connect, struct st_media * media, struct st_job * job);
		bool (*mark_archive_file_as_checked)(struct st_database_connection * connect, struct st_archive * archive, struct st_archive_file * file, bool ok);
		bool (*mark_archive_volume_as_checked)(struct st_database_connection * connect, struct st_archive_volume * volume, bool ok);
		int (*sync_archive)(struct st_database_connection * connect, struct st_archive * archive);
		int (*sync_archive_mirror)(struct st_database_connection * connect, struct st_archive * a1, struct st_archive * a2);

		struct st_vtl_config * (*get_vtls)(struct st_database_connection * connect, unsigned int * nb_vtls);
		int (*delete_vtl)(struct st_database_connection * connect, struct st_vtl_config * config);
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
		 * \brief This function releases all memory associated to database configuration
		 *
		 * \param[in] database configuration
		 */
		void (*free)(struct st_database_config * db_config);
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
		 * \param[in] params hashtable which contains parameters
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
	/**
	 * \brief Sha1 sum of plugins source code
	 */
	const char * src_checksum;
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
 * \param[in] name name of config
 * \returns \b NULL if not found
 */
struct st_database_config * st_database_get_config_by_name(const char * name);

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
struct st_database * st_database_get_driver(const char * database);

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
void st_database_register_driver(struct st_database * database);

#endif

