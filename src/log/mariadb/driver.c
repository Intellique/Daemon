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
*  Copyright (C) 2013-2016, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// NULL
#include <stddef.h>

#include <liblog-mariadb.chcksum>

#include "common.h"

static struct lgr_log_module * st_log_mariadb_add_module(struct st_value * params);
static void st_log_mariadb_init(void) __attribute__((constructor));


static struct lgr_log_driver st_log_mariadb_driver = {
	.name       = "mariadb",
	.new_module = st_log_mariadb_add_module,
	.cookie     = NULL,
	.api_level  = 0,
	.src_checksum = STONE_LOG_MARIADB_SRCSUM,
};


static struct lgr_log_module * st_log_mariadb_add_module(struct st_value * params) {
	struct lgr_log_module * mod = st_log_mariadb_new_module(params);
	if (mod != NULL)
		mod->driver = &st_log_mariadb_driver;
	return mod;
}

static void st_log_mariadb_init(void) {
	lgr_log_register_driver(&st_log_mariadb_driver);
}

