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

#ifndef __STONED_CONF_H__
#define __STONED_CONF_H__

struct st_value;

/**
 * \brief Callback function
 *
 * \param[in] params : All keys values associated to section
 */
typedef void (*st_conf_callback_f)(const struct st_value * params);

/**
 * \brief st_read config file
 *
 * \param[in] conf_file : config file
 * \return a value which correspond to
 * \li 0 if ok
 * \li 1 if error
 */
int st_conf_read_config(const char * conf_file);

/**
 * \brief Register a function which will be called if a \a section is found
 *
 * First, register a function \a callback associated to a \a section.
 * Then, call st_conf_read_config and if \a section is found, the function 
 * \a callback will be called.
 *
 * \param[in] section : a section name
 * \param[in] callback : a function called if \a section is found into a config file.
 */
void st_conf_register_callback(const char * section, st_conf_callback_f callback);

#endif

