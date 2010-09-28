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
*  Last modified: Tue, 28 Sep 2010 17:55:31 +0200                       *
\***********************************************************************/

// free, malloc
#include <malloc.h>
// strdup
#include <string.h>

#include <storiqArchiver/database.h>
#include <storiqArchiver/hashtable.h>

#include "connection.h"

static int db_postgresql_setup(struct database * db, struct hashtable * params);

static struct database_ops db_postgresql_ops = {
	.connect =	0,
	.ping =		0,
	.setup =	db_postgresql_setup,
};

static struct database db_postgresql = {
	.driverName =	"postgresql",
	.ops =			&db_postgresql_ops,
	.data =			0,
	.cookie =		0,
};

__attribute__((constructor))
static void db_postgresql_init() {
	db_registerDb(&db_postgresql);
}

void db_postgresql_prFree(struct db_postgresql_private * self) {
	if (!self)
		return;

	if (self->user)
		free(self->user);
	self->user = 0;
	if (self->password)
		free(self->password);
	self->password = 0;
	if (self->db)
		free(self->db);
	self->db = 0;
	if (self->host)
		free(self->host);
	self->host = 0;
	if (self->port)
		free(self->port);
	self->port = 0;
}

int db_postgresql_setup(struct database * db, struct hashtable * params) {
	if (!db)
		return 1;

	struct db_postgresql_private * self = db->data;
	if (self)
		db_postgresql_prFree(self);
	else
		self = malloc(sizeof(struct db_postgresql_private));

	self->user = strdup(hashtable_value(params, "user"));
	self->password = strdup(hashtable_value(params, "password"));
	self->db = strdup(hashtable_value(params, "db"));
	self->host = strdup(hashtable_value(params, "host"));
	self->port = strdup(hashtable_value(params, "port"));

	return 0;
}

