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
*  Copyright (C) 2013-2015, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#ifndef __STORIQONE_CONFIG_H__
#define __STORIQONE_CONFIG_H__

#define DAEMON_CONFIG_FILE "/etc/storiq/storiqone.conf"
#define DAEMON_CRASH_DIR "tmp/crash"
#define DAEMON_SOCKET_DIR "/var/run/storiqone"
#define DAEMON_PID_FILE "/var/run/storiqone.pid"
#define DAEMON_BIN_DIR "/usr/lib/storiqone/1.2/bin"
#define DAEMON_JOB_DIR "/usr/lib/storiqone/1.2/job"
#define LOCALE_DIR "/usr/share/locale/"
#define MODULE_PATH "lib"
#define SCRIPT_PATH "/usr/lib/storiqone/1.2/scripts"
#define TMP_DIR "/tmp"

#define ADMIN_DEFAULT_HOST "localhost"
#define ADMIN_DEFAULT_PORT 4862

#endif

