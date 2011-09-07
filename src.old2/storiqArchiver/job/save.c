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
*  Last modified: Sat, 16 Oct 2010 23:13:35 +0200                       *
\***********************************************************************/

#include "common.h"

static int job_save_doJob(struct job * job);
static void job_save_free(struct job * job);

static struct job_ops job_save_ops = {
	.doJob = job_save_doJob,
	.free = job_save_free,
};


int job_save_doJob(struct job * job) {
	if (!job)
		return -1;

	return 0;
}

void job_save_free(struct job * job) {
	if (!job)
		return;

	job->type = job_type_dummy;
	job->data = 0;
	job->ops = 0;
}

int job_save_init(struct job * job) {
	if (!job)
		return -1;

	job->type = job_type_save;
	job->ops = &job_save_ops;
	job->data = 0;

	return 0;
}

