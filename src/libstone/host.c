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
*  Copyright (C) 2013, Clercin guillaume <gclercin@intellique.com>           *
*  Last modified: Thu, 26 Dec 2013 13:45:39 +0100                            *
\****************************************************************************/

// NULL
#include <stddef.h>
// uname
#include <sys/utsname.h>

#include <libstone/database.h>
#include <libstone/host.h>

static struct st_host host = { NULL, NULL };


bool st_host_init(struct st_database_connection * connect) {
	if (connect == NULL)
		return false;

	struct utsname name;
	uname(&name);

	int failed = connect->ops->get_host_by_name(connect, name.nodename, &host);
	return failed == 0;
}

const struct st_host * st_host_get() {
	return &host;
}

