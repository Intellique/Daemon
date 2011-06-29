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
*  Copyright (C) 2011, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Tue, 19 Oct 2010 11:11:56 +0200                       *
\***********************************************************************/

// free, realloc
#include <malloc.h>
// signal
#include <signal.h>
// time
#include <time.h>
// sleep
#include <unistd.h>

#include <storiqArchiver/database.h>
#include <storiqArchiver/job.h>
#include <storiqArchiver/log.h>

#include "scheduler.h"

static void sched_exit(int signal);
static short sched_stopRequest = 0;


void sched_doLoop() {
	log_writeAll(Log_level_info, "Scheduler: starting main loop");

	signal(SIGINT, sched_exit);

	static struct database * db;
	db = db_getDefaultDB();
	if (!db) {
		log_writeAll(Log_level_error, "Scheduler: there is no default database");
		return;
	}

	static struct database_connection connection;
	if (!db->ops->connect(db, &connection)) {
		log_writeAll(Log_level_error, "Scheduler: failed to connect to database");
		return;
	}

	static struct job ** jobs = 0;
	static unsigned int nbJobs = 0;

	static time_t lastUpdate = 0;

	while (!sched_stopRequest) {
		static short ok_transaction;
		static time_t update;

		ok_transaction = connection.ops->startTransaction(&connection, 1) >= 0;
		update = time(0);
		if (!ok_transaction)
			log_writeAll(Log_level_error, "Scheduler: error while starting transaction");

		if (nbJobs > 0) {
			static int nbModifiedJobs;
			nbModifiedJobs = connection.ops->jobModified(&connection, lastUpdate);

			if (nbModifiedJobs > 0) {
				log_writeAll(Log_level_debug, "Scheduler: There is modified jobs (%d)", nbModifiedJobs);

				static unsigned int i;
				static int j;
				for (i = 0, j = 0; i < nbJobs && j < nbModifiedJobs; i++) {
					static int ok_update;
					ok_update = connection.ops->updateJob(&connection, jobs[i]);
					if (ok_update > 0)
						j++;
					else if (ok_update < 0)
						log_writeAll(Log_level_error, "Scheduler: error while updating a job (job name=%s)", jobs[i]->name);
				}
			} else if (nbModifiedJobs == 0) {
				log_writeAll(Log_level_debug, "Scheduler: There is no modified jobs");
			} else {
				log_writeAll(Log_level_error, "Scheduler: error while fetching modified jobs");
			}
		}

		static int nbNewJobs;
		nbNewJobs = connection.ops->newJobs(&connection, lastUpdate);
		if (nbNewJobs > 0) {
			log_writeAll(Log_level_debug, "Scheduler: There is new jobs (%d)", nbNewJobs);

			jobs = realloc(jobs, (nbJobs + nbNewJobs) * sizeof(struct job *));

			static int i;
			for (i = 0; i < nbNewJobs; i++)
				jobs[nbJobs + i] = connection.ops->addJob(&connection, 0, i, lastUpdate);
			nbJobs += nbNewJobs;

		} else if (nbNewJobs == 0) {
			log_writeAll(Log_level_debug, "Scheduler: There is no new jobs");
		} else {
			log_writeAll(Log_level_error, "Scheduler: error while fetching new jobs");
		}

		ok_transaction = connection.ops->finishTransaction(&connection) >= 0;
		if (!ok_transaction)
			log_writeAll(Log_level_error, "Scheduler: error while finishing transaction");

		static short i;
		for (i = 0; i < 60 && !sched_stopRequest; i++)
			sleep(1);

		// TODO: scheduler stop request
		// if (sched_stopRequest) {}

		lastUpdate = update;
	}

	connection.ops->free(&connection);
}

void sched_exit(int signal) {
	if (signal == SIGINT) {
		log_writeAll(Log_level_warning, "Scheduler: catch SIGINT");
		sched_stopRequest = 1;
	}
}

