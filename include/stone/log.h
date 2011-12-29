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
*  Copyright (C) 2011, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Thu, 29 Dec 2011 20:42:19 +0100                         *
\*************************************************************************/

#ifndef __STONE_LOG_H__
#define __STONE_LOG_H__

// forward declarations
struct st_hashtable;
struct st_log_module;
struct st_user;


/**
 * \brief Enumerate level
 *
 * Each level has a priority (i.e. debug < info < warning < error)
 */
enum st_log_level {
	/**
	 * \brief reserved for debugging
	 */
	st_log_level_debug		= 0x3,

	/**
	 * \brief should be used to alert errors
	 */
	st_log_level_error		= 0x0,

	/**
	 * \brief should be used to inform what the server does.
	 *
	 * Example: server starts a new job
	 */
	st_log_level_info		= 0x2,

	/**
	 * \brief should be used to alert errors which can be recovered
	 *
	 * Example: dns service not available
	 */
	st_log_level_warning	= 0x1,

	/**
	 * \brief should not be used
	 *
	 * Used only by st_log_stringTolevel to report an error
	 */
	st_log_level_unknown	= 0xF,
};

enum st_log_type {
	st_log_type_changer,
	st_log_type_checksum,
	st_log_type_daemon,
	st_log_type_database,
	st_log_type_drive,
	st_log_type_job,
	st_log_type_plugin_checksum,
	st_log_type_plugin_db,
	st_log_type_plugin_log,
	st_log_type_scheduler,
	st_log_type_ui,
	st_log_type_user_message,

	st_log_type_unknown,
};


struct st_log_driver {
	const char * name;
	int (*add)(struct st_log_driver * driver, const char * alias, enum st_log_level level, struct st_hashtable * params);
	void * data;

	// used by server only
	// should not be modified
	void * cookie;
	const int api_version;

	struct st_log_module {
		char * alias;
		enum st_log_level level;
		struct st_log_module_ops {
			void (*free)(struct st_log_module * module);
			void (*write)(struct st_log_module * module, enum st_log_level level, enum st_log_type type, const char * message, struct st_user * user);
		} * ops;

		void * data;
	} * modules;
	unsigned int nb_modules;
};

/**
 * \brief Current api version
 *
 * Will increment from new version of struct st_log_driver or struct st_log
 */
#define STONE_LOG_APIVERSION 1


/**
 * \brief get a log driver
 * \param module : driver's name
 * \return 0 if failed
 * \note if the driver is not loaded, we try to load it
 */
struct st_log_driver * st_log_get_driver(const char * module);

/**
 * \brief Each log driver should call this function only one time
 * \param driver : a static allocated struct
 * \code
 * __attribute__((constructor))
 * static void log_myLog_init() {
 *    log_register_driver(&log_myLog_module);
 * }
 * \endcode
 */
void st_log_register_driver(struct st_log_driver * driver);

/**
 * \brief convert an enumeration to a statically allocated string
 * \param level : one log level
 * \return string
 */
const char * st_log_level_to_string(enum st_log_level level);

/**
 * \brief convert a string to an enumeration
 * \param string : one string level
 * \return an enumeration
 */
enum st_log_level st_log_string_to_level(const char * string);

enum st_log_type st_log_string_to_type(const char * string);
const char * st_log_type_to_string(enum st_log_type type);

void st_log_write_all(enum st_log_level level, enum st_log_type type, const char * format, ...) __attribute__ ((format (printf, 3, 4)));
void st_log_write_all2(enum st_log_level level, enum st_log_type type, struct st_user * user, const char * format, ...) __attribute__ ((format (printf, 4, 5)));
void st_log_write_to(const char * alias, enum st_log_type type, enum st_log_level level, const char * format, ...) __attribute__ ((format (printf, 4, 5)));

#endif

