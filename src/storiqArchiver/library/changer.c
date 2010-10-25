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
*  Last modified: Mon, 25 Oct 2010 16:52:41 +0200                       *
\***********************************************************************/

// calloc
#include <malloc.h>
//
#include <pthread.h>
// strcasecmp
#include <string.h>
// sleep
#include <unistd.h>

#include <storiqArchiver/database.h>
#include <storiqArchiver/library.h>
#include <storiqArchiver/log.h>

static void * library_changer_work(void * args);

static struct library_changer * library_changers = 0;
static pthread_t * library_changers_th = 0;
static int library_nbChangers = 0;

static struct changer_status {
	enum library_changer_status status;
	char * name;
} library_status[] = {
	{ library_changer_error,     "error" },
	{ library_changer_exporting, "exporting" },
	{ library_changer_idle,      "idle" },
	{ library_changer_importing, "importing" },
	{ library_changer_loading,   "loading" },
	{ library_changer_unknown,   "unknown" },
	{ library_changer_unloading, "unloading" },

	{ library_changer_unknown, 0 },
};


const char * library_changer_statusToString(enum library_changer_status status) {
	struct changer_status * ptr;
	for (ptr = library_status; ptr->name; ptr++)
		if (ptr->status == status)
			return ptr->name;
	return 0;
}

enum library_changer_status library_changer_stringToStatus(const char * status) {
	struct changer_status * ptr;
	for (ptr = library_status; ptr->name; ptr++)
		if (!strcasecmp(ptr->name, status))
			return ptr->status;
	return library_changer_unknown;
}

void library_configureChanger() {
	struct database * db = db_getDefaultDB();
	if (!db) {
		log_writeAll(Log_level_error, "Library: configureChanger: failed to get default database");
		return;
	}

	struct database_connection connection;
	if (!db->ops->connect(db, &connection)) {
		log_writeAll(Log_level_error, "Library: configureChanger: failed to get a connection to default database");
		return;
	}

	int nbChanger = connection.ops->getNbChanger(&connection);
	if (nbChanger > 0) {
		library_changers = calloc(nbChanger, sizeof(struct library_changer));
		library_nbChangers = nbChanger;

		int i;
		for (i = 0; i < nbChanger; i++)
			connection.ops->getChanger(&connection, library_changers + i, i);

		library_changers_th = calloc(nbChanger, sizeof(pthread_t));
		for (i = 0; i < nbChanger; i++)
			pthread_create(library_changers_th + i, 0, library_changer_work, library_changers + i);
	}
}

void * library_changer_work(void * args) {
	struct library_changer * changer = args;

	log_writeAll(Log_level_info, "Library: starting changer (model:%s, device:%s)", changer->model, changer->device);
	for (;;) {
		sleep(1);
	}

	return 0;
}

