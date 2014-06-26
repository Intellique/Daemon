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

#ifndef __STONECHANGER_CHANGER_H__
#define __STONECHANGER_CHANGER_H__

#include <libstone/changer.h>

struct st_database_connection;
struct st_value;

struct st_changer_driver {
	const char * name;

	struct st_changer * device;
	int (*configure_device)(struct st_value * config);

	unsigned int api_level;
	const char * src_checksum;
};

struct st_changer_ops {
	int (*init)(struct st_value * config, struct st_database_connection * db_connection);
	int (*put_offline)(void);
	int (*put_online)(void);
};

void stchgr_changer_register(struct st_changer_driver * chngr);

#endif

