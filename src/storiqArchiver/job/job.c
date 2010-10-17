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
*  Last modified: Sat, 16 Oct 2010 22:49:26 +0200                       *
\***********************************************************************/

#include <string.h>

#include "common.h"

static const struct job_info {
	enum job_type type;
	char * name;
} job_jobs[] = {
	{ job_type_diffSave,        "differential save" },
	{ job_type_dummy,           "dummy job" },
	{ job_type_incSave,         "incremental save" },
	{ job_type_integrety_check, "integrety check" },
	{ job_type_list,            "list" },
	{ job_type_restore,         "restore" },
	{ job_type_save,            "save" },
	{ job_type_verify,          "verify" },

	{ job_type_dummy, 0 },
};


int job_initJob(struct job * job, enum job_type type) {
	if (!job || type == job_type_dummy)
		return -1;

	switch (type) {
		case job_type_save:
			return job_save_init(job);

		default:
			return -1;
	}
}

const char * job_jobToString(enum job_type type) {
	const struct job_info * ptr;
	for (ptr = job_jobs; ptr->name; ptr++)
		if (ptr->type == type)
			return ptr->name;
	return 0;
}

enum job_type job_stringToJob(const char * jobname) {
	const struct job_info * ptr;
	for (ptr = job_jobs; ptr->name; ptr++)
		if (!strcasecmp(jobname, ptr->name))
			return ptr->type;
	return job_type_dummy;
}

