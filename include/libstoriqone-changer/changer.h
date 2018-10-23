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

#ifndef __LIBSTORIQONE_CHANGER_CHANGER_H__
#define __LIBSTORIQONE_CHANGER_CHANGER_H__

// ssize_t
#include <sys/types.h>

#include <libstoriqone/changer.h>

struct so_database_connection;
struct so_media;
struct so_media_format;
struct so_value;
struct sochgr_peer;

struct so_changer_driver {
	const char * name;

	struct so_changer * device;
	int (*configure_device)(struct so_value * config);

	unsigned int api_level;
	const char * src_checksum;
};

struct so_changer_ops {
	int (*check)(unsigned int nb_clients, struct so_database_connection * db_connection);
	ssize_t (*get_reserved_space)(struct so_media_format * format);
	int (*init)(struct so_value * config, struct so_database_connection * db_connection);
	int (*load)(struct sochgr_peer * peer, struct so_slot * from, struct so_drive * to, struct so_database_connection * db_connection);
	int (*put_offline)(struct so_database_connection * db_connection);
	int (*put_online)(struct so_database_connection * db_connection);
	int (*shut_down)(struct so_database_connection * db_connection);
	int (*unload)(struct sochgr_peer * peer, struct so_drive * from, struct so_database_connection * db_connection);
};

void sochgr_changer_register(struct so_changer_driver * chngr);

#endif
