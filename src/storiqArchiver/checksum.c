/*************************************************************************\
*        ______           ____     ___           __   _                   *
*       / __/ /____  ____/  _/__ _/ _ | ________/ /  (_)  _____ ____      *
*      _\ \/ __/ _ \/ __// // _ `/ __ |/ __/ __/ _ \/ / |/ / -_) __/      *
*     /___/\__/\___/_/ /___/\_, /_/ |_/_/  \__/_//_/_/|___/\__/_/         *
*                            /_/                                          *
*  ---------------------------------------------------------------------  *
*  This file is a part of StorIqArchiver                                  *
*                                                                         *
*  StorIqArchiver is free software; you can redistribute it and/or        *
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
*  Copyright (C) 2011, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Mon, 21 Nov 2011 16:32:32 +0100                         *
\*************************************************************************/

// realloc
#include <stdlib.h>
// snprintf
#include <stdio.h>
// strcmp
#include <string.h>

#include <storiqArchiver/checksum.h>
#include <storiqArchiver/log.h>

#include "loader.h"

static struct sa_checksum_driver ** sa_checksum_drivers = 0;
static unsigned int sa_checksum_nb_drivers = 0;

char * sa_checksum_compute(const char * checksum, const char * data, unsigned int length) {
	if (!checksum || !data || length == 0)
		return 0;

	struct sa_checksum_driver * driver = sa_checksum_get_driver(checksum);
	if (!driver) {
		sa_log_write_all(sa_log_level_error, "Checksum: Driver %s not found", checksum);
		return 0;
	}

	struct sa_checksum chck;

	driver->new_checksum(&chck);
	chck.ops->update(&chck, data, length);

	char * digest = chck.ops->digest(&chck);

	chck.ops->free(&chck);

	sa_log_write_all(sa_log_level_debug, "Checksum: compute %s checksum => %s", checksum, digest);

	return digest;
}

void sa_checksum_convert_to_hex(unsigned char * digest, int length, char * hexDigest) {
	if (!digest || length < 1 || !hexDigest) {
		sa_log_write_all(sa_log_level_error, "Checksum: while sa_checksum_convert_to_hex");
		if (!digest)
			sa_log_write_all(sa_log_level_error, "Checksum: error because digest is null");
		if (length < 1)
			sa_log_write_all(sa_log_level_error, "Checksum: error because length is lower than 1 (length=%d)", length);
		if (!hexDigest)
			sa_log_write_all(sa_log_level_error, "Checksum: error because hexDigest is null");
		return;
	}

	int i;
	for (i = 0; i < length; i++)
		snprintf(hexDigest + (i << 1), 3, "%02x", digest[i]);
	hexDigest[i << 1] = '\0';

	sa_log_write_all(sa_log_level_debug, "Checksum: computed => %s", hexDigest);
}

struct sa_checksum_driver * sa_checksum_get_driver(const char * driver) {
	unsigned int i;
	for (i = 0; i < sa_checksum_nb_drivers; i++)
		if (!strcmp(driver, sa_checksum_drivers[i]->name))
			return sa_checksum_drivers[i];
	void * cookie = sa_loader_load("checksum", driver);
	if (!cookie) {
		sa_log_write_all(sa_log_level_error, "Checksum: Failed to load driver %s", driver);
		return 0;
	}
	for (i = 0; i < sa_checksum_nb_drivers; i++)
		if (!strcmp(driver, sa_checksum_drivers[i]->name)) {
			struct sa_checksum_driver * driver = sa_checksum_drivers[i];
			driver->cookie = cookie;
			return driver;
		}
	sa_log_write_all(sa_log_level_error, "Checksum: Driver %s not found", driver);
	return 0;
}

void sa_checksum_register_driver(struct sa_checksum_driver * driver) {
	if (!driver) {
		sa_log_write_all(sa_log_level_error, "Checksum: Try to register with driver=0");
		return;
	}

	if (driver->api_version != STORIQARCHIVER_CHECKSUM_APIVERSION) {
		sa_log_write_all(sa_log_level_error, "Checksum: Driver(%s) has not the correct api version (current: %d, expected: %d)", driver->name, driver->api_version, STORIQARCHIVER_CHECKSUM_APIVERSION);
		return;
	}

	unsigned int i;
	for (i = 0; i < sa_checksum_nb_drivers; i++)
		if (sa_checksum_drivers[i] == driver || !strcmp(driver->name, sa_checksum_drivers[i]->name)) {
			sa_log_write_all(sa_log_level_error, "Checksum: Driver(%s) is already registred", driver->name);
			return;
		}

	sa_checksum_drivers = realloc(sa_checksum_drivers, (sa_checksum_nb_drivers + 1) * sizeof(struct sa_checksum_driver *));
	sa_checksum_drivers[sa_checksum_nb_drivers] = driver;
	sa_checksum_nb_drivers++;

	sa_loader_register_ok();

	sa_log_write_all(sa_log_level_info, "Checksum: Driver(%s) is now registred", driver->name);
}

