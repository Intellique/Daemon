/****************************************************************************\
*                             __________                                     *
*                            / __/_  __/__  ___  ___                         *
*                           _\ \  / / / _ \/ _ \/ -_)                        *
*                          /___/ /_/  \___/_//_/\__/                         *
*                                                                            *
*  ------------------------------------------------------------------------  *
*  This file is a part of STone                                              *
*                                                                            *
*  STone is free software; you can redistribute it and/or modify             *
*  it under the terms of the GNU Affero General Public License               *
*  as published by the Free Software Foundation; either version 3            *
*  of the License, or (at your option) any later version.                    *
*                                                                            *
*  This program is distributed in the hope that it will be useful,           *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of            *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
*  GNU Affero General Public License for more details.                       *
*                                                                            *
*  You should have received a copy of the GNU Affero General Public License  *
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.     *
*                                                                            *
*  ------------------------------------------------------------------------  *
*  Copyright (C) 2014, Clercin guillaume <gclercin@intellique.com>           *
\****************************************************************************/

// free
#include <stdlib.h>

#include "job.h"
#include "string.h"
#include "value.h"

static struct st_job_status2 {
	const char * name;
	const enum st_job_status status;
	unsigned long long hash;
} st_job_status[] = {
	{ "disable",   st_job_status_disable,   0 },
	{ "error",     st_job_status_error,     0 },
	{ "finished",  st_job_status_finished,  0 },
	{ "pause",     st_job_status_pause,     0 },
	{ "running",   st_job_status_running,   0 },
	{ "scheduled", st_job_status_scheduled, 0 },
	{ "stopped",   st_job_status_stopped,   0 },
	{ "waiting",   st_job_status_waiting,   0 },

	{ "unknown", st_job_status_unknown, 0 },
};

static struct st_job_record_notif2 {
	const char * name;
	const enum st_job_record_notif notif;
	unsigned long long hash;
} st_job_record_notifs[] = {
	{ "normal",    st_job_record_notif_normal,    0 },
	{ "important", st_job_record_notif_important, 0 },
	{ "read",      st_job_record_notif_read,      0 },

	{ "unknown", st_job_record_notif_unknown, 0 },
};

static void st_job_init(void) __attribute__((constructor));


__asm__(".symver st_job_convert_v1, st_job_convert@@LIBSTONE_1.2");
struct st_value * st_job_convert_v1(struct st_job * job) {
	return st_value_pack("{sssssssisisisisfsssisb}",
		"id", job->key,
		"name", job->name,
		"type", job->type,

		"next start", job->next_start,
		"interval", job->interval,
		"repetition", job->repetition,

		"num runs", job->num_runs,
		"done", job->done,
		"status", st_job_status_to_string_v1(job->status),

		"exit code", (long int) job->exit_code,
		"stopped by user", job->stopped_by_user
	);
}

__asm__(".symver st_job_free_v1, st_job_free@@LIBSTONE_1.2");
void st_job_free_v1(struct st_job * job) {
	if (job == NULL)
		return;

	free(job->key);
	free(job->name);
	free(job->type);
	st_value_free(job->meta);
	st_value_free(job->option);
	st_value_free(job->db_data);

	free(job);
}

__asm__(".symver st_job_free2_v1, st_job_free2@@LIBSTONE_1.2");
void st_job_free2_v1(void * job) {
	st_job_free_v1(job);
}

static void st_job_init() {
	int i;
	for (i = 0; st_job_status[i].status != st_job_status_unknown; i++)
		st_job_status[i].hash = st_string_compute_hash2(st_job_status[i].name);
	st_job_status[i].hash = st_string_compute_hash2(st_job_status[i].name);

	for (i = 0; st_job_record_notifs[i].notif != st_job_record_notif_unknown; i++)
		st_job_record_notifs[i].hash = st_string_compute_hash2(st_job_record_notifs[i].name);
	st_job_record_notifs[i].hash = st_string_compute_hash2(st_job_record_notifs[i].name);
}

__asm__(".symver st_job_report_notif_to_string_v1, st_job_report_notif_to_string@@LIBSTONE_1.2");
const char * st_job_report_notif_to_string_v1(enum st_job_record_notif notif) {
	unsigned int i;
	for (i = 0; st_job_record_notifs[i].notif != st_job_record_notif_unknown; i++)
		if (st_job_record_notifs[i].notif == notif)
			return st_job_record_notifs[i].name;

	return st_job_record_notifs[i].name;
}

__asm__(".symver st_job_status_to_string_v1, st_job_status_to_string@@LIBSTONE_1.2");
const char * st_job_status_to_string_v1(enum st_job_status status) {
	unsigned int i;
	for (i = 0; st_job_status[i].status != st_job_status_unknown; i++)
		if (st_job_status[i].status == status)
			return st_job_status[i].name;

	return st_job_status[i].name;
}

__asm__(".symver st_job_string_to_record_notif_v1, st_job_string_to_record_notif@@LIBSTONE_1.2");
enum st_job_record_notif st_job_string_to_record_notif_v1(const char * notif) {
	if (notif == NULL)
		return st_job_record_notif_unknown;

	unsigned int i;
	const unsigned long long hash = st_string_compute_hash2(notif);
	for (i = 0; st_job_record_notifs[i].notif != st_job_record_notif_unknown; i++)
		if (st_job_record_notifs[i].hash == hash)
			return st_job_record_notifs[i].notif;

	return st_job_record_notifs[i].notif;
}

__asm__(".symver st_job_string_to_status_v1, st_job_string_to_status@@LIBSTONE_1.2");
enum st_job_status st_job_string_to_status_v1(const char * status) {
	if (status == NULL)
		return st_job_status_unknown;

	unsigned int i;
	const unsigned long long hash = st_string_compute_hash2(status);
	for (i = 0; st_job_status[i].status != st_job_status_unknown; i++)
		if (st_job_status[i].hash == hash)
			return st_job_status[i].status;

	return st_job_status[i].status;
}

__asm__(".symver st_job_sync_v1, st_job_sync@@LIBSTONE_1.2");
void st_job_sync_v1(struct st_job * job, struct st_value * new_job) {
	free(job->key);
	free(job->name);
	free(job->type);
	job->key = job->name = job->type = NULL;

	char * status = NULL;
	double done = 0;
	long int exit_code = 0;

	st_value_unpack(new_job, "{sssssssisisisisfsssisb}",
		"id", &job->key,
		"name", &job->name,
		"type", &job->type,

		"next start", &job->next_start,
		"interval", &job->interval,
		"repetition", &job->repetition,

		"num runs", &job->num_runs,
		"done", &done,
		"status", status,

		"exit code", &exit_code,
		"stopped by user", &job->stopped_by_user
	);

	job->done = done;
	job->status = st_job_string_to_status_v1(status);
	free(status);
	job->exit_code = exit_code;
}

