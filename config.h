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

#ifndef __STONE_CONFIG_H__
#define __STONE_CONFIG_H__

#define DAEMON_CONFIG_FILE "/etc/storiq/stone.conf"
#define DAEMON_SOCKET_DIR "/var/run/stoned"
#define DAEMON_PID_FILE "/var/run/stoned.pid"
#define DAEMON_BIN_DIR "/usr/lib/stoned/bin"
#define LOCALE_DIR "/usr/share/locale/"
#define MODULE_PATH "/usr/lib/stoned"
#define SCRIPT_PATH "/var/lib/stoned"
#define TMP_DIR "/tmp"

#define ADMIN_DEFAULT_HOST "localhost"
#define ADMIN_DEFAULT_PORT 4862

#endif

