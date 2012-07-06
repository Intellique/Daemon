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
*  Last modified: Tue, 03 Jul 2012 23:55:36 +0200                         *
\*************************************************************************/

#ifndef __STONE_THREADPOOL_H__
#define __STONE_THREADPOOL_H__

/**
 * \brief Run this function into another thread
 *
 * \param[in] function : call this function from another thread
 * \param[in] arg : call this function by passing this argument
 *
 * \note this function reuse an unused thread or create new one
 *
 * \note All threads which are not used while 5 minutes are stopped
 */
void st_threadpool_run(void (*function)(void * arg), void * arg);

#endif

