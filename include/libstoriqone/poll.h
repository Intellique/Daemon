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

#ifndef __LIBSTONE_POLL_H__
#define __LIBSTONE_POLL_H__

// bool
#include <stdbool.h>
// pool events
#include <poll.h>

typedef void (*st_poll_callback_f)(int fd, short event, void * data);
typedef void (*st_poll_free_f)(void * data);
typedef void (*st_poll_timeout_f)(int fd, void * data);

int st_poll(int timeout);
unsigned int st_poll_nb_handlers(void);
bool st_poll_register(int fd, short event, st_poll_callback_f callback, void * data, st_poll_free_f release);
bool st_poll_set_timeout(int fd, int timeout, st_poll_timeout_f callback);
void st_poll_unregister(int fd, short event);

#endif

