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
*  Copyright (C) 2013-2015, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Thu, 19 Feb 2015 11:56:45 +0100                            *
\****************************************************************************/

// json_*
#include <jansson.h>
// free, malloc
#include <stdlib.h>
// sleep
#include <unistd.h>

#include <libstone/database.h>
#include <libstone/host.h>
#include <libstone/job.h>
#include <libstone/io.h>
#include <libstone/library/archive.h>
#include <libstone/library/drive.h>
#include <libstone/library/media.h>
#include <libstone/library/ressource.h>
#include <libstone/library/slot.h>
#include <libstone/log.h>
#include <libstone/script.h>
#include <libstone/util/hashtable.h>
#include <stoned/library/changer.h>

#include <libjob-erase-media.chcksum>

struct st_job_erase_media_private {
	struct st_media * media;

	struct st_archive ** archives;
	unsigned int nb_archives;
};

static bool st_job_erase_media_check(struct st_job * job);
static void st_job_erase_media_free(struct st_job * job);
static void st_job_erase_media_init(void) __attribute__((constructor));
static char * st_job_erase_media_make_report(struct st_job * job);
static void st_job_erase_media_new_job(struct st_job * job);
static void st_job_erase_media_on_error(struct st_job * job);
static void st_job_erase_media_post_run(struct st_job * job);
static bool st_job_erase_media_pre_run(struct st_job * job);
static int st_job_erase_media_run(struct st_job * job);

static struct st_job_ops st_job_erase_media_ops = {
	.check    = st_job_erase_media_check,
	.free     = st_job_erase_media_free,
	.on_error = st_job_erase_media_on_error,
	.post_run = st_job_erase_media_post_run,
	.pre_run  = st_job_erase_media_pre_run,
	.run      = st_job_erase_media_run,
};

static struct st_job_driver st_job_erase_media_driver = {
	.name        = "erase-media",
	.new_job     = st_job_erase_media_new_job,
	.cookie      = NULL,
	.api_level   = {
		.database = STONE_DATABASE_API_LEVEL,
		.job      = STONE_JOB_API_LEVEL,
	},
	.src_checksum   = STONE_JOB_ERASEMEDIA_SRCSUM,
};


static bool st_job_erase_media_check(struct st_job * job) {
	job->sched_status = st_job_status_error;
	job->repetition = 0;
	return false;
}

static void st_job_erase_media_free(struct st_job * job) {
	struct st_job_erase_media_private * self = job->data;
	unsigned int i = 0;
	for (i = 0; i < self->nb_archives; i++)
		st_archive_free(self->archives[i]);
	free(self->archives);
	free(self);
	job->data = NULL;
}

static void st_job_erase_media_init() {
	st_job_register_driver(&st_job_erase_media_driver);
}

static char * st_job_erase_media_make_report(struct st_job * job) {
	struct st_job_erase_media_private * self = job->data;

	json_t * archives = json_array();
	unsigned int i;
	for (i = 0; i < self->nb_archives; i++) {
		struct st_archive * archive = self->archives[i];
		json_t * jarchive = json_pack("{sssssb}",
			"name", archive->name,
			"uuid", archive->uuid,
			"deleted", archive->deleted
		);

		json_array_append_new(archives, jarchive);
	}

	json_t * jjob = json_object();

	json_t * media = json_object();
	if (self->media->uuid[0] != '\0')
		json_object_set_new(media, "uuid", json_string(self->media->uuid));
	else
		json_object_set_new(media, "uuid", json_null());
	if (self->media->label != NULL)
		json_object_set_new(media, "label", json_string(self->media->label));
	else
		json_object_set_new(media, "label", json_null());
	json_object_set_new(media, "name", json_string(self->media->name));

	json_t * sdata = json_object();
	json_object_set_new(sdata, "archives", archives);
	json_object_set_new(sdata, "job", jjob);
	json_object_set_new(sdata, "host", st_host_get_info());
	json_object_set_new(sdata, "media", media);

	char * report = json_dumps(sdata, JSON_COMPACT);
	json_decref(sdata);
	return report;
}

static void st_job_erase_media_new_job(struct st_job * job) {
	struct st_job_erase_media_private * self = malloc(sizeof(struct st_job_erase_media_private));
	job->db_connect = job->db_config->ops->connect(job->db_config);
	self->media = st_media_get_by_job(job, job->db_connect);

	if (self->media != NULL)
		self->archives = job->db_connect->ops->get_archive_by_media(job->db_connect, self->media, &self->nb_archives);

	job->data = self;
	job->ops = &st_job_erase_media_ops;
}

static void st_job_erase_media_on_error(struct st_job * job) {
	struct st_job_erase_media_private * self = job->data;

	if (job->db_connect->ops->get_nb_scripts(job->db_connect, job->driver->name, st_script_type_on_error, self->media->pool) == 0)
		return;

	json_t * archives = json_array();
	unsigned int i;
	for (i = 0; i < self->nb_archives; i++) {
		struct st_archive * archive = self->archives[i];
		json_t * jarchive = json_pack("{sssssb}",
			"name", archive->name,
			"uuid", archive->uuid,
			"deleted", archive->deleted
		);

		json_array_append_new(archives, jarchive);
	}

	json_t * jjob = json_object();

	json_t * media = json_object();
	if (self->media->uuid[0] != '\0')
		json_object_set_new(media, "uuid", json_string(self->media->uuid));
	else
		json_object_set_new(media, "uuid", json_null());
	if (self->media->label != NULL)
		json_object_set_new(media, "label", json_string(self->media->label));
	else
		json_object_set_new(media, "label", json_null());
	json_object_set_new(media, "name", json_string(self->media->name));

	json_t * sdata = json_object();
	json_object_set_new(sdata, "archives", archives);
	json_object_set_new(sdata, "job", jjob);
	json_object_set_new(sdata, "host", st_host_get_info());
	json_object_set_new(sdata, "media", media);

	json_t * returned_data = st_script_run(job->db_connect, job, job->driver->name, st_script_type_pre, self->media->pool, sdata);
	json_decref(returned_data);
	json_decref(sdata);
}

static void st_job_erase_media_post_run(struct st_job * job) {
	struct st_job_erase_media_private * self = job->data;

	if (job->db_connect->ops->get_nb_scripts(job->db_connect, job->driver->name, st_script_type_post, self->media->pool) == 0)
		return;

	json_t * archives = json_array();
	unsigned int i;
	for (i = 0; i < self->nb_archives; i++) {
		struct st_archive * archive = self->archives[i];
		json_t * jarchive = json_pack("{sssssb}",
			"name", archive->name,
			"uuid", archive->uuid,
			"deleted", archive->deleted
		);

		json_array_append_new(archives, jarchive);
	}

	json_t * jjob = json_object();

	json_t * media = json_object();
	if (self->media->uuid[0] != '\0')
		json_object_set_new(media, "uuid", json_string(self->media->uuid));
	else
		json_object_set_new(media, "uuid", json_null());
	if (self->media->label != NULL)
		json_object_set_new(media, "label", json_string(self->media->label));
	else
		json_object_set_new(media, "label", json_null());
	json_object_set_new(media, "name", json_string(self->media->name));

	json_t * sdata = json_object();
	json_object_set_new(sdata, "archives", archives);
	json_object_set_new(sdata, "job", jjob);
	json_object_set_new(sdata, "host", st_host_get_info());
	json_object_set_new(sdata, "media", media);

	json_t * returned_data = st_script_run(job->db_connect, job, job->driver->name, st_script_type_pre, self->media->pool, sdata);
	json_decref(returned_data);
	json_decref(sdata);
}

static bool st_job_erase_media_pre_run(struct st_job * job) {
	struct st_job_erase_media_private * self = job->data;

	if (self->media == NULL) {
		st_job_add_record(job->db_connect, st_log_level_error, job, st_job_record_notif_important, "Error: no media associated with this job");
		return false;
	}

	if (job->db_connect->ops->get_nb_scripts(job->db_connect, job->driver->name, st_script_type_pre, self->media->pool) == 0)
		return true;

	json_t * archives = json_array();
	unsigned int i;
	for (i = 0; i < self->nb_archives; i++) {
		struct st_archive * archive = self->archives[i];
		json_t * jarchive = json_pack("{sssssb}",
			"name", archive->name,
			"uuid", archive->uuid,
			"deleted", archive->deleted
		);

		json_array_append_new(archives, jarchive);
	}

	json_t * jjob = json_object();

	json_t * media = json_object();
	if (self->media->uuid[0] != '\0')
		json_object_set_new(media, "uuid", json_string(self->media->uuid));
	else
		json_object_set_new(media, "uuid", json_null());
	if (self->media->label != NULL)
		json_object_set_new(media, "label", json_string(self->media->label));
	else
		json_object_set_new(media, "label", json_null());
	json_object_set_new(media, "name", json_string(self->media->name));

	json_t * sdata = json_object();
	json_object_set_new(sdata, "archives", archives);
	json_object_set_new(sdata, "job", jjob);
	json_object_set_new(sdata, "host", st_host_get_info());
	json_object_set_new(sdata, "media", media);

	json_t * returned_data = st_script_run(job->db_connect, job, job->driver->name, st_script_type_pre, self->media->pool, sdata);
	bool sr = st_io_json_should_run(returned_data);
	json_decref(returned_data);
	json_decref(sdata);

	return sr;
}

static int st_job_erase_media_run(struct st_job * job) {
	struct st_job_erase_media_private * self = job->data;

	st_job_add_record(job->db_connect, st_log_level_info, job, st_job_record_notif_important, "Start erase media job (job name: %s), num runs %ld", job->name, job->num_runs);

	if (self->media == NULL) {
		st_job_add_record(job->db_connect, st_log_level_error, job, st_job_record_notif_important, "Media not found");
		job->sched_status = st_job_status_error;
		return 1;
	}
	if (self->media->type == st_media_type_cleaning) {
		st_job_add_record(job->db_connect, st_log_level_error, job, st_job_record_notif_important, "Try to erase a cleaning media");
		job->sched_status = st_job_status_error;
		return 1;
	}
	if (self->media->type == st_media_type_worm) {
		st_job_add_record(job->db_connect, st_log_level_error, job, st_job_record_notif_important, "Try to erase a worm media with data");
		job->sched_status = st_job_status_error;
		return 1;
	}
	if (self->media->status == st_media_status_error)
		st_job_add_record(job->db_connect, st_log_level_warning, job, st_job_record_notif_important, "Try to erase a media with error status");

	unsigned int i;
	bool deleted = true;
	for (i = 0; i < self->nb_archives && deleted; i++)
		deleted = self->archives[i]->deleted;

	if (!deleted) {
		st_job_add_record(job->db_connect, st_log_level_warning, job, st_job_record_notif_important, "Try to erase a media witch contains archives not deleted");
		return 1;
	}

	enum {
		alert_user,
		changer_has_free_drive,
		drive_is_free,
		look_for_media,
		media_in_drive,
		slot_is_free,
	} state = look_for_media;
	bool stop = false, has_alert_user = false, has_lock_on_drive = false, has_lock_on_slot = false;
	struct st_drive * drive = NULL;
	struct st_slot * slot = NULL;

	while (stop == false && job->db_status != st_job_status_stopped) {
		switch (state) {
			case alert_user:
				job->sched_status = st_job_status_waiting;
				if (!has_alert_user)
					st_job_add_record(job->db_connect, st_log_level_warning, job, st_job_record_notif_important, "Media not found (named: %s)", self->media->name);
				has_alert_user = true;
				sleep(15);
				state = look_for_media;
				break;

			case changer_has_free_drive:
				// if drive is not NULL, we own also a lock on it
				drive = slot->changer->ops->find_free_drive(slot->changer, self->media->format, true, true);
				if (drive != NULL) {
					has_lock_on_drive = true;
					stop = true;
				} else {
					job->sched_status = st_job_status_waiting;
					sleep(5);
					state = media_in_drive;
				}
				break;

			case drive_is_free:
				if (drive->lock->ops->timed_lock(drive->lock, 2000)) {
					job->sched_status = st_job_status_waiting;
					sleep(5);
					state = media_in_drive;
				} else if (slot->media == self->media) {
					has_lock_on_drive = true;
					stop = true;
				} else {
					// media has been vannished
					drive->lock->ops->unlock(drive->lock);
					drive = NULL;
					slot = NULL;
					state = look_for_media;
				}
				break;

			case look_for_media:
				// lock of this slot can be owned by another job
				slot = st_changer_find_slot_by_media(self->media);
				state = slot != NULL ? media_in_drive : alert_user;
				break;

			case media_in_drive:
				drive = slot->drive;
				state = drive != NULL ? drive_is_free : slot_is_free;
				break;

			case slot_is_free:
				if (slot->lock->ops->timed_lock(slot->lock, 2000)) {
					job->sched_status = st_job_status_waiting;
					sleep(8);
					state = look_for_media;
				} else {
					has_lock_on_slot = true;
					state = changer_has_free_drive;
				}
				break;
		}
	}

	job->sched_status = st_job_status_running;

	if (job->db_status != st_job_status_stopped)
		job->done = 0.2;

	if (job->db_status != st_job_status_stopped && drive->slot->media != NULL && drive->slot->media != self->media) {
		st_job_add_record(job->db_connect, st_log_level_info, job, st_job_record_notif_normal, "unloading media: %s from drive: { %s, %s, #%td }", drive->slot->media->name, drive->vendor, drive->model, drive - drive->changer->drives);
		int failed = slot->changer->ops->unload(slot->changer, drive);
		if (failed) {
			st_job_add_record(job->db_connect, st_log_level_error, job, st_job_record_notif_important, "failed to unload media: %s from drive: { %s, %s, #%td }", drive->slot->media->name, drive->vendor, drive->model, drive - drive->changer->drives);

			if (has_lock_on_slot)
				slot->lock->ops->unlock(slot->lock);
			if (has_lock_on_drive)
				drive->lock->ops->unlock(drive->lock);

			self->media->locked = false;
			self->media->lock->ops->unlock(self->media->lock);
			job->db_connect->ops->sync_media(job->db_connect, self->media);

			job->sched_status = st_job_status_error;

			return 2;
		}
	}

	if (job->db_status != st_job_status_stopped)
		job->done = 0.4;

	if (job->db_status != st_job_status_stopped && drive->slot->media == NULL) {
		st_job_add_record(job->db_connect, st_log_level_info, job, st_job_record_notif_normal, "loading media: %s to drive: { %s, %s, #%td }", self->media->name, drive->vendor, drive->model, drive - drive->changer->drives);
		int failed = slot->changer->ops->load_slot(slot->changer, slot, drive);

		if (has_lock_on_slot) {
			slot->lock->ops->unlock(slot->lock);
			has_lock_on_slot = false;
			slot = NULL;
		}

		if (failed) {
			st_job_add_record(job->db_connect, st_log_level_error, job, st_job_record_notif_important, "failed to load media: %s from drive: { %s, %s, #%td }", self->media->name, drive->vendor, drive->model, drive - drive->changer->drives);

			if (has_lock_on_drive)
				drive->lock->ops->unlock(drive->lock);

			self->media->locked = false;
			self->media->lock->ops->unlock(self->media->lock);
			job->db_connect->ops->sync_media(job->db_connect, self->media);

			job->sched_status = st_job_status_error;

			return 3;
		}
	}

	if (job->db_status != st_job_status_stopped) {
		job->done = 0.6;

		if (self->media->type == st_media_type_readonly) {
			st_job_add_record(job->db_connect, st_log_level_error, job, st_job_record_notif_important, "Try to erase a write protected media");
			job->sched_status = st_job_status_error;

			job->db_connect->ops->sync_media(job->db_connect, self->media);
			self->media->locked = false;
			self->media->lock->ops->unlock(self->media->lock);
			job->db_connect->ops->sync_media(job->db_connect, self->media);

			if (drive != NULL && has_lock_on_drive)
				drive->lock->ops->unlock(drive->lock);

			return 1;
		}
	}

	bool quick_mode = true;
	if (st_hashtable_has_key(job->option, "quick mode")) {
		struct st_hashtable_value qm = st_hashtable_get(job->option, "quick mode");
		if (qm.type == st_hashtable_value_boolean)
			quick_mode = qm.value.boolean;
		else if (st_hashtable_val_can_convert(&qm, st_hashtable_value_boolean))
			quick_mode = st_hashtable_val_convert_to_bool(&qm);
	}

	int status = 0;
	if (job->db_status != st_job_status_stopped) {
		st_job_add_record(job->db_connect, st_log_level_info, job, st_job_record_notif_normal, "Erasing media in progress");
		status = drive->ops->format(drive, 0, quick_mode);

		if (status) {
			st_job_add_record(job->db_connect, st_log_level_info, job, st_job_record_notif_important, "Job: erase media finished with code = %d, num runs %ld", status, job->num_runs);
			job->sched_status = st_job_status_error;
			status = 1;
		} else {
			job->db_connect->ops->mark_archive_as_purged(job->db_connect, self->media, job);

			char * report = st_job_erase_media_make_report(job);
			if (report != NULL)
				job->db_connect->ops->add_report(job->db_connect, job, NULL, self->media, report);
			free(report);

			job->done = 1;
			st_job_add_record(job->db_connect, st_log_level_info, job, st_job_record_notif_important, "Job: erase media finished with code = OK, num runs %ld", job->num_runs);
		}
	} else {
		st_job_add_record(job->db_connect, st_log_level_warning, job, st_job_record_notif_important, "Job: erase media aborted by user before erasing media");
	}

	if (drive != NULL && has_lock_on_drive)
		drive->lock->ops->unlock(drive->lock);

	return status;
}

