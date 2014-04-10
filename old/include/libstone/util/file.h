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
*  Last modified: Wed, 20 Nov 2013 13:06:28 +0100                            *
\****************************************************************************/

#ifndef __STONE_UTIL_FILE_H__
#define __STONE_UTIL_FILE_H__

// bool
#include <stdbool.h>
// gid_t, mode_t, ssize_t
#include <sys/types.h>

struct dirent;

char * st_util_file_get_serial(const char * filename);

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

bool st_util_file_write_to(const char * filename, const char * data, ssize_t length);

#endif

