/*************************************************************************\
*                            __________                                   *
*                           / __/_  __/__  ___  ___                       *
*                          _\ \  / / / _ \/ _ \/ -_)                      *
*                         /___/ /_/  \___/_//_/\__/                       *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  This file is a part of STone                                           *
*                                                                         *
*  STone is free software; you can redistribute it and/or                 *
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
*  Last modified: Sat, 17 Dec 2011 19:08:26 +0100                         *
\*************************************************************************/

#ifndef __STONE_JOB_H__
#define __STONE_JOB_H__

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
	// database
	long id;
	char * name;
	enum job_type type;
	time_t start;
	void * scheduler_private;

	// job
	char * name;
	struct job_ops {
		void (*free)(struct job * j);
		int (*start)(struct job * j);
		int (*stop)(struct job * j);
	} * ops;




	long interval;
	long repetition;

	void * data;
};


int job_initJob(struct job * job, enum job_type type);
const char * job_jobToString(enum job_type type);
enum job_type job_stringToJob(const char * jobname);

#endif

