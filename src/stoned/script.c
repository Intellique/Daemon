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
*  Last modified: Mon, 11 Nov 2013 12:28:32 +0100                            *
\****************************************************************************/

#include <stddef.h>

#define _GNU_SOURCE
// glob, globfree
#include <glob.h>

#include <libstone/database.h>

#include "config.h"
#include "script.h"

void st_script_sync(struct st_database_connection * connection) {
	glob_t gl;
	glob(SCRIPT_PATH "/*", 0, NULL, &gl);

	unsigned int i;
	for (i = 0; i < gl.gl_pathc; i++) {
		connection->ops->sync_script(connection, gl.gl_pathv[i]);
	}

	globfree(&gl);
}

