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
*  Last modified: Fri, 25 May 2012 16:00:09 +0200                         *
\*************************************************************************/

#ifndef __STONE_CONFIG_H__
#define __STONE_CONFIG_H__

#define DAEMON_CONFIG_FILE "/etc/storiq/stoned.conf"
//#define DAEMON_CONFIG_FILE "example-config.conf"
#define DAEMON_PID_FILE "/var/run/stoned.pid"
//#define DAEMON_PID_FILE "stone.pid"

//#define MODULE_PATH "/usr/lib/stone"
#define MODULE_PATH "lib"

#define ADMIN_DEFAULT_HOST "localhost"
#define ADMIN_DEFAULT_PORT 4862

#endif

