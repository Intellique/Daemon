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

#ifndef __STONE_THREADPOOL_H__
#define __STONE_THREADPOOL_H__

typedef void (*so_thread_pool_f)(void * arg);

/**
 * \brief Run this function into another thread
 *
 * \param[in] function : call this function from another thread
 * \param[in] arg : call this function by passing this argument
 * \returns 0 if OK
 *
 * \note this function reuse an unused thread or create new one
 *
 * \note All threads which are not used while 5 minutes are stopped
 */
int so_thread_pool_run(const char * thread_name, so_thread_pool_f callback, void * arg);

/**
 * \brief Run this function into another thread with specified
 * \a nice priority.
 *
 * \param[in] function : call this function from another thread
 * \param[in] arg : call this function by passing this argument
 * \param[in] nice : call nice(2) before call \a function
 * \returns 0 if OK
 *
 * \note this function reuse an unused thread or create new one
 *
 * \note All threads which are not used while 5 minutes are stopped
 */
int so_thread_pool_run2(const char * thread_name, so_thread_pool_f callback, void * arg, int nice);

void so_thread_pool_set_name(const char * name);

#endif
