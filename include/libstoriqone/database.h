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
*  Copyright (C) 2013-2021, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#ifndef __LIBSTORIQONE_DATABASE_H__
#define __LIBSTORIQONE_DATABASE_H__

// bool
#include <stdbool.h>
// ssize_t
#include <sys/types.h>

struct so_application;
struct so_archive;
struct so_archive_file;
struct so_archive_volume;
struct so_backup;
struct so_backup_volume;
struct so_changer;
struct so_checksum_driver;
struct so_database;
struct so_database_config;
struct so_drive;
struct so_host;
struct so_job;
enum so_job_record_notif;
enum so_log_level;
enum so_job_status;
struct so_media;
struct so_media_format;
enum so_media_format_mode;
struct so_pool;
enum so_script_type;
struct so_value;

/**
 * \brief Synchronize method
 */
enum so_database_sync_method {
	/**
	 * \brief default method
	 */
	so_database_sync_default,
	/**
	 * \brief initial synchro
	 */
	so_database_sync_init,
	/**
	 * \brief Retrieve only id
	 */
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
	 * \var so_database_connection::ops
	 * \brief Operations
	 */
	struct so_database_connection_ops {
		/**
		 * \brief close \a db connection
		 *
		 * \param[in] connect : a database connection
		 * \return a value which correspond to
		 * \li 0 if ok
		 * \li < 0 if error
		 */
		int (*close)(struct so_database_connection * connect);
		/**
		 * \brief free memory associated with database connection
		 *
		 * \param[in] connect : a database connection
		 * \return a value which correspond to
		 * \li 0 if ok
		 * \li < 0 if error
		 *
		 * \warning Implementation of this function SHOULD NOT call
		 * \code
		 * free(db);
		 * \endcode
		 */
		int (*free)(struct so_database_connection * connect);
		/**
		 * \brief check if the connection to database is closed
		 *
		 * \param[in] connect : a database connection
		 * \return 0 if the connection is not closed
		 */
		bool (*is_connected)(struct so_database_connection * connect);

		/**
		 * \brief Rool back a transaction
		 *
		 * \param[in] connect : a database connection
		 * \li 0 if ok
		 * \li 1 if noop
		 * \li < 0 if error
		 */
		int (*cancel_transaction)(struct so_database_connection * connect);
		/**
		 * \brief Finish a transaction
		 *
		 * \param[in] connect : a database connection
		 * \return a value which correspond to
		 * \li 0 if ok
		 * \li 1 if noop
		 * \li < 0 if error
		 */
		int (*finish_transaction)(struct so_database_connection * connect);
		/**
		 * \brief Starts a transaction
		 *
		 * \param[in] connection : a database connection
		 * \return a value which correspond to
		 * \li 0 if ok
		 * \li 1 if noop
		 * \li < 0 if error
		 */
		int (*start_transaction)(struct so_database_connection * connect);

		/**
		 * \brief add an host to database
		 *
		 * \param[in] connection : a database connection
		 * \param[in] uuid : a uuid of host
		 * \param[in] name : name of host without domaine name
		 * \param[in] domaine : domaine name of host. can be NULL.
		 * \param[in] description : description of host. can be NULL.
		 * \param[in] daemon_version : version of storiqone
		 * \return 0 on success
		 */
		int (*add_host)(struct so_database_connection * connect, const char * uuid, const char * name, const char * domaine, const char * description, const char * daemon_version);
		/**
		 * \brief find a host by name
		 *
		 * \param[in] connection : a database connection
		 * \param[in] uuid : find by its uuid
		 * \param[in] hostname : find by its name
		 * \return \b true if found
		 */
		bool (*find_host)(struct so_database_connection * connect, const char * uuid, const char * hostname);
		/**
		 * \brief get host information by name
		 *
		 * \param[in] connection : a database connection
		 * \param[out] host : host information
		 * \param[in] name : hostname
		 * \return 0 on success
		 */
		int (*get_host_by_name)(struct so_database_connection * connect, struct so_host * host, const char * name);
		/**
		 * \brief update host
		 *
		 * \param[in] connection : a database connection
		 * \param[in] uuid : uuid of host
		 * \param[in] daemon_version : version of daemon
		 * \return 0 on success
		 */
		int (*update_host)(struct so_database_connection * connect, const char * uuid, const char * daemon_version);

		int (*can_delete_vtl)(struct so_database_connection * connect, struct so_changer * changer);
		/**
		 * \brief delete changer from database
		 *
		 * \param[in] connection : a database connection
		 * \param[in] changer : remove this \a changer
		 * \return 0 on success
		 */
		int (*delete_changer)(struct so_database_connection * connect, struct so_changer * changer);
		/**
		 * \brief delete drive from database
		 *
		 * \param[in] connection : a database connection
		 * \param[in] drive : remove this \a drive
		 * \return 0 on success
		 */
		int (*delete_drive)(struct so_database_connection * connect, struct so_drive * drive);
		ssize_t (*get_block_size_by_pool)(struct so_database_connection * connect, struct so_pool * pool) __attribute__((nonnull));
		/**
		 * \brief get a list of scsi changers
		 *
		 * \param[in] connection : a database connection
		 * \return a value which contains a list of scsi changers
		 */
		struct so_value * (*get_changers)(struct so_database_connection * connect);
		/**
		 * \brief get a list of checksums of pool
		 *
		 * \param[in] connection : a database connection
		 * \param[in] pool : a pool
		 * \return a value which contains a list of checksums
		 */
		struct so_value * (*get_checksums_from_pool)(struct so_database_connection * connect, struct so_pool * pool) __attribute__((warn_unused_result));
		/**
		 * \brief get a list of free medias
		 *
		 * \param[in] connection : a database connection
		 * \param[in] media_format : format of media
		 * \param[in] online : if \b true then returned list contains only online medias else only offline medias
		 * \return a value which contains a list of (on/off)line medias
		 */
		struct so_value * (*get_free_medias)(struct so_database_connection * connect, struct so_media_format * media_format, bool online) __attribute__((warn_unused_result));
		/**
		 * \brief get a media by his medium serial number or label or associated with job
		 *
		 * \param[in] connection : a database connection
		 * \param[in] medium_serial_number : serial number of media
		 * \param[in] label : media label
		 * \param[in] job : media associated with this job
		 * \return \b NULL on failure or a media
		 *
		 * \note only one parameter in \a medium_serial_number or \a label or \a job should not be \b NULL
		 */
		struct so_media * (*get_media)(struct so_database_connection * connect, const char * medium_serial_number, const char * label, struct so_job * job) __attribute__((warn_unused_result));
		/**
		 * \brief get a list of medias which are members of pool
		 *
		 * \param[in] connection : a database connection
		 * \param[in] pool : a pool
		 * \return a list of medias in \a pool
		 */
		struct so_value * (*get_medias_of_pool)(struct so_database_connection * connect, struct so_pool * pool) __attribute__((warn_unused_result));
		/**
		 * /brief get a media format by its density code and mode
		 *
		 * \param[in] connection : a database connection
		 * \param[in] density_code : a density code
		 * \param[in] mode : a media format mode
		 * \return \b NULL on failure
		 */
		struct so_media_format * (*get_media_format)(struct so_database_connection * connect, unsigned int density_code, enum so_media_format_mode mode);
		/**
		 * \brief get pool by its uuid or associated to this job
		 *
		 * \param[in] connection : a database connection
		 * \param[in] uuid : uuid of pool
		 * \param[in] job : pool associated with this job
		 * \return \b NULL on failure
		 */
		struct so_pool * (*get_pool)(struct so_database_connection * connect, const char * uuid, struct so_job * job) __attribute__((warn_unused_result));
		/**
		 * \brief get a list of pool which are also member of pool mirror minus \a pool
		 *
		 * \param[in] connection : a database connection
		 * \param[in] pool : pool of reference
		 * \return a list of pool
		 */
		struct so_value * (*get_pool_by_pool_mirror)(struct so_database_connection * connect, struct so_pool * pool) __attribute__((warn_unused_result));
		/**
		 * \brief get a list of standalone drives
		 *
		 * \param[in] connection : a database connection
		 * \return a list of changer which manage a standalone drive
		 */
		struct so_value * (*get_standalone_drives)(struct so_database_connection * connect);
		/**
		 * \brief get a list of VTLs
		 *
		 * \param[in] connection : a database connection
		 * \return a list of VTLs
		 */
		struct so_value * (*get_vtls)(struct so_database_connection * connect, bool new_vtl);
		int (*ignore_vtl_deletion)(struct so_database_connection * connect, struct so_changer * changer);
		/**
		 * \brief synchronize the status of \a changer with database
		 *
		 * \param[in] connection : a database connection
		 * \param[in,out] changer : a changer
		 * \param[in] method : synchronize strategy
		 * \return 0 on success
		 *
		 * \note the field \a next_action of \a changer can be updated
		 */
		int (*sync_changer)(struct so_database_connection * connect, struct so_changer * changer, enum so_database_sync_method method);
		/**
		 * \brief synchronize the status of \a drive with database
		 *
		 * \param[in] connection : a database connection
		 * \param[in] drive : a drive
		 * \param[in] sync_drive : should synchronize media in this drive
		 * \param[in] method : synchronize strategy
		 * \return 0 on success
		 */
		int (*sync_drive)(struct so_database_connection * connect, struct so_drive * drive, bool sync_media, enum so_database_sync_method method);
		/**
		 * \brief synchronize the status of \a media with database
		 *
		 * \param[in] connection : a database connection
		 * \param[in] media : a media
		 * \param[in] method : synchronize strategy
		 * \return 0 on success
		 */
		int (*sync_media)(struct so_database_connection * connect, struct so_media * media, enum so_database_sync_method method);
		/**
		 * \brief synchronize the status of <a>media format</a> with database
		 *
		 * \param[in] connection : a database connection
		 * \param[in] media_format : a media format
		 * \param[in] method : synchronize strategy
		 * \return 0 on success
		 */
		int (*sync_media_format)(struct so_database_connection * connect, struct so_media_format * format, enum so_database_sync_method method);
		/**
		 * \brief get new status of vtl
		 *
		 * \param[in] connection : a database connection
		 * \param[in] vtl : a vtl
		 * \return a value which contains new values of vtl
		 */
		struct so_value * (*update_vtl)(struct so_database_connection * connect, struct so_changer * vtl);

		/**
		 * \brief add job record
		 *
		 * \param[in] connection : a database connection
		 * \param[in] job : a job
		 * \param[in] level : level of record
		 * \param[in] notif : should notify user
		 * \param[in] message : a message
		 * \return 0 on success
		 */
		int (*add_job_record)(struct so_database_connection * connect, struct so_job * job, enum so_log_level level, enum so_job_record_notif notif, const char * message);
		/**
		 * \brief add job record
		 *
		 * \param[in] connection : a database connection
		 * \param[in] job_id : a job's id
		 * \param[in] num_run : number of instance of job
		 * \param[in] level : level of record
		 * \param[in] notif : should notify user
		 * \param[in] message : a message
		 * \return 0 on success
		 */
		int (*add_job_record2)(struct so_database_connection * connect, const char * job_id, unsigned int num_run, enum so_job_status status, enum so_log_level level, enum so_job_record_notif notif, const char * message);
		/**
		 * \brief add report into database
		 *
		 * \param[in] connection : a database connection
		 * \param[in] job :
		 * \param[in] archive :
		 * \param[in] media :
		 * \param[in] data : a json formatted report
		 * \return 0 on success
		 */
		int (*add_report)(struct so_database_connection * connect, struct so_job * job, struct so_archive * archive, struct so_media * media, const char * data);
		int (*disable_old_jobs)(struct so_database_connection * connect);
		/**
		 * \brief get alternative path of restore-archive job
		 *
		 * \param[in] connection : a database connection
		 * \param[in] job : a job
		 * \return \b NULL on failure or if there is no restore path
		 */
		char * (*get_restore_path)(struct so_database_connection * connect, struct so_job * job) __attribute__((warn_unused_result));
		/**
		 * \brief check if user of \a job has been disactivated
		 *
		 * \param[in] connection : a database connection
		 * \param[in] job : a job
		 * \return \b true on failure or if user has been disactivated
		 */
		bool (*is_user_disabled)(struct so_database_connection * connect, struct so_job * job);
		/**
		 * \brief Create new jobrun
		 *
		 * \param[in] connection : a database connection
		 * \param[in] job : a job
		 * \return 0 on success
		 */
		int (*start_job)(struct so_database_connection * connect, struct so_job * job);
		/**
		 * \brief Update new jobrun
		 *
		 * \param[in] connection : a database connection
		 * \param[in] job : a job
		 * \return 0 on success
		 */
		int (*stop_job)(struct so_database_connection * connect, struct so_job * job);
		/**
		 * \brief Update job's information
		 *
		 * \param[in] connection : a database connection
		 * \param[in] job : a job
		 * \return 0 on success
		 */
		int (*sync_job)(struct so_database_connection * connect, struct so_job * job);
		/**
		 * \brief Get new jobs from database and remove old
		 *
		 * \param[in] connection : a database connection
		 * \param[in,out] jobs : a set of jobs
		 * \return 0 on success
		 */
		int (*sync_jobs)(struct so_database_connection * connect, struct so_value * jobs);

		/**
		 * \brief Get the number of scripts related to pool
		 *
		 * \param[in] connection : a database connection
		 * \param[in] job_type : type of job
		 * \param[in] type : type of script
		 * \param[in] pool : a pool
		 * \return < 0 on failure or the number of scripts
		 */
		int (*get_nb_scripts)(struct so_database_connection * connect, const char * job_type, enum so_script_type type, struct so_pool * pool);
		/**
		 * \brief Get the path of one script
		 *
		 * \param[in] connection : a database connection
		 * \param[in] job_type : type of job
		 * \param[in] sequence : nth script
		 * \param[in] type : type of script
		 * \param[in] pool : a pool
		 * return NULL on failure or a dynamically allocated string
		 */
		char * (*get_script)(struct so_database_connection * connect, const char * job_type, unsigned int sequence, enum so_script_type type, struct so_pool * pool);

		/**
		 * \brief Find a checksum plugin by its name
		 *
		 * \param[in] connection : a database connection
		 * \param[in] checksum : name of checksum
		 * \return \b false on failure
		 */
		bool (*find_plugin_checksum)(struct so_database_connection * connect, const char * checksum);
		/**
		 * \brief Insert new checksum into database if not found
		 *
		 * \param[in] connection : a database connection
		 * \param[in] driver : a checksum's driver
		 * \return 0 on success
		 */
		int (*sync_plugin_checksum)(struct so_database_connection * connect, struct so_checksum_driver * driver);
		/**
		 * \brief Insert new job's type into database if not found
		 *
		 * \param[in] connection : a database connection
		 * \param[in] job : a job's type
		 * \return 0 on success
		 */
		int (*sync_plugin_job)(struct so_database_connection * connect, const char * job);
		/**
		 * \brief Insert new script into database if not found
		 *
		 * \param[in] connection : a database connection
		 * \param[in] script_path : path of script
		 * \return 0 on success
		 */
		int (*sync_plugin_script)(struct so_database_connection * connect, const char * script_name, const char * script_description, const char * script_path, const char * script_type);

		/**
		 * \brief update integrity of file
		 *
		 * \param[in] connection : a database connection
		 * \param[in] archive : an archive
		 * \param[in] file : a file in \a archive
		 * \return 0 on success
		 */
		int (*check_archive_file)(struct so_database_connection * connect, struct so_archive * archive, struct so_archive_file * file);
		bool (*check_archive_file_up_to_date)(struct so_database_connection * connect, struct so_archive * archive, const char * archive_filename);
		/**
		 * \brief update integrity of volume
		 *
		 * \param[in] connection : a database connection
		 * \param[in] volume : a volume
		 * \return 0 on success
		 */
		int (*check_archive_volume)(struct so_database_connection * connect, struct so_archive_volume * volume);
		/**
		 * \brief create job to check an archive
		 *
		 * \param[in] connection : a database connection
		 * \param[in] current_job : job which create new one
		 * \param[in] archive : check this \a archive
		 * \param[in] quick_mode : checking mode
		 * \return 0 on success
		 */
		int (*create_check_archive_job)(struct so_database_connection * connect, struct so_job * current_job, struct so_archive * archive, bool quick_mode);
		int (*find_first_volume_of_archive_file)(struct so_database_connection * connect, struct so_archive * archive, const char * archive_file);
		/**
		 * \brief get a list of archives which are member of same archive mirror as \a archive
		 *
		 * \param[in] connection : a database connection
		 * \param[in] archive : reference archive
		 * \return a value which contains a list of archives
		 */
		/**
		 * \brief get an archive related to \a job
		 *
		 * \param[in] connection : a database connection
		 * \param[in] job : a job
		 * \return \b NULL on failure
		 */
		struct so_archive * (*get_archive_by_job)(struct so_database_connection * connect, struct so_job * job) __attribute__((warn_unused_result));
		/**
		 * \brief get an archive format by its name
		 *
		 * \param[in] connection : a database connection
		 * \param[in] name : name of archive format
		 * \return \b NULL on failure
		 */
		struct so_archive_format * (*get_archive_format_by_name)(struct so_database_connection * connect, const char * name) __attribute__((warn_unused_result));
		struct so_value * (*get_archives_by_archive_mirror)(struct so_database_connection * connect, struct so_archive * archive) __attribute__((warn_unused_result));
		/**
		 * \brief get a list of archives which are on this \a media
		 *
		 * \param[in] connection : a database connection
		 * \param[in] media : a media
		 * \return a list of archives
		 */
		struct so_value * (*get_archives_by_media)(struct so_database_connection * connect, struct so_media * media) __attribute__((warn_unused_result));
		int (*get_nb_archives_by_media)(struct so_database_connection * connect, const char * archive_uuid, struct so_media * media);
		/**
		 * \brief compute the number of volume where \a file is on \a archive
		 *
		 * \param[in] connection : a database connection
		 * \param[in] archive : an archive
		 * \param[in] file : a file in \a archive
		 * \return \b -1 on failure
		 */
		unsigned int (*get_nb_volumes_of_file)(struct so_database_connection * connect, struct so_archive * archive, struct so_archive_file * file);
		char * (*get_original_path_from_alternate_path)(struct so_database_connection * connect, struct so_archive * archive, const char * path) __attribute__((warn_unused_result));
		char * (*get_selected_path_from_alternate_path)(struct so_database_connection * connect, struct so_archive * archive, const char * path) __attribute__((warn_unused_result));
		/**
		 * \brief get a list of archives which are synchronized
		 *
		 * \param[in] connection : a database connection
		 * \param[in] archive : reference archive
		 * \return a list of archives
		 */
		struct so_value * (*get_synchronized_archive)(struct so_database_connection * connect, struct so_archive * archive) __attribute__((warn_unused_result));
		/**
		 * \brief check if \a is synchronized
		 *
		 * \param[in] connection : a database connection
		 * \param[in] archive : an archive
		 * \return \b false on failure
		 */
		bool (*is_archive_synchronized)(struct so_database_connection * connect, struct so_archive * archive);
		/**
		 * \brief create an archive mirror if needed and link \a source and \a copy to same archive mirror
		 *
		 * \param[in] connection : a database connection
		 * \param[in] job : a job
		 * \param[in] source : an archive
		 * \param[in] copy : an archive
		 * \return \b false on failure
		 */
		int (*link_archives)(struct so_database_connection * connect, struct so_job * job, struct so_archive * source, struct so_archive * copy, struct so_pool * pool);
		int (*mark_archive_as_purged)(struct so_database_connection * connect, struct so_media * media, struct so_job * job);
		int (*sync_archive)(struct so_database_connection * connect, struct so_archive * archive, struct so_archive * original, bool close_archive);
		int (*sync_archive_format)(struct so_database_connection * connect, struct so_archive_format * formats, unsigned int nb_formats);
		int (*update_link_archive)(struct so_database_connection * connect, struct so_archive * archive, struct so_job * job);

		int (*backup_add)(struct so_database_connection * connect, struct so_backup * backup);
		struct so_backup * (*get_backup)(struct so_database_connection * connect, struct so_job * job) __attribute__((warn_unused_result));
		int (*mark_backup_volume_checked)(struct so_database_connection * connect, struct so_backup_volume * volume);

		bool (*find_user_by_login)(struct so_database_connection * connect, const char * login);

		int (*create_selected_file_if_missing)(struct so_database_connection * connect, const char * path);
		struct so_value * (*get_selected_files_by_job)(struct so_database_connection * connect, struct so_job * job) __attribute__((warn_unused_result));

		struct so_application * (*api_key_create)(struct so_database_connection * connect, const char * application) __attribute__((warn_unused_result));
		struct so_application * (*api_key_list)(struct so_database_connection * connect, unsigned int * nb_keys) __attribute__((warn_unused_result));
		bool (*api_key_remove)(struct so_database_connection * connect, const char * application);
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
	 * \var so_database_config::ops
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
		struct so_database_connection * (*connect)(struct so_database_config * db_config) __attribute__((warn_unused_result));
		/**
		 * \brief Check if database is online
		 */
		int (*ping)(struct so_database_config * db_config);
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
	 * \var so_database::ops
	 * \brief Database operation
	 */
	struct so_database_ops {
		/**
		 * \brief Add configure to this database driver
		 *
		 * \param[in] params hashtable which contains parameters
		 * \returns \b 0 if ok
		 */
		struct so_database_config * (*add)(struct so_value * params);
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
 * \param[in] driver : driver's name
 * \return NULL if failed
 *
 * \note if this driver is not loaded, this function will load it
 * \warning the returned value <b>SHALL NOT BE RELEASE</b> with \a free
 */
struct so_database * so_database_get_driver(const char * driver);

/**
 * \brief Load database configuration
 *
 * \param[in] config : database configuration
 *
 * \note this function is intent to be used by libstoriqone-changer, libstoriqone-drive and libstoriqone-job
 */
void so_database_load_config(struct so_value * config);

/**
 * \brief Register a database driver
 *
 * \param[in] driver :a statically allocated struct so_database
 *
 * \note Each database driver should call this function only one time
 * \code
 * static void database_myDb_init() __attribute__((constructor)) {
 * \endcode
 */
void so_database_register_driver(struct so_database * driver);

#endif

