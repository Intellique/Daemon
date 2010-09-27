/***********************************************************************\
*       ______           ____     ___           __   _                  *
*      / __/ /____  ____/  _/__ _/ _ | ________/ /  (_)  _____ ____     *
*     _\ \/ __/ _ \/ __// // _ `/ __ |/ __/ __/ _ \/ / |/ / -_) __/     *
*    /___/\__/\___/_/ /___/\_, /_/ |_/_/  \__/_//_/_/|___/\__/_/        *
*                           /_/                                         *
*  -------------------------------------------------------------------  *
*  This file is a part of StorIqArchiver                                *
*                                                                       *
*  StorIqArchiver is free software; you can redistribute it and/or      *
*  modify it under the terms of the GNU General Public License          *
*  as published by the Free Software Foundation; either version 3       *
*  of the License, or (at your option) any later version.               *
*                                                                       *
*  This program is distributed in the hope that it will be useful,      *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of       *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
*  GNU General Public License for more details.                         *
*                                                                       *
*  You should have received a copy of the GNU General Public License    *
*  along with this program; if not, write to the Free Software          *
*  Foundation, Inc., 51 Franklin Street, Fifth Floor,                   *
*  Boston, MA  02110-1301, USA.                                         *
*                                                                       *
*  -------------------------------------------------------------------  *
*  Copyright (C) 2010, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Mon, 27 Sep 2010 16:20:22 +0200                       *
\***********************************************************************/

// open
#include <fcntl.h>
// snprintf, sscanf
#include <stdio.h>
// strcmp, strrchr
#include <string.h>
// open
#include <sys/stat.h>
// open
#include <sys/types.h>
// access, close, read, readlink, unlink, write
#include <unistd.h>

#include "conf.h"


int conf_checkPid(int pid) {
	char path[64];
	snprintf(path, 64, "/proc/%d/exe", pid);

	if (access(path, F_OK))
		return 0;

	char link[128];
	readlink(path, link, 128);

	char * ptr = strrchr(link, '/');
	if (ptr)
		ptr++;
	else
		ptr = link;

	return strcmp(link, "storiqArchiver") ? -1 : 1;
}

int conf_deletePid(const char * pidFile) {
	return unlink(pidFile);
}

int conf_readPid(const char * pidFile) {
	if (access(pidFile, R_OK))
		return -1;

	int fd = open(pidFile, O_RDONLY);
	char buffer[16];
	read(fd, buffer, 16);
	close(fd);

	int pid = 0;
	if (sscanf(buffer, "%d", &pid) == 1)
		return pid;

	return -1;
}

int conf_writePid(const char * pidFile, int pid) {
	int fd = open(pidFile, O_RDONLY | O_TRUNC | O_CREAT, 0644);
	if (fd < 0)
		return 1;

	char buffer[16];
	int length = 0;
	snprintf(buffer, 16, "%d\n%n", pid, &length);

	int nbWrite = write(fd, buffer, length);
	close(fd);

	return nbWrite != length;
}


int conf_readConfig(const char * confFile) {
	return 0;
}

