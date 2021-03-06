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
*  Copyright (C) 2013-2021, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#define _GNU_SOURCE
// dgettext, gettext
#include <libintl.h>
// free
#include <stdlib.h>

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
	const char * translation;
	const enum so_job_status status;
} so_job_status[] = {
	[so_job_status_disable]                = { 0, 0, gettext_noop("disabled"),               NULL, so_job_status_disable },
	[so_job_status_error]                  = { 0, 0, gettext_noop("error"),                  NULL, so_job_status_error },
	[so_job_status_finished]               = { 0, 0, gettext_noop("finished"),               NULL, so_job_status_finished },
	[so_job_status_finished_with_warnings] = { 0, 0, gettext_noop("finished with warnings"), NULL, so_job_status_finished },
	[so_job_status_pause]                  = { 0, 0, gettext_noop("paused"),                 NULL, so_job_status_pause },
	[so_job_status_running]                = { 0, 0, gettext_noop("running"),                NULL, so_job_status_running },
	[so_job_status_scheduled]              = { 0, 0, gettext_noop("scheduled"),              NULL, so_job_status_scheduled },
	[so_job_status_stopped]                = { 0, 0, gettext_noop("stopped"),                NULL, so_job_status_stopped },
	[so_job_status_waiting]                = { 0, 0, gettext_noop("waiting"),                NULL, so_job_status_waiting },

	[so_job_status_unknown] = { 0, 0, gettext_noop("unknown status"), NULL, so_job_status_unknown },
};
static const unsigned int so_job_nb_status = sizeof(so_job_status) / sizeof(*so_job_status);

static struct so_job_record_notif2 {
	unsigned long long hash;
	unsigned long long hash_translated;

	const char * name;
	const char * translation;
	const enum so_job_record_notif notif;
} so_job_record_notifs[] = {
	[so_job_record_notif_normal]    = { 0, 0, gettext_noop("normal"),    NULL, so_job_record_notif_normal },
	[so_job_record_notif_important] = { 0, 0, gettext_noop("important"), NULL, so_job_record_notif_important },
	[so_job_record_notif_read]      = { 0, 0, gettext_noop("read"),      NULL, so_job_record_notif_read },

	[so_job_record_notif_unknown] = { 0, 0, gettext_noop("unknown notification"), NULL, so_job_record_notif_unknown },
};
static const unsigned int so_job_record_notif2 = sizeof(so_job_record_notifs) / sizeof(*so_job_record_notifs);

static struct so_job_run_step2 {
	unsigned long long hash;
	unsigned long long hash_translated;

	const char * name;
	const char * translation;
	const enum so_job_run_step step;
} so_job_run_steps[] = {
	[so_job_run_step_job]      = { 0, 0, gettext_noop("job"), NULL, so_job_run_step_job },
	[so_job_run_step_on_error] = { 0, 0, gettext_noop("on error"), NULL, so_job_run_step_on_error },
	[so_job_run_step_post_job] = { 0, 0, gettext_noop("post job"), NULL, so_job_run_step_post_job },
	[so_job_run_step_pre_job]  = { 0, 0, gettext_noop("pre job"),  NULL, so_job_run_step_pre_job },
	[so_job_run_step_warm_up]  = { 0, 0, gettext_noop("warm up"),  NULL, so_job_run_step_warm_up },

	[so_job_run_step_unknown]  = { 0, 0, gettext_noop("unknown"),  NULL, so_job_run_step_unknown },
};
static const unsigned int so_job_run_step2 = sizeof(so_job_run_steps) / sizeof(*so_job_run_steps);

static void so_job_init(void) __attribute__((constructor));


struct so_value * so_job_convert(struct so_job * job) {
	return so_value_pack("{sssssssssssIsIsIsIsssfsssisbsOsO}",
		"id", job->id,
		"name", job->name,
		"type", job->type,
		"user", job->user,
		"email", job->email,

		"next start", (long) job->next_start,
		"interval", job->interval,
		"repetition", job->repetition,

		"num runs", job->num_runs,
		"step", so_job_run_step_to_string(job->step, false),
		"done", job->done,
		"status", so_job_status_to_string(job->status, false),

		"exit code", job->exit_code,
		"stopped by user", job->stopped_by_user,

		"metadatas", job->meta,
		"options", job->option
	);
}

void so_job_free(struct so_job * job) {
	if (job == NULL)
		return;

	free(job->id);
	free(job->name);
	free(job->type);
	free(job->user);
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
		so_job_status[i].translation = dgettext("libstoriqone", so_job_status[i].name);
		so_job_status[i].hash_translated = so_string_compute_hash2(dgettext("libstoriqone", so_job_status[i].name));
	}

	for (i = 0; i < so_job_record_notif2; i++) {
		so_job_record_notifs[i].hash = so_string_compute_hash2(so_job_record_notifs[i].name);
		so_job_record_notifs[i].translation = dgettext("libstoriqone", so_job_record_notifs[i].name);
		so_job_record_notifs[i].hash_translated = so_string_compute_hash2(dgettext("libstoriqone", so_job_record_notifs[i].name));
	}

	for (i = 0; i < so_job_run_step2; i++) {
		so_job_run_steps[i].hash = so_string_compute_hash2(so_job_run_steps[i].name);
		so_job_run_steps[i].translation = dgettext("libstoriqone", so_job_run_steps[i].name);
		so_job_run_steps[i].hash_translated = so_string_compute_hash2(dgettext("libstoriqone", so_job_run_steps[i].name));
	}
}

const char * so_job_report_notif_to_string(enum so_job_record_notif notif, bool translate) {
	if (translate)
		return so_job_record_notifs[notif].translation;
	else
		return so_job_record_notifs[notif].name;
}

const char * so_job_run_step_to_string(enum so_job_run_step step, bool translate) {
	if (translate)
		return so_job_run_steps[step].translation;
	else
		return so_job_run_steps[step].name;
}

enum so_job_run_step so_job_run_string_to_step(const char * step, bool translate) {
	if (step == NULL)
		return so_job_run_step_unknown;

	unsigned int i;
	const unsigned long long hash = so_string_compute_hash2(step);

	if (translate) {
		for (i = 0; i < so_job_run_step2; i++)
			if (so_job_run_steps[i].hash_translated == hash)
				return so_job_run_steps[i].step;
	} else {
		for (i = 0; i < so_job_run_step2; i++)
			if (so_job_run_steps[i].hash == hash)
				return so_job_run_steps[i].step;
	}

	return so_job_run_step_unknown;
}

const char * so_job_status_to_string(enum so_job_status status, bool translate) {
	if (translate)
		return so_job_status[status].translation;
	else
		return so_job_status[status].name;
}

enum so_job_record_notif so_job_string_to_record_notif(const char * notif, bool translate) {
	if (notif == NULL)
		return so_job_record_notif_unknown;

	unsigned int i;
	const unsigned long long hash = so_string_compute_hash2(notif);

	if (translate) {
		for (i = 0; i < so_job_record_notif2; i++)
			if (so_job_record_notifs[i].hash_translated == hash)
				return so_job_record_notifs[i].notif;
	} else {
		for (i = 0; i < so_job_record_notif2; i++)
			if (so_job_record_notifs[i].hash == hash)
				return so_job_record_notifs[i].notif;
	}

	return so_job_record_notif_unknown;
}

enum so_job_status so_job_string_to_status(const char * status, bool translate) {
	if (status == NULL)
		return so_job_status_unknown;

	unsigned int i;
	const unsigned long long hash = so_string_compute_hash2(status);

	if (translate) {
		for (i = 0; i < so_job_nb_status; i++)
			if (so_job_status[i].hash_translated == hash)
				return so_job_status[i].status;
	} else {
		for (i = 0; i < so_job_nb_status; i++)
			if (so_job_status[i].hash == hash)
				return so_job_status[i].status;
	}

	return so_job_status_unknown;
}

void so_job_sync(struct so_job * job, struct so_value * new_job) {
	free(job->id);
	free(job->name);
	free(job->type);
	free(job->user);
	so_value_free(job->meta);
	so_value_free(job->option);
	job->id = job->name = job->type = NULL;
	job->meta = job->option = NULL;

	const char * status = NULL;
	double done = 0;
	const char * step = NULL;

	so_value_unpack(new_job, "{sssssssssssisIsIsIsSsfsSsisbsOsO}",
		"id", &job->id,
		"name", &job->name,
		"type", &job->type,
		"user", &job->user,
		"email", &job->email,

		"next start", &job->next_start,
		"interval", &job->interval,
		"repetition", &job->repetition,

		"num runs", &job->num_runs,
		"step", &step,
		"done", &done,
		"status", &status,

		"exit code", &job->exit_code,
		"stopped by user", &job->stopped_by_user,

		"metadatas", &job->meta,
		"options", &job->option
	);

	job->done = done;
	job->status = so_job_string_to_status(status, false);
	job->step = so_job_run_string_to_step(step, false);
}
