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

#ifndef __LIBSTONE_FILE_H__
#define __LIBSTONE_FILE_H__

// bool
#include <stdbool.h>
// gid_t, mode_t, ssize_t
#include <sys/types.h>

struct dirent;

/**
 * \brief Basic function which is designed to be used by scandir
 *
 * This function, when used by scandir, remove only files named '.' and '..'
 *
 * \param[in] d : directory information
 * \returns 0 if d->d_name equals '.' or '..'
 */
int st_file_basic_scandir_filter(const struct dirent * d);

/**
 * \brief Convert a file mode to \b buffer with `ls -l` style
 * \param[out] buffer : a 10 bytes already allocated buffer
 * \param[in] mode : convert with this mode
 */
void st_file_convert_mode(char * buffer, mode_t mode);

/**
 * \brief Convert \a size to humain readeable format (i.e. 30KB)
 *
 * \param[in] size : convert this \a size
 * \param[out] str : an allocated string which will contain result
 * \param[in] str_len : length of \a str in bytes
 */
void st_file_convert_size_to_string(size_t size, char * str, ssize_t str_len);

int st_file_cp(const char * src, const char * dst);

/**
 * \brief Create directory recursively
 *
 * \param[in] dirname : a directory name
 * \param[in] mode : create directory with specific mode
 * \returns 0 if ok or read errno
 */
int st_file_mkdir(const char * dirname, mode_t mode);

int st_file_mv(const char * src, const char * dst);

/**
 * \brief Remove recursively path
 *
 * \param[in] path : a path that will be deleted
 * \returns 0 if ok or read errno
 */
int st_file_rm(const char * path);

#endif

