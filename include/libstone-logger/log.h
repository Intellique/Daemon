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

#ifndef __STONELOGGER_LOG_H__
#define __STONELOGGER_LOG_H__

// bool
#include <stdbool.h>

#include <libstone/log.h>

struct st_value;

struct lgr_log_module;

/**
 * \struct lgr_log_driver
 * \brief Structure used used only one time by each log module
 *
 * \note This structure should be staticaly allocated and passed to function st_log_register_driver
 * \see st_log_register_driver
 */
struct lgr_log_driver {
	/**
	 * \brief Name of driver
	 *
	 * \note Should be unique and equals to liblog-name.so where name is the name of driver.
	 */
	const char * name;

	struct lgr_log_module * (*new_module)(struct st_value * param) __attribute__((nonnull,warn_unused_result));

	/**
	 * \brief cookie
	 *
	 * Is a value returns by dlopen and should not be used nor released
	 */
	void * cookie;
	unsigned int api_level;
	const char * src_checksum;
};

struct lgr_log_module {
	/**
	 * \brief Minimal level
	 */
	enum st_log_level level;

	/**
	 * \struct lgr_log_module_ops
	 * \brief Functions associated to a log module
	 */
	struct lgr_log_module_ops {
		/**
		 * \brief release a module
		 *
		 * \param[in] module : release this module
		 */
		void (*free)(struct lgr_log_module * module);
		/**
		 * \brief Write a message to a module
		 *
		 * \param[in] module : write to this module
		 * \param[in] message : write this message
		 */
		void (*write)(struct lgr_log_module * module, struct st_value * message);
	} * ops;

	struct lgr_log_driver * driver;
	/**
	 * \brief Private data of a log module
	 */
	void * data;
};

bool lgr_log_load(struct st_value * params);

/**
 * \brief Each log driver should call this function only one time
 *
 * \param driver : a static allocated structure
 * \code
 * \_\_attribute\_\_((constructor))
 * static void log_myLog_init() {
 *    lgr_log_register_driver(&log_myLog_module);
 * }
 * \endcode
 */
void lgr_log_register_driver(struct lgr_log_driver * driver);

void lgr_log_write(struct st_value * message);

#endif

