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
*  Copyright (C) 2012, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Mon, 09 Jul 2012 13:32:26 +0200                         *
\*************************************************************************/

#ifndef __STONE_UTIL_FILE_H__
#define __STONE_UTIL_FILE_H__

// gid_t, ssize_t
#include <sys/types.h>

/**
 * \brief Convert \a size to humain readeable format (i.e. 30KB)
 *
 * \param[in] size : convert this \a size
 * \param[out] str : an allocated string which will contain result
 * \param[in] str_len : length of \a str in bytes
 */
void st_util_file_convert_size_to_string(ssize_t size, char * str, ssize_t str_len);

/**
 * \brief Convert group's name from \a gid
 *
 * \param[out] name : write login into it, name should be allocated
 * \param[in] length : length of \a name
 * \param[in] gid : a known \a gid
 *
 * \note if \a gid is not found, write gid number into \a name
 */
void st_util_file_gid2name(char * name, ssize_t length, gid_t gid);

/**
 * \brief Trunc \a nb_trunc_path directories from \a path
 *
 * \param[in] path : a \a path
 * \param[in] nb_trunc_path : number of directories to trunc
 * \returns a pointer on \a path or \b NULL
 */
char * st_util_file_trunc_path(char * path, int nb_trunc_path);

/**
 * \brief Convert user's name from \a uid
 *
 * \param[out] name write user's name into it, \a name should be allocated
 * \param[in] length : length of \a name
 * \param[in] uid : a known \a uid
 *
 * \note if \a uid is not found, write uid number into \a name
 */
void st_util_file_uid2name(char * name, ssize_t length, uid_t uid);

#endif

