/***********************************************************************\
*       ______           ____     ___           __   _                  *
*      / __/ /____  ____/  _/__ _/ _ | ________/ /  (_)  _____ ____     *
*     _\ \/ __/ _ \/ __// // _ `/ __ |/ __/ __/ _ \/ / |/ / -_) __/     *
*    /___/\__/\___/_/ /___/\_, /_/ |_/_/  \__/_//_/_/|___/\__/_/        *
*                           /_/                                         *
*  -------------------------------------------------------------------  *
*  This file is a part of StorIqArchiver                                *
*                                                                       *
*  StorIqArchiver is free software; you can redistribute it and/or      *
*  modify it under the terms of the GNU General Public License          *
*  as published by the Free Software Foundation; either version 3       *
*  of the License, or (at your option) any later version.               *
*                                                                       *
*  This program is distributed in the hope that it will be useful,      *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of       *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
*  GNU General Public License for more details.                         *
*                                                                       *
*  You should have received a copy of the GNU General Public License    *
*  along with this program; if not, write to the Free Software          *
*  Foundation, Inc., 51 Franklin Street, Fifth Floor,                   *
*  Boston, MA  02110-1301, USA.                                         *
*                                                                       *
*  -------------------------------------------------------------------  *
*  Copyright (C) 2010, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Fri, 15 Oct 2010 17:07:21 +0200                       *
\***********************************************************************/

#ifndef __STORIQARCHIVER_DB_POSTGRESQL_CONNNECTION_H__
#define __STORIQARCHIVER_DB_POSTGRESQL_CONNNECTION_H__

#include <postgresql/libpq-fe.h>
#include <storiqArchiver/database.h>

struct db_postgresql_private {
	char * user;
	char * password;
	char * db;
	char * host;
	char * port;
};

struct db_postgresql_connetion_private {
	PGconn * db_con;
};

int db_postgresql_initConnection(struct database_connection * connection, struct db_postgresql_private * driver_private);
void db_postgresql_prFree(struct db_postgresql_private * self);

#endif

