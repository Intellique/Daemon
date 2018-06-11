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
*  Copyright (C) 2013-2016, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#ifndef __LIBSTORIQONE_ARCHIVE_H__
#define __LIBSTORIQONE_ARCHIVE_H__

// bool
#include <stdbool.h>
// time_t
#include <sys/time.h>
// gid_t, mode_t, ssize_t, uid_t
#include <sys/types.h>

struct so_archive_file;
enum st_archive_file_type;
struct so_archive_volume;
struct so_format_file;
struct so_media;
struct so_value;

/**
 * \enum so_archive_file_type
 * \brief Represents a file type
 */
enum so_archive_file_type {
	/**
	 * \brief Block device (aka /dev/sd*)
	 */
	so_archive_file_type_block_device,
	/**
	 * \brief Character device (aka /dev/st*)
	 */
	so_archive_file_type_character_device,
	/**
	 * \brief Directory
	 */
	so_archive_file_type_directory,
	/**
	 * \brief Fifo (created with mkfifo)
	 */
	so_archive_file_type_fifo,
	/**
	 * \brief Simple file
	 */
	so_archive_file_type_regular_file,
	/**
	 * \brief Unix socket
	 */
	so_archive_file_type_socket,
	/**
	 * \brief Symbolic link
	 */
	so_archive_file_type_symbolic_link,

	/**
	 * \brief Used to manage errors
	 */
	so_archive_file_type_unknown,
};

enum so_archive_status {
	so_archive_status_incomplete,
	so_archive_status_data_complete,
	so_archive_status_complete,
	so_archive_status_error
};

/**
 * \struct so_archive
 * \brief Represents an archive
 */
struct so_archive {
	/**
	 * \brief UUID of archive
	 */
	char uuid[37];
	/**
	 * \brief Name of archive
	 */
	char * name;

	/**
	 * \brief Size of archive
	 *
	 * Size is the sum of all volumes of archive
	 */
	ssize_t size;

	/**
	 * \brief Volumes of archive
	 */
	struct so_archive_volume * volumes;
	/**
	 * \brief Number of volumes of archive
	 */
	unsigned int nb_volumes;

	/**
	 * \brief Creator of archive
	 */
	char * creator;
	/**
	 * \brief Manager of archive
	 */
	char * owner;

	/**
	 * \brief Value which contains all archive's metadata
	 */
	struct so_value * metadata;

	/**
	 * \brief Flag which allows (or not) to append volume to current archive
	 */
	bool can_append;
	enum so_archive_status status;
	/**
	 * \brief \a true if archive has been logicaly (or really) deleted
	 *
	 * \note A media can only be erased if all archives on it are deleted
	 */
	bool deleted;

	/**
	 * \brief private data used by database
	 */
	struct so_value * db_data;
};

/**
 * \struct so_archive_volume
 * \brief Represents a volume of an archive
 */
struct so_archive_volume {
	/**
	 * \brief sequence number of volume in archive
	 */
	unsigned int sequence;
	/**
	 * \brief Size of this volume
	 */
	ssize_t size;

	/**
	 * \brief Creation time of this volume
	 *
	 * \note Time is store as Unix timestamp.
	 */
	time_t start_time;
	/**
	 * \brief Closing time of this volume
	 *
	 * \note Time is store as Unix timestamp.
	 */
	time_t end_time;

	/**
	 * \brief result of last check
	 */
	bool check_ok;
	/**
	 * \brief Last volume check time.
	 *
	 * \note Time is store as Unix timestamp.
	 *
	 * \note If \a check_time equals to \b 0 then it means
	 * than this volume has never been checked
	 */
	time_t check_time;

	/**
	 * \brief Archive of current volume
	 */
	struct so_archive * archive;
	/**
	 * \brief Media of current volume
	 */
	struct so_media * media;
	/**
	 * \brief File position in current media
	 *
	 * \note Usefull when media is a tape
	 */
	unsigned int media_position;
	/**
	 * \brief Job which create this volume
	 *
	 * \warning MAY BE NULL
	 */
	struct so_job * job;

	/**
	 * \brief An hashtable which contains digests of this volume.
	 *
	 * In this hashtable, keys are algorithm and values are digests.
	 */
	struct so_value * digests;

	/**
	 * \struct so_archive_files
	 * \brief file's information related to this volume
	 *
	 * \var so_archive_volume::files
	 * \brief files in current volume
	 */
	struct so_archive_files {
		/**
		 * \brief file's information
		 */
		struct so_archive_file * file;
		/**
		 * \brief Position of file on this volume
		 */
		ssize_t position;
		/**
		 * \brief Archive time of this file
		 */
		time_t archived_time;
	} * files;
	/**
	 * \brief Number of files in current volume
	 */
	unsigned int nb_files;

	/**
	 * \brief private data used by database
	 */
	struct so_value * db_data;
};

/**
 * \struct so_archive_file
 * \brief Represents a file of an archive
 */
struct so_archive_file {
	/**
	 * \brief Original path of file
	 */
	char * path;
	/**
	 * \brief alternate path of file
	 *
	 * \warning can be NULL
	 */
	char * alternate_path;
	/**
	 * \brief Restore path of file
	 *
	 * \warning can be NULL
	 */
	char * restored_to;
	char * hash;

	/**
	 * \brief permission of file
	 */
	mode_t perm;
	/**
	 * \brief type of file
	 */
	enum so_archive_file_type type;
	/**
	 * \brief owner's id of file
	 */
	uid_t ownerid;
	/**
	 * \brief owner's name of file
	 */
	char * owner;
	/**
	 * \brief group's id of file
	 */
	gid_t groupid;
	/**
	 * \brief group's name of file
	 */
	char * group;

	/**
	 * \brief creation time of file
	 */
	time_t create_time;
	/**
	 * \brief last modification time of file
	 */
	time_t modify_time;

	/**
	 * \brief result of last check
	 */
	bool check_ok;
	/**
	 * \brief Last time this file has been checked
	 *
	 * \note Time is store as Unix timestamp.
	 *
	 * \note if \a check_time is equals to \b 0 then
	 * no check has been performed.
	 */
	time_t check_time;

	/**
	 * \brief Size of file
	 */
	ssize_t size;

	/**
	 * \brief Metadata of file
	 */
	struct so_value * metadata;
	/**
	 * \brief Mime type of file
	 */
	char * mime_type;
	/**
	 * \brief path which led to archiving this file
	 */
	char * selected_path;

	/**
	 * \brief An hashtable which contains digests of this file.
	 *
	 * In this hashtable, keys are algorithm and values are digests.
	 */
	struct so_value * digests;

	/**
	 * \brief private data used by database
	 */
	struct so_value * db_data;
};

/**
 * \struct so_archive_format
 * \brief Represents an archive format
 */
struct so_archive_format {
	/**
	 * \brief name of archive format
	 */
	char name[33];

	/**
	 * \brief \a true if Storiq One can read this archive format
	 */
	bool readable;
	/**
	 * \brief \a true if Storiq One can write this archive format
	 */
	bool writable;

	/**
	 * \brief private data used by database
	 */
	struct so_value * db_data;
};

/**
 * \brief Create a new volume into archive
 *
 * \param[in,out] archive : an archive
 * \return a new volume
 */
struct so_archive_volume * so_archive_add_volume(struct so_archive * archive);
/**
 * \brief Convert an archive into a so_value
 *
 * \param[in] archive : an archive
 * \return a value
 *
 * \note To encode an archive into JSON, we need to call so_json_encode with
 * returned value
 * \see so_json_encode_to_fd
 * \see so_json_encode_to_file
 * \see so_json_encode_to_string
 */
struct so_value * so_archive_convert(struct so_archive * archive);
/**
 * \brief Release memory used by \a archive
 *
 * \param[in] archive : an archive
 */
void so_archive_free(struct so_archive * archive);
/**
 * \brief Release memory used by \a archive
 *
 * This function is a wrapper to so_archive_free used by so_value_free
 * \param[in] archive : an archive
 *
 * \note you may need this function when you pass an archive to so_value_new_custom
 * \see so_archive_free
 * \see so_value_free
 * \see so_value_new_custom
 */
void so_archive_free2(void * archive);
/**
 * \brief Create new archive
 *
 * \return new archive
 */
struct so_archive * so_archive_new(void);
const char * so_archive_status_to_string(enum so_archive_status status, bool translate);
enum so_archive_status so_archive_string_to_status(const char * status, bool translate);
/**
 * \brief synchronize an \a archive with \a new_archive
 *
 * \param[in,out] archive : an archive
 * \param[in] new_archive : values of one archive
 */
void so_archive_sync(struct so_archive * archive, struct so_value * new_archive);

/**
 * \brief Deep copy of an archive file
 *
 * \param[in] file : an archive file
 * \return copy an \a file
 */
struct so_archive_file * so_archive_file_copy(struct so_archive_file * file);
/**
 * \brief Create an archive file from a format file
 *
 * \param[in] file : a format file
 * \return an archive file
 *
 * \see so_format_file
 */
struct so_archive_file * so_archive_file_import(struct so_format_file * file);
void so_archive_file_update_hash(struct so_archive_file * file);

/**
 * \brief Convert a unix mode to an archive file type
 *
 * Unix mode can be retrieve with "stat" syscall
 * \param[in] mode : a unix mode
 * \return an archive file type
 */
enum so_archive_file_type so_archive_file_mode_to_type(mode_t mode);
/**
 * \brief parse a string and convert it into an archive file type
 *
 * \param[in] type : a string representing an archive file type
 * \param[in] translate : translate \a type into internal form
 * \return an archive file type or so_archive_file_type_unknown if not found
 */
enum so_archive_file_type so_archive_file_string_to_type(const char * type, bool translate);
/**
 * \brief an archive file type into string
 *
 * \param[in] type : an archive file type
 * \param[in] translate : translate the returned value
 * \return a string representing an archive file type
 */
const char * so_archive_file_type_to_string(enum so_archive_file_type type, bool translate);

/**
 * \brief Convert an archive format into a value
 *
 * \param[in] archive_format : an archive format
 * \return a value
 *
 * \note To encode an archive format into JSON, we need to call so_json_encode with
 * returned value
 * \see so_json_encode_to_fd
 * \see so_json_encode_to_file
 * \see so_json_encode_to_string
 */
struct so_value * so_archive_format_convert(struct so_archive_format * archive_format) __attribute__((warn_unused_result));
/**
 * \brief Duplicate an archive format
 *
 * \param[in] archive_format : an archive format
 * \return a copy of \a archive_format
 */
struct so_archive_format * so_archive_format_dup(struct so_archive_format * archive_format) __attribute__((warn_unused_result));
/**
 * \brief Release memory used by an archive format
 *
 * \param[in] archive_format : an archive format
 */
void so_archive_format_free(struct so_archive_format * archive_format);
/**
 * \brief synchronize an <a>archive format</a> with \a new_archive_format
 *
 * \param[in,out] archive_format : an archive format
 * \param[in] new_archive_format : values of one archive format
 */
void so_archive_format_sync(struct so_archive_format * archive_format, struct so_value * new_archive_format);

/**
 * \brief Convert an archive volume into a value
 *
 * \param[in] volume : an archive volume
 * \return a value
 *
 * \note To encode an archive volume into JSON, we need to call so_json_encode with
 * returned value
 * \see so_json_encode_to_fd
 * \see so_json_encode_to_file
 * \see so_json_encode_to_string
 */
struct so_value * so_archive_volume_convert(struct so_archive_volume * volume);
/**
 * \brief Release memory used by an archive volume
 *
 * \param[in] volume : an archive volume
 */
void so_archive_volume_free(struct so_archive_volume * volume);
/**
 * \brief synchronize an <a>archive volume</a> with \a new_volume
 *
 * \param[in,out] volume : an archive volume
 * \param[in] new_volume : values of one archive volume
 */
void so_archive_volume_sync(struct so_archive_volume * volume, struct so_value * new_volume);

#endif

