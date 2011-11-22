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
*  Last modified: Tue, 22 Nov 2011 12:41:25 +0100                         *
\*************************************************************************/

#ifndef __STORIQARCHIVER_LOG_H__
#define __STORIQARCHIVER_LOG_H__

// forward declarations
struct sa_hashtable;
struct sa_log_module;


/**
 * \brief Enumerate level
 *
 * Each level has a priority (i.e. debug < info < warning < error)
 */
enum sa_log_level {
	/**
	 * \brief reserved for debugging
	 */
	sa_log_level_debug		= 0x3,

	/**
	 * \brief should be used to alert errors
	 */
	sa_log_level_error		= 0x0,

	/**
	 * \brief should be used to inform what the server does.
	 *
	 * Example: server starts a new job
	 */
	sa_log_level_info		= 0x2,

	/**
	 * \brief should be used to alert errors which can be recovered
	 *
	 * Example: dns service not available
	 */
	sa_log_level_warning	= 0x1,

	/**
	 * \brief should not be used
	 *
	 * Used only by sa_log_stringTolevel to report an error
	 */
	sa_log_level_unknown	= 0xF,
};


struct sa_log_driver {
	const char * name;
	int (*add)(struct sa_log_driver * driver, const char * alias, enum sa_log_level level, struct sa_hashtable * params);
	void * data;

	// used by server only
	// should not be modified
	void * cookie;
	int api_version;

	struct sa_log_module * modules;
	unsigned int nbModules;
};

/**
 * \brief Current api version
 *
 * Will increment from new version of struct sa_log_driver or struct sa_log
 */
#define STORIQARCHIVER_LOG_APIVERSION 1


struct sa_log_module {
	char * alias;
	enum sa_log_level level;
	struct sa_log_module_ops {
		void (*free)(struct sa_log_module * module);
		void (*write)(struct sa_log_module * module, enum sa_log_level level, const char * message);
	} * ops;

	void * data;
};


/**
 * \brief convert an enumeration to a statically allocated string
 * \param level : one log level
 * \return string
 */
const char * sa_log_level_to_string(enum sa_log_level level);

/**
 * \brief convert a string to an enumeration
 * \param string : one string level
 * \return an enumeration
 */
enum sa_log_level sa_log_string_to_level(const char * string);

/**
 * \brief get a log driver
 * \param module : driver's name
 * \return 0 if failed
 * \note if the driver is not loaded, we try to load it
 */
struct sa_log_driver * sa_log_get_driver(const char * module);

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
void sa_log_register_driver(struct sa_log_driver * driver);

void sa_log_write_all(enum sa_log_level level, const char * format, ...) __attribute__ ((format (printf, 2, 3)));
void sa_log_write_to(const char * alias, enum sa_log_level level, const char * format, ...) __attribute__ ((format (printf, 3, 4)));

#endif

