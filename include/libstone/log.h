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

#ifndef __LIBSTONE_LOG_H__
#define __LIBSTONE_LOG_H__

// time_t
#include <sys/time.h>

struct st_value;

/**
 * \enum st_log_level
 * \brief Enumerate level of message
 *
 * Each level has a priority (i.e. debug > info > warning > error)
 */
enum st_log_level {
	/**
	 * \brief action must be taken immendiately
	 */
	st_log_level_alert = 0x1,

	/**
	 * \brief critical condictions
	 */
	st_log_level_critical = 0x2,

	/**
	 * \brief Reserved for debugging
	 */
	st_log_level_debug = 0x7,

	/**
	 * \brief System unstable
	 */
	st_log_level_emergencey = 0x0,

	/**
	 * \brief Should be used to alert errors
	 */
	st_log_level_error = 0x3,

	/**
	 * \brief Should be used to inform what the server does.
	 *
	 * Example: server starts a new job
	 */
	st_log_level_info = 0x6,

	/**
	 * \brief normal but significant condition
	 */
	st_log_level_notice = 0x5,

	/**
	 * \brief Should be used to alert errors which can be recovered
	 *
	 * Example: dns service not available
	 */
	st_log_level_warning = 0x4,

	/**
	 * \brief Should not be used
	 *
	 * Used only by st_log_string_to_level to report an error
	 * \see st_log_string_to_level
	 */
	st_log_level_unknown = -1,
};

/**
 * \enum st_log_type
 * \brief Enumerate type of message
 */
enum st_log_type {
	/**
	 * \brief used by changer
	 */
	st_log_type_changer,
	/**
	 * \brief generic message from daemon
	 */
	st_log_type_daemon,
	/**
	 * \brief used by tape drive
	 */
	st_log_type_drive,
	/**
	 * \brief used by jobs
	 */
	st_log_type_job,
	st_log_type_logger,
	/**
	 * \brief used by checksum module
	 */
	st_log_type_plugin_checksum,
	/**
	 * \brief used by database module
	 */
	st_log_type_plugin_db,
	/**
	 * \brief used by log module
	 */
	st_log_type_plugin_log,
	/**
	 * \brief used by scheduler
	 */
	st_log_type_scheduler,

	/**
	 * \brief used by user interface
	 */
	st_log_type_ui,
	st_log_type_user_message,

	/**
	 * \brief Should not be used
	 *
	 * Used only by st_log_string_to_type to report an error
	 * \see st_log_string_to_type
	 */
	st_log_type_unknown,
};


void st_log_configure(struct st_value * config, enum st_log_type default_type);

/**
 * \brief Convert an enumeration to a statically allocated string
 *
 * \param level : one log level
 * \return a statically allocated string
 * \note returned value should not be released
 */
const char * st_log_level_to_string(enum st_log_level level);

void st_log_stop_logger(void);

/**
 * \brief Convert a c string to an enumeration
 *
 * \param string : one string level
 * \return an enumeration
 */
enum st_log_level st_log_string_to_level(const char * string);

/**
 * \brief Convert a c string to an enum st_log_type
 *
 * \param string : one string type
 * \return an enumeration
 */
enum st_log_type st_log_string_to_type(const char * string);

/**
 * \brief Convert an enum st_log_type to a statically allocated c string
 *
 * \param[in] type : a log type
 * \return a statically allocated c string
 *
 * \note st_log_type_to_string never returns NULL and you <b>SHOULD NOT RELEASE</b> returned value
 */
const char * st_log_type_to_string(enum st_log_type type);

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
void st_log_write(enum st_log_level level, const char * format, ...) __attribute__ ((format (printf, 2, 3)));
void st_log_write2(enum st_log_level level, enum st_log_type type, const char * format, ...) __attribute__ ((format (printf, 3, 4)));

/*

 * \brief Disable display remains messages.
 *
 * Default behaviour is to display all messages into terminal
 * if no log modules are loaded at exit of process.
void st_log_disable_display_log(void);

 * \brief Start thread which write messages to log modules
 *
 * \note Should be used only one time
 * \note This function is thread-safe
void st_log_start_logger(void);

void st_log_stop_logger();

 * \brief Write a message to all log modules
 *
 * \param[in] level : level of message
 * \param[in] type : type of message
 * \param[in] user : user associted to this message
 * \param[in] format : message with printf-like syntax
 *
 * \note Message can be wrote after that this function has returned.
 * \note This function is thread-safe
void st_log_write_all2(enum st_log_level level, enum st_log_type type, struct st_user * user, const char * format, ...) __attribute__ ((format (printf, 4, 5)));

*/

#endif

