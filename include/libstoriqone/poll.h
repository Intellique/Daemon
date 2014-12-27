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
*  Copyright (C) 2014, Clercin guillaume <gclercin@intellique.com>           *
\****************************************************************************/

#ifndef __LIBSTORIQONE_POLL_H__
#define __LIBSTORIQONE_POLL_H__

// bool
#include <stdbool.h>
// pool events
#include <poll.h>

typedef void (*so_poll_callback_f)(int fd, short event, void * data);
typedef void (*so_poll_free_f)(void * data);
typedef void (*so_poll_timeout_f)(int fd, void * data);

int so_poll(int timeout);
unsigned int so_poll_nb_handlers(void);
bool so_poll_register(int fd, short event, so_poll_callback_f callback, void * data, so_poll_free_f release);
bool so_poll_set_timeout(int fd, int timeout, so_poll_timeout_f callback);
void so_poll_unregister(int fd, short event);
bool so_poll_unset_timeout(int fd);

#endif

