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

#ifndef __LIBSTONE_DATABASE_H__
#define __LIBSTONE_DATABASE_H__

/**
 * \brief Get a database configuration by his name
 *
 * This configuration should be previous loaded.
 *
 * \param[in] name name of config
 * \returns \b NULL if not found
 */
struct st_database_config * st_database_get_config_by_name(const char * name) __attribute__((nonnull));

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
struct st_database * st_database_get_driver(const char * database) __attribute__((nonnull));

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
void st_database_register_driver(struct st_database * database) __attribute__((nonnull));

#endif

