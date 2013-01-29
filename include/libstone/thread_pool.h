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
*  Copyright (C) 2013, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Sun, 14 Oct 2012 21:30:38 +0200                         *
\*************************************************************************/

#ifndef __STONE_THREADPOOL_H__
#define __STONE_THREADPOOL_H__

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
int st_thread_pool_run(void (*function)(void * arg), void * arg);

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
int st_thread_pool_run2(void (*function)(void * arg), void * arg, int nice);

#endif

