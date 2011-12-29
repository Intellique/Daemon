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
*  Last modified: Sat, 17 Dec 2011 19:07:29 +0100                         *
\*************************************************************************/

#ifndef __STONE_CONF_H__
#define __STONE_CONF_H__

/**
 * \brief st_conf_check_pid
 * \param[in] pid : pid
 * \return a value which correspond to
 * \li 1 is the daemon is alive
 * \li 0 if the daemon is dead
 * \li -1 if another process used this pid
 */
int st_conf_check_pid(int pid);

/**
 * \brief st_conf_delete_pid
 * \param[in] pid_file : file with pid
 * \return what "unlink" returned
 * \note see man page unlink(2)
 */
int st_conf_delete_pid(const char * pid_file);

/**
 * \brief st_conf_read_pid read pid file
 * \param[in] pid_file : file with pid
 * \return the pid or -1 if not found
 */
int st_conf_read_pid(const char * pid_file);

/**
 * \brief st_conf_write_pid write pid into file
 * \param[in] pid_file : file with pid
 * \param[in] pid : pid
 * \return 0 if ok
 */
int st_conf_write_pid(const char * pid_file, int pid);


/**
 * \brief st_read config file
 * \param[in] conf_file : config file
 * \return a value which correspond to
 * \li 0 if ok
 * \li 1 if error
 */
int st_conf_read_config(const char * conf_file);

#endif

