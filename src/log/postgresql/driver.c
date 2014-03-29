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

// realloc
#include <malloc.h>

#include <liblog-postgresql.chcksum>

#include "common.h"

static struct lgr_log_module * st_log_postgresql_add_module(struct st_value * params);
static void st_log_postgresql_init(void) __attribute__((constructor));


static struct lgr_log_driver st_log_postgresql_driver = {
	.name       = "postgresql",
	.new_module = st_log_postgresql_add_module,
	.cookie     = NULL,
	.api_level  = 0,
	.src_checksum = STONE_LOG_POSTGRESQL_SRCSUM,
};


static struct lgr_log_module * st_log_postgresql_add_module(struct st_value * params) {
	struct lgr_log_module * mod = st_log_postgresql_new_module(params);
	if (mod != NULL)
		mod->driver = &st_log_postgresql_driver;
	return mod;
}

static void st_log_postgresql_init(void) {
	lgr_log_register_driver(&st_log_postgresql_driver);
}

