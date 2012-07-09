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
*  Last modified: Mon, 09 Jul 2012 12:19:28 +0200                         *
\*************************************************************************/

#ifndef __STONE_UTIL_UTIL_H__
#define __STONE_UTIL_UTIL_H__

/**
 * \brief Basic function to release memory
 *
 * Use it if \a key and \a value are malloc allocated memory.\n
 * This function use \a free to release memory.
 *
 * \param[in] key : a key
 * \param[in] value : a value
 *
 * \note \a key can be equal to \a value
 *
 * \see st_hashtable_new2
 */
void st_util_basic_free(void * key, void * value);

#endif

