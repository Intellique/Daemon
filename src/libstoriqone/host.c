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
*  Copyright (C) 2015, Guillaume Clercin <gclercin@intellique.com>           *
\****************************************************************************/

// free
#include <stdlib.h>
// uname
#include <sys/utsname.h>

#include <libstoriqone/database.h>
#include <libstoriqone/host.h>
#include <libstoriqone/value.h>

static struct so_host host = { NULL, NULL };

static void so_host_exit(void) __attribute__((destructor));


static void so_host_exit() {
	free(host.hostname);
	free(host.uuid);
}

bool so_host_init(struct so_database_connection * connect) {
	if (connect == NULL)
		return false;

	struct utsname name;
	uname(&name);

	int failed = connect->ops->get_host_by_name(connect, &host, name.nodename);

	return failed == 0;
}

struct so_host * so_host_get_info() {
	return &host;
}

struct so_value * so_host_get_info2() {
	return so_value_pack("{ssss}",
		"name", host.hostname,
		"uuid", host.uuid
	);
}

