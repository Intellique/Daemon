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

// NULL
#include <stddef.h>
// uname
#include <sys/utsname.h>

#include <libstone/database.h>
#include <libstone/value.h>

#include "host.h"

static struct st_value * host = NULL;

static void st_host_exit(void) __attribute__((destructor));


static void st_host_exit() {
	st_value_free(host);
}

__asm__(".symver st_host_init_v1, st_host_init@@LIBSTONE_1.2");
bool st_host_init_v1(struct st_database_connection * connect) {
	if (connect == NULL)
		return false;

	struct utsname name;
	uname(&name);

	host = connect->ops->get_host_by_name(connect, name.nodename);
	struct st_value * error = st_value_hashtable_get2(host, "error", false, false);

	return host != NULL && error->type == st_value_boolean && !st_value_boolean_get(error);
}

__asm__(".symver st_host_get_info_v1, st_host_get_info@@LIBSTONE_1.2");
struct st_value * st_host_get_info_v1() {
	if (host == NULL)
		return host;

	return st_value_share(host);
}

