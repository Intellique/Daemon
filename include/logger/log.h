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

#ifndef __LOGGER_LOG_H__
#define __LOGGER_LOG_H__

// bool
#include <stdbool.h>

#include <libstoriqone/log.h>

struct so_value;

struct solgr_log_module;

/**
 * \struct solgr_log_driver
 * \brief Structure used used only one time by each log module
 *
 * \note This structure should be staticaly allocated and passed to function so_log_register_driver
 * \see so_log_register_driver
 */
struct solgr_log_driver {
	/**
	 * \brief Name of driver
	 *
	 * \note Should be unique and equals to liblog-name.so where name is the name of driver.
	 */
	const char * name;

	struct solgr_log_module * (*new_module)(struct so_value * param) __attribute__((warn_unused_result));

	/**
	 * \brief cookie
	 *
	 * Is a value returns by dlopen and should not be used nor released
	 */
	void * cookie;
	const char * src_checksum;
};

struct solgr_log_module {
	/**
	 * \brief Minimal level
	 */
	enum so_log_level level;

	/**
	 * \struct solgr_log_module_ops
	 * \brief Functions associated to a log module
	 */
	struct solgr_log_module_ops {
		/**
		 * \brief release a module
		 *
		 * \param[in] module : release this module
		 */
		void (*free)(struct solgr_log_module * module);
		/**
		 * \brief Write a message to a module
		 *
		 * \param[in] module : write to this module
		 * \param[in] message : write this message
		 */
		void (*write)(struct solgr_log_module * module, struct so_value * message);
	} * ops;

	struct solgr_log_driver * driver;
	/**
	 * \brief Private data of a log module
	 */
	void * data;
};

bool solgr_log_load(struct so_value * params);

/**
 * \brief Each log driver should call this function only one time
 *
 * \param driver : a static allocated structure
 * \code
 * \_\_attribute\_\_((constructor))
 * static void log_myLog_init() {
 *    solgr_log_register_driver(&log_myLog_module);
 * }
 * \endcode
 */
void solgr_log_register_driver(struct solgr_log_driver * driver);

void solgr_log_write(struct so_value * message);

void solgr_log_write2(enum so_log_level level, enum so_log_type type, const char * format, ...) __attribute__ ((format (printf, 3, 4)));

#endif
