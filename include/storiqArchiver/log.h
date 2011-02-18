/***********************************************************************\
*       ______           ____     ___           __   _                  *
*      / __/ /____  ____/  _/__ _/ _ | ________/ /  (_)  _____ ____     *
*     _\ \/ __/ _ \/ __// // _ `/ __ |/ __/ __/ _ \/ / |/ / -_) __/     *
*    /___/\__/\___/_/ /___/\_, /_/ |_/_/  \__/_//_/_/|___/\__/_/        *
*                           /_/                                         *
*  -------------------------------------------------------------------  *
*  This file is a part of StorIqArchiver                                *
*                                                                       *
*  StorIqArchiver is free software; you can redistribute it and/or      *
*  modify it under the terms of the GNU General Public License          *
*  as published by the Free Software Foundation; either version 3       *
*  of the License, or (at your option) any later version.               *
*                                                                       *
*  This program is distributed in the hope that it will be useful,      *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of       *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
*  GNU General Public License for more details.                         *
*                                                                       *
*  You should have received a copy of the GNU General Public License    *
*  along with this program; if not, write to the Free Software          *
*  Foundation, Inc., 51 Franklin Street, Fifth Floor,                   *
*  Boston, MA  02110-1301, USA.                                         *
*                                                                       *
*  -------------------------------------------------------------------  *
*  Copyright (C) 2011, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Sun, 06 Feb 2011 22:59:39 +0100                       *
\***********************************************************************/

#ifndef __STORIQARCHIVER_LOG_H__
#define __STORIQARCHIVER_LOG_H__

// forward declarations
struct hashtable;
struct log_moduleSub;


/**
 * \brief Enumerate level
 *
 * Each level has a priority (i.e. debug < info < warning < error)
 */
enum Log_level {
	/**
	 * \brief reserved for debugging
	 */
	Log_level_debug		= 0x0,

	/**
	 * \brief should be used to alert errors
	 */
	Log_level_error		= 0x3,

	/**
	 * \brief should be used to inform what the server does.
	 *
	 * Example: server starts a new job
	 */
	Log_level_info		= 0x1,

	/**
	 * \brief should be used to alert errors which can be recovered
	 *
	 * Example: dns service not available
	 */
	Log_level_warning	= 0x2,

	/**
	 * \brief should not be used
	 *
	 * Used only by log_stringTolevel to report an error
	 */
	Log_level_unknown	= 0xF,
};


struct log_module {
	const char * moduleName;
	int (*add)(struct log_module * module, const char * alias, enum Log_level level, struct hashtable * params);
	void * data;

	// used by server only
	// should not be modified
	void * cookie;

	struct log_moduleSub * subModules;
	unsigned int nbSubModules;
};


struct log_moduleSub {
	char * alias;
	enum Log_level level;
	struct log_moduleSub_ops {
		void (*free)(struct log_moduleSub * moduleSub);
		void (*write)(struct log_moduleSub * moduleSub, enum Log_level level, const char * message);
	} * ops;

	void * data;
};


/**
 * \brief convert an enumeration to a statically allocated string
 * \param level : one log level
 * \return string
 */
const char * log_levelToString(enum Log_level level);

/**
 * \brief convert a string to an enumeration
 * \param string : one string level
 * \return an enumeration
 */
enum Log_level log_stringTolevel(const char * string);

/**
 * \brief get a module
 * \param module : module's name
 * \return 0 if failed
 * \note if the module is not loaded, we try to load it
 */
struct log_module * log_getModule(const char * module);

/**
 * \brief try to load a module by his name
 * \param module : name of module
 * \return a value which correspond to
 * \li 0 if ok
 * \li 1 if permission error
 * \li 2 if module didn't call log_registerModule
 * \li 3 if module is null
 */
int log_loadModule(const char * module);

/**
 * \brief Each module log should call this function only one time
 * \param module : a static allocated struct
 * \code
 * __attribute__((constructor))
 * static void log_myLog_init() {
 *    log_registerModule(&log_myLog_module);
 * }
 * \endcode
 */
void log_registerModule(struct log_module * module);

void log_writeAll(enum Log_level level, const char * format, ...) __attribute__ ((format (printf, 2, 3)));
void log_writeTo(const char * alias, enum Log_level level, const char * format, ...) __attribute__ ((format (printf, 3, 4)));

#endif

