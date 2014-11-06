/****************************************************************************\
*                    ______           _      ____                            *
*                   / __/ /____  ____(_)__ _/ __ \___  ___                   *
*                  _\ \/ __/ _ \/ __/ / _ `/ /_/ / _ \/ -_)                  *
*                 /___/\__/\___/_/ /_/\_, /\____/_//_/\__/                   *
*                                      /_/                                   *
*  ------------------------------------------------------------------------  *
*  This file is a part of Storiq One                                         *
*                                                                            *
*  Storiq One is free software; you can redistribute it and/or modify        *
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

#define _GNU_SOURCE
// gettext
#include <libintl.h>
// free
#include <stdlib.h>
// vasprintf
#include <stdio.h>

#define gettext_noop(String) String

#include <libstoriqone/database.h>
#include <libstoriqone/job.h>
#include <libstoriqone/log.h>
#include <libstoriqone/string.h>
#include <libstoriqone/value.h>

static struct so_job_status2 {
	unsigned long long hash;
	unsigned long long hash_translated;

	const char * name;
	const enum so_job_status status;
} so_job_status[] = {
	[so_job_status_disable]   = { 0, 0, gettext_noop("disable"),   so_job_status_disable },
	[so_job_status_error]     = { 0, 0, gettext_noop("error"),     so_job_status_error },
	[so_job_status_finished]  = { 0, 0, gettext_noop("finished"),  so_job_status_finished },
	[so_job_status_pause]     = { 0, 0, gettext_noop("pause"),     so_job_status_pause },
	[so_job_status_running]   = { 0, 0, gettext_noop("running"),   so_job_status_running },
	[so_job_status_scheduled] = { 0, 0, gettext_noop("scheduled"), so_job_status_scheduled },
	[so_job_status_stopped]   = { 0, 0, gettext_noop("stopped"),   so_job_status_stopped },
	[so_job_status_waiting]   = { 0, 0, gettext_noop("waiting"),   so_job_status_waiting },

	[so_job_status_unknown] = { 0, 0, gettext_noop("unknown"), so_job_status_unknown },
};
static const unsigned int so_job_nb_status = sizeof(so_job_status) / sizeof(*so_job_status);

static struct so_job_record_notif2 {
	unsigned long long hash;
	unsigned long long hash_translated;

	const char * name;
	const enum so_job_record_notif notif;
} so_job_record_notifs[] = {
	[so_job_record_notif_normal]    = { 0, 0, gettext_noop("normal"),    so_job_record_notif_normal },
	[so_job_record_notif_important] = { 0, 0, gettext_noop("important"), so_job_record_notif_important },
	[so_job_record_notif_read]      = { 0, 0, gettext_noop("read"),      so_job_record_notif_read },

	[so_job_record_notif_unknown] = { 0, 0, gettext_noop("unknown"), so_job_record_notif_unknown },
};
static const unsigned int so_job_record_notif2 = sizeof(so_job_record_notifs) / sizeof(*so_job_record_notifs);

static void so_job_init(void) __attribute__((constructor));


int so_job_add_record(struct so_job * job, struct so_database_connection * db_connect, enum so_log_level level, enum so_job_record_notif notif, const char * format, ...) {
	char * message = NULL;

	va_list va;
	va_start(va, format);
	vasprintf(&message, format, va);
	va_end(va);

	so_log_write(level, message);
	int failed = db_connect->ops->add_job_record(db_connect, job, level, notif, message);

	free(message);

	return failed;
}

struct so_value * so_job_convert(struct so_job * job) {
	return so_value_pack("{sssssssisisisisfsssisb}",
		"id", job->key,
		"name", job->name,
		"type", job->type,

		"next start", job->next_start,
		"interval", job->interval,
		"repetition", job->repetition,

		"num runs", job->num_runs,
		"done", job->done,
		"status", so_job_status_to_string(job->status, false),

		"exit code", (long int) job->exit_code,
		"stopped by user", job->stopped_by_user
	);
}

void so_job_free(struct so_job * job) {
	if (job == NULL)
		return;

	free(job->key);
	free(job->name);
	free(job->type);
	so_value_free(job->meta);
	so_value_free(job->option);
	so_value_free(job->db_data);

	free(job);
}

void so_job_free2(void * job) {
	so_job_free(job);
}

static void so_job_init() {
	unsigned int i;
	for (i = 0; i < so_job_nb_status; i++) {
		so_job_status[i].hash = so_string_compute_hash2(so_job_status[i].name);
		so_job_status[i].hash_translated = so_string_compute_hash2(gettext(so_job_status[i].name));
	}

	for (i = 0; i < so_job_record_notif2; i++) {
		so_job_record_notifs[i].hash = so_string_compute_hash2(so_job_record_notifs[i].name);
		so_job_record_notifs[i].hash_translated = so_string_compute_hash2(gettext(so_job_record_notifs[i].name));
	}
}

const char * so_job_report_notif_to_string(enum so_job_record_notif notif, bool translate) {
	const char * value = so_job_record_notifs[notif].name;
	if (translate)
		value = gettext(value);
	return value;
}

const char * so_job_status_to_string(enum so_job_status status, bool translate) {
	const char * value = so_job_status[status].name;
	if (translate)
		value = gettext(value);
	return value;
}

enum so_job_record_notif so_job_string_to_record_notif(const char * notif, bool translate) {
	if (notif == NULL)
		return so_job_record_notif_unknown;

	unsigned int i;
	const unsigned long long hash = so_string_compute_hash2(notif);

	if (translate) {
		for (i = 0; so_job_record_notifs[i].notif != so_job_record_notif_unknown; i++)
			if (so_job_record_notifs[i].hash_translated == hash)
				return so_job_record_notifs[i].notif;
	} else {
		for (i = 0; so_job_record_notifs[i].notif != so_job_record_notif_unknown; i++)
			if (so_job_record_notifs[i].hash == hash)
				return so_job_record_notifs[i].notif;
	}

	return so_job_record_notifs[i].notif;
}

enum so_job_status so_job_string_to_status(const char * status, bool translate) {
	if (status == NULL)
		return so_job_status_unknown;

	unsigned int i;
	const unsigned long long hash = so_string_compute_hash2(status);

	if (translate) {
		for (i = 0; so_job_status[i].status != so_job_status_unknown; i++)
			if (so_job_status[i].hash_translated == hash)
				return so_job_status[i].status;
	} else {
		for (i = 0; so_job_status[i].status != so_job_status_unknown; i++)
			if (so_job_status[i].hash == hash)
				return so_job_status[i].status;
	}

	return so_job_status[i].status;
}

void so_job_sync(struct so_job * job, struct so_value * new_job) {
	free(job->key);
	free(job->name);
	free(job->type);
	job->key = job->name = job->type = NULL;

	char * status = NULL;
	double done = 0;
	long int exit_code = 0;

	so_value_unpack(new_job, "{sssssssisisisisfsssisb}",
		"id", &job->key,
		"name", &job->name,
		"type", &job->type,

		"next start", &job->next_start,
		"interval", &job->interval,
		"repetition", &job->repetition,

		"num runs", &job->num_runs,
		"done", &done,
		"status", &status,

		"exit code", &exit_code,
		"stopped by user", &job->stopped_by_user
	);

	job->done = done;
	job->status = so_job_string_to_status(status, false);
	free(status);
	job->exit_code = exit_code;
}

