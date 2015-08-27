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
*  Copyright (C) 2013-2015, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#ifndef __LIBSTORIQONE_LOG_H__
#define __LIBSTORIQONE_LOG_H__

// bool
#include <stdbool.h>
// time_t
#include <sys/time.h>

struct so_value;

/**
 * \enum so_log_level
 * \brief Enumerate level of message
 *
 * Each level has a priority (i.e. debug > info > warning > error)
 */
enum so_log_level {
	/**
	 * \brief action must be taken immendiately
	 */
	so_log_level_alert = 0x2,

	/**
	 * \brief critical condictions
	 */
	so_log_level_critical = 0x3,

	/**
	 * \brief Reserved for debugging
	 */
	so_log_level_debug = 0x8,

	/**
	 * \brief System unstable
	 */
	so_log_level_emergencey = 0x1,

	/**
	 * \brief Should be used to alert errors
	 */
	so_log_level_error = 0x4,

	/**
	 * \brief Should be used to inform what the server does.
	 *
	 * Example: server starts a new job
	 */
	so_log_level_info = 0x7,

	/**
	 * \brief normal but significant condition
	 */
	so_log_level_notice = 0x6,

	/**
	 * \brief Should be used to alert errors which can be recovered
	 *
	 * Example: dns service not available
	 */
	so_log_level_warning = 0x5,

	/**
	 * \brief Should not be used
	 *
	 * Used only by so_log_string_to_level to report an error
	 * \see so_log_string_to_level
	 */
	so_log_level_unknown = 0x0,
};

/**
 * \enum so_log_type
 * \brief Enumerate type of message
 */
enum so_log_type {
	/**
	 * \brief used by changer
	 */
	so_log_type_changer = 0x1,
	/**
	 * \brief generic message from daemon
	 */
	so_log_type_daemon = 0x2,
	/**
	 * \brief used by tape drive
	 */
	so_log_type_drive = 0x3,
	/**
	 * \brief used by jobs
	 */
	so_log_type_job = 0x4,
	so_log_type_logger = 0x5,
	/**
	 * \brief used by checksum module
	 */
	so_log_type_plugin_checksum = 0x6,
	/**
	 * \brief used by database module
	 */
	so_log_type_plugin_db = 0x7,
	/**
	 * \brief used by log module
	 */
	so_log_type_plugin_log = 0x8,
	/**
	 * \brief used by scheduler
	 */
	so_log_type_scheduler = 0x9,

	/**
	 * \brief used by user interface
	 */
	so_log_type_ui = 0xa,
	so_log_type_user_message = 0xb,

	/**
	 * \brief Should not be used
	 *
	 * Used only by so_log_string_to_type to report an error
	 * \see so_log_string_to_type
	 */
	so_log_type_unknown = 0x0,
};


void so_log_configure(struct so_value * config, enum so_log_type default_type);

unsigned int so_log_level_max_length(void);

/**
 * \brief Convert an enumeration to a statically allocated string
 *
 * \param level : one log level
 * \return a statically allocated string
 * \note returned value should not be released
 */
const char * so_log_level_to_string(enum so_log_level level, bool translate);

void so_log_stop_logger(void);

/**
 * \brief Convert a c string to an enumeration
 *
 * \param string : one string level
 * \return an enumeration
 */
enum so_log_level so_log_string_to_level(const char * string, bool translate);

/**
 * \brief Convert a c string to an enum so_log_type
 *
 * \param string : one string type
 * \return an enumeration
 */
enum so_log_type so_log_string_to_type(const char * string, bool translate);

unsigned int so_log_type_max_length(void);

/**
 * \brief Convert an enum so_log_type to a statically allocated c string
 *
 * \param[in] type : a log type
 * \return a statically allocated c string
 *
 * \note so_log_type_to_string never returns NULL and you <b>SHOULD NOT RELEASE</b> returned value
 */
const char * so_log_type_to_string(enum so_log_type type, bool translate);

/**
 * \brief Write a message to all log modules
 *
 * \param[in] level : level of message
 * \param[in] type : type of message
 * \param[in] format : message with printf-like syntax
 *
 * \note Message can be wrote after that this function has returned.
 * \note This function is thread-safe
 */
void so_log_write(enum so_log_level level, const char * format, ...) __attribute__ ((format (printf, 2, 3)));
void so_log_write2(enum so_log_level level, enum so_log_type type, const char * format, ...) __attribute__ ((format (printf, 3, 4)));

/*

 * \brief Disable display remains messages.
 *
 * Default behaviour is to display all messages into terminal
 * if no log modules are loaded at exit of process.
void so_log_disable_display_log(void);

 * \brief Start thread which write messages to log modules
 *
 * \note Should be used only one time
 * \note This function is thread-safe
void so_log_start_logger(void);

void so_log_stop_logger();

 * \brief Write a message to all log modules
 *
 * \param[in] level : level of message
 * \param[in] type : type of message
 * \param[in] user : user associted to this message
 * \param[in] format : message with printf-like syntax
 *
 * \note Message can be wrote after that this function has returned.
 * \note This function is thread-safe
void so_log_write_all2(enum so_log_level level, enum so_log_type type, struct so_user * user, const char * format, ...) __attribute__ ((format (printf, 4, 5)));

*/

#endif

