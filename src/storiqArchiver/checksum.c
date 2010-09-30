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
*  Last modified: Thu, 30 Sep 2010 10:55:36 +0200                       *
\***********************************************************************/

// dlerror, dlopen
#include <dlfcn.h>
// realloc
#include <malloc.h>
// snprintf
#include <stdio.h>
// strcasecmp, strcmp
#include <string.h>
// access
#include <unistd.h>

#include <storiqArchiver/checksum.h>

#include "config.h"

static struct checksum_driver ** checksum_drivers = 0;
static unsigned int checksum_nbDrivers = 0;


void checksum_convert2Hex(unsigned char * digest, int length, char * hexDigest) {
	int i;
	for (i = 0; i < length; i++)
		snprintf(hexDigest + (i << 1), 2, "%02x", digest[i]);
	hexDigest[i << 1] = '\0';
}

struct checksum_driver * checksum_getDriver(const char * driver) {
	unsigned int i;
	for (i = 0; i < checksum_nbDrivers; i++)
		if (!strcmp(checksum_drivers[i]->name, driver))
			return checksum_drivers[i];
	if (!checksum_loadDriver(driver))
		return checksum_getDriver(driver);
	return 0;
}

int checksum_loadDriver(const char * driver) {
	char path[128];
	snprintf(path, 128, "%s/lib%s.so", CHECKSUM_DIRNAME, driver);

	// check if module is already loaded
	unsigned int i;
	for (i = 0; i < checksum_nbDrivers; i++)
		if (!strcmp(checksum_drivers[i]->name, driver))
			return 0;

	// check if you can load module
	if (access(path, R_OK | X_OK))
		return 1;

	// load module
	void * cookie = dlopen(path, RTLD_NOW);

	if (!cookie) {
		printf("Error while loading %s => %s\n", path, dlerror());
		return 2;
	} else if (checksum_nbDrivers > 0 && !strcmp(checksum_drivers[checksum_nbDrivers - 1]->name, driver)) {
		checksum_drivers[checksum_nbDrivers - 1]->cookie = cookie;
	} else {
		// module didn't call log_registerModule so we unload it
		dlclose(cookie);
		return 2;
	}

	return 0;
}

void checksum_registerDriver(struct checksum_driver * driver) {
	checksum_drivers = realloc(checksum_drivers, (checksum_nbDrivers + 1) * sizeof(struct checksum_driver *));

	checksum_drivers[checksum_nbDrivers] = driver;
	checksum_nbDrivers++;
}

