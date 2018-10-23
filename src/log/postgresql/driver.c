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
*  Copyright (C) 2013-2018, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// NULL
#include <stddef.h>

#include <liblog-postgresql.chcksum>

#include "common.h"

static struct solgr_log_module * so_log_postgresql_add_module(struct so_value * params);
static void so_log_postgresql_init(void) __attribute__((constructor));


static struct solgr_log_driver so_log_postgresql_driver = {
	.name       = "postgresql",
	.new_module = so_log_postgresql_add_module,
	.cookie     = NULL,
	.src_checksum = STORIQONE_LOG_POSTGRESQL_SRCSUM,
};


static struct solgr_log_module * so_log_postgresql_add_module(struct so_value * params) {
	struct solgr_log_module * mod = so_log_postgresql_new_module(params);
	if (mod != NULL)
		mod->driver = &so_log_postgresql_driver;
	return mod;
}

static void so_log_postgresql_init(void) {
	solgr_log_register_driver(&so_log_postgresql_driver);
}
