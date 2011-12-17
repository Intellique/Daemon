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
*  Last modified: Fri, 09 Dec 2011 10:21:38 +0100                         *
\*************************************************************************/

#ifndef __STORIQARCHIVER_JOB_H__
#define __STORIQARCHIVER_JOB_H__

#include <sys/time.h>

struct database_connection;
struct job;

enum job_type {
	job_type_diffSave,
	job_type_dummy,
	job_type_incSave,
	job_type_integrity_check,
	job_type_list,
	job_type_restore,
	job_type_save,
	job_type_verify,
};

struct job_ops {
	int (*doJob)(struct job * j);
	void (*free)(struct job * j);
};

struct job {
	long id;
	char * name;
	enum job_type type;
	time_t start;
	unsigned long interval;
	unsigned long repetition;
	time_t modified;

	struct job_ops * ops;
	void * data;

	struct database_connection * connection;
};


int job_initJob(struct job * job, enum job_type type);
const char * job_jobToString(enum job_type type);
enum job_type job_stringToJob(const char * jobname);

#endif

