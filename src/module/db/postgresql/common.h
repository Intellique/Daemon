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
*  Last modified: Wed, 23 Nov 2011 12:00:46 +0100                         *
\*************************************************************************/

#ifndef __STORIQARCHIVER_DB_POSTGRESQL_CONNNECTION_H__
#define __STORIQARCHIVER_DB_POSTGRESQL_CONNNECTION_H__

#include <postgresql/libpq-fe.h>
#include <storiqArchiver/database.h>

struct sa_db_postgresql_private {
	char * user;
	char * password;
	char * db;
	char * host;
	char * port;
};

struct sa_db_postgresql_connetion_private {
	PGconn * db_con;
};

int sa_db_postgresql_init_connection(struct sa_database_connection * connection, struct sa_db_postgresql_private * driver_private);
void sa_db_postgresql_pr_free(struct sa_db_postgresql_private * self);

#endif

