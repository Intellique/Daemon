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
*  Last modified: Sat, 17 Dec 2011 17:21:41 +0100                         *
\*************************************************************************/

// free, realloc
#include <malloc.h>
// signal
#include <signal.h>
// time
#include <time.h>
// sleep
#include <unistd.h>

#include <stone/database.h>
#include <stone/job.h>
#include <stone/log.h>

#include "scheduler.h"

static void sa_sched_exit(int signal);
static short sa_sched_stop_request = 0;


void sa_sched_do_loop() {
	sa_log_write_all(sa_log_level_info, "Scheduler: starting main loop");

	signal(SIGINT, sa_sched_exit);

	static struct sa_database * db;
	db = sa_db_get_default_db();
	if (!db) {
		sa_log_write_all(sa_log_level_error, "Scheduler: there is no default database");
		return;
	}

	static struct sa_database_connection connection;
	if (!db->ops->connect(db, &connection)) {
		sa_log_write_all(sa_log_level_error, "Scheduler: failed to connect to database");
		return;
	}

	/*
	static struct job ** jobs = 0;
	static unsigned int nbJobs = 0;

	static time_t lastUpdate = 0;

	while (!sa_sched_stop_request) {
		static short ok_transaction;
		static time_t update;

		ok_transaction = connection.ops->startTransaction(&connection, 1) >= 0;
		update = time(0);
		if (!ok_transaction)
			sa_log_write_all(sa_log_level_error, "Scheduler: error while starting transaction");

		if (nbJobs > 0) {
			static int nbModifiedJobs;
			nbModifiedJobs = connection.ops->jobModified(&connection, lastUpdate);

			if (nbModifiedJobs > 0) {
				sa_log_write_all(sa_log_level_debug, "Scheduler: There is modified jobs (%d)", nbModifiedJobs);

				static unsigned int i;
				static int j;
				for (i = 0, j = 0; i < nbJobs && j < nbModifiedJobs; i++) {
					static int ok_update;
					ok_update = connection.ops->updateJob(&connection, jobs[i]);
					if (ok_update > 0)
						j++;
					else if (ok_update < 0)
						sa_log_write_all(sa_log_level_error, "Scheduler: error while updating a job (job name=%s)", jobs[i]->name);
				}
			} else if (nbModifiedJobs == 0) {
				sa_log_write_all(sa_log_level_debug, "Scheduler: There is no modified jobs");
			} else {
				sa_log_write_all(sa_log_level_error, "Scheduler: error while fetching modified jobs");
			}
		}

		static int nbNewJobs;
		nbNewJobs = connection.ops->newJobs(&connection, lastUpdate);
		if (nbNewJobs > 0) {
			sa_log_write_all(sa_log_level_debug, "Scheduler: There is new jobs (%d)", nbNewJobs);

			jobs = realloc(jobs, (nbJobs + nbNewJobs) * sizeof(struct job *));

			static int i;
			for (i = 0; i < nbNewJobs; i++)
				jobs[nbJobs + i] = connection.ops->addJob(&connection, 0, i, lastUpdate);
			nbJobs += nbNewJobs;

		} else if (nbNewJobs == 0) {
			sa_log_write_all(sa_log_level_debug, "Scheduler: There is no new jobs");
		} else {
			sa_log_write_all(sa_log_level_error, "Scheduler: error while fetching new jobs");
		}

		ok_transaction = connection.ops->finishTransaction(&connection) >= 0;
		if (!ok_transaction)
			sa_log_write_all(sa_log_level_error, "Scheduler: error while finishing transaction");

		static short i;
		for (i = 0; i < 60 && !sa_sched_stop_request; i++)
			sleep(1);

		// TODO: scheduler stop request
		// if (sched_stopRequest) {}

		lastUpdate = update;
	}*/

	connection.ops->free(&connection);
}

void sa_sched_exit(int signal) {
	if (signal == SIGINT) {
		sa_log_write_all(sa_log_level_warning, "Scheduler: catch SIGINT");
		sa_sched_stop_request = 1;
	}
}

