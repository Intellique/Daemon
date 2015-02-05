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
*  Last modified: Mon, 02 Feb 2015 18:16:37 +0100                            *
\****************************************************************************/

// json_*
#include <jansson.h>
// bool
#include <stdbool.h>
// sscanf
#include <stdio.h>
// free, malloc
#include <stdlib.h>
// strcmp, strdup
#include <string.h>
// sleep
#include <unistd.h>
// uuid_generate, uuid_unparse_lower
#include <uuid/uuid.h>

#include <libstone/checksum.h>
#include <libstone/database.h>
#include <libstone/host.h>
#include <libstone/io.h>
#include <libstone/library/drive.h>
#include <libstone/library/media.h>
#include <libstone/library/ressource.h>
#include <libstone/library/slot.h>
#include <libstone/log.h>
#include <libstone/script.h>
#include <libstone/job.h>
#include <libstone/user.h>
#include <libstone/util/hashtable.h>
#include <stoned/library/changer.h>

#include <libjob-format-media.chcksum>


struct st_job_format_media_private {
	struct st_media * media;
	struct st_pool * pool;
};

static bool st_job_format_media_check(struct st_job * job);
static void st_job_format_media_free(struct st_job * job);
static void st_job_format_media_init(void) __attribute__((constructor));
static void st_job_format_media_new_job(struct st_job * job);
static void st_job_format_media_on_error(struct st_job * job);
static void st_job_format_media_post_run(struct st_job * job);
static bool st_job_format_media_pre_run(struct st_job * job);
static int st_job_format_media_run(struct st_job * job);

static struct st_job_ops st_job_format_media_ops = {
	.check    = st_job_format_media_check,
	.free     = st_job_format_media_free,
	.on_error = st_job_format_media_on_error,
	.post_run = st_job_format_media_post_run,
	.pre_run  = st_job_format_media_pre_run,
	.run      = st_job_format_media_run,
};

static struct st_job_driver st_job_format_media_driver = {
	.name        = "format-media",
	.new_job     = st_job_format_media_new_job,
	.cookie      = NULL,
	.api_level   = {
		.checksum = STONE_CHECKSUM_API_LEVEL,
		.database = STONE_DATABASE_API_LEVEL,
		.job      = STONE_JOB_API_LEVEL,
	},
	.src_checksum   = STONE_JOB_FORMATMEDIA_SRCSUM,
};


static bool st_job_format_media_check(struct st_job * job) {
	struct st_job_format_media_private * self = job->data;

	st_job_add_record(job->db_connect, st_log_level_info, job, st_job_record_notif_important, "Start format media job (recover mode) (job name: %s), num runs %ld", job->name, job->num_runs);

	if (self->media == NULL) {
		st_job_add_record(job->db_connect, st_log_level_error, job, st_job_record_notif_important, "Media not found");
		return false;
	}
	if (self->media->type == st_media_type_cleaning) {
		st_job_add_record(job->db_connect, st_log_level_error, job, st_job_record_notif_important, "Try to format a cleaning media");
		return false;
	}

	struct st_pool * pool = st_pool_get_by_job(job, job->db_connect);

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

	while (stop == false) {
		switch (state) {
			case alert_user:
				if (has_alert_user == false)
					st_job_add_record(job->db_connect, st_log_level_warning, job, st_job_record_notif_important, "Media not found (named: %s)", self->media->name);
				has_alert_user = true;
				sleep(15);
				state = look_for_media;
				break;

			case changer_has_free_drive:
				// if drive is not NULL, we own also a lock on it
				drive = slot->changer->ops->find_free_drive(slot->changer, pool->format, true, true);
				if (drive != NULL) {
					has_lock_on_drive = true;
					stop = true;
				} else {
					sleep(5);
					state = media_in_drive;
				}
				break;

			case drive_is_free:
				if (drive->lock->ops->timed_lock(drive->lock, 2000)) {
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
					sleep(8);
					state = look_for_media;
				} else {
					has_lock_on_slot = true;
					state = changer_has_free_drive;
				}
				break;
		}
	}

	if (drive->slot->media != NULL && drive->slot->media != self->media) {
		st_job_add_record(job->db_connect, st_log_level_info, job, st_job_record_notif_normal, "unloading media: %s from drive: { %s, %s, #%td }", drive->slot->media->name, drive->vendor, drive->model, drive - drive->changer->drives);
		int failed = slot->changer->ops->unload(slot->changer, drive);
		if (failed) {
			st_job_add_record(job->db_connect, st_log_level_error, job, st_job_record_notif_normal, "failed to unload media: %s from drive: { %s, %s, #%td }", drive->slot->media->name, drive->vendor, drive->model, drive - drive->changer->drives);

			if (has_lock_on_slot)
				slot->lock->ops->unlock(slot->lock);
			if (has_lock_on_drive)
				drive->lock->ops->unlock(drive->lock);

			return false;
		}
	}

	if (drive->slot->media == NULL) {
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

			return false;
		}
	}

	if (drive == NULL) {
		st_job_add_record(job->db_connect, st_log_level_error, job, st_job_record_notif_important, "Internal error, drive is not supposed to be NULL");

		job->repetition = 0;
		job->done = 0;
		job->db_status = job->sched_status = st_job_status_error;

		return true;
	}

	char buffer[512];
	struct st_stream_reader * reader = drive->ops->get_raw_reader(drive, 0);
	ssize_t nb_read = reader->ops->read(reader, buffer, 512);
	reader->ops->close(reader);
	reader->ops->free(reader);

	if (has_lock_on_drive)
		drive->lock->ops->unlock(drive->lock);

	if (nb_read <= 0) {
		st_job_add_record(job->db_connect, st_log_level_info, job, st_job_record_notif_normal, "no header found, re-enable job");

		job->repetition = 1;
		job->done = 0;
		job->db_status = job->sched_status = st_job_status_scheduled;

		return false;
	}

	// M | STone (v0.1)
	// M | Tape format: version=1
	// O | Label: A0000002
	// M | Tape id: uuid=f680dd48-dd3e-4715-8ddc-a90d3e708914
	// M | Pool: name=Foo, uuid=07117f1a-2b13-11e1-8bcb-80ee73001df6
	// M | Block size: 32768
	// M | Checksum: crc32=1eb6931d
	char stone_version[65];
	int media_format_version = 0;
	int nb_parsed = 0;
	if (sscanf(buffer, "STone (%64[^)])\nTape format: version=%d\n%n", stone_version, &media_format_version, &nb_parsed) == 2) {
		char uuid[37];
		char name[65];
		char pool_id[37];
		char pool_name[65];
		ssize_t block_size;
		char checksum_name[12];
		char checksum_value[65];

		int nb_parsed2 = 0;
		int ok = 1;

		if (sscanf(buffer + nb_parsed, "Label: %36s\n%n", name, &nb_parsed2) == 1)
			nb_parsed += nb_parsed2;

		if (ok && sscanf(buffer + nb_parsed, "Tape id: uuid=%36s\n%n", uuid, &nb_parsed2) == 1)
			nb_parsed += nb_parsed2;
		else
			ok = 0;

		if (ok && sscanf(buffer + nb_parsed, "Pool: name=%65[^,], uuid=%36s\n%n", pool_name, pool_id, &nb_parsed2) == 2)
			nb_parsed += nb_parsed2;
		else
			ok = 0;

		if (ok && sscanf(buffer + nb_parsed, "Block size: %zd\n%n", &block_size, &nb_parsed2) == 1)
			nb_parsed += nb_parsed2;
		else
			ok = 0;

		if (ok)
			ok = sscanf(buffer + nb_parsed, "Checksum: %11[^=]=%64s\n", checksum_name, checksum_value) == 2;

		if (ok) {
			char * digest = st_checksum_compute(checksum_name, buffer, nb_parsed);
			ok = digest != NULL && !strcmp(checksum_value, digest);
			free(digest);
		}

		// checking parsed value
		if (ok && strcmp(uuid, self->media->uuid))
			ok = 0;

		if (ok && strcmp(pool_id, pool->uuid))
			ok = 0;

		if (ok) {
			st_job_add_record(job->db_connect, st_log_level_info, job, st_job_record_notif_normal, "header found, disable job");

			job->repetition = 0;
			job->done = 0;
			job->db_status = job->sched_status = st_job_status_finished;

			return true;
		} else {
			st_job_add_record(job->db_connect, st_log_level_info, job, st_job_record_notif_normal, "wrong header found, re-enable job");

			job->repetition = 1;
			job->done = 0;
			job->db_status = job->sched_status = st_job_status_scheduled;

			return true;
		}
	} else {
		st_job_add_record(job->db_connect, st_log_level_info, job, st_job_record_notif_normal, "media contains data but no header found, re-enable job");

		job->repetition = 1;
		job->done = 0;
		job->db_status = job->sched_status = st_job_status_scheduled;

		return true;
	}
}

static void st_job_format_media_free(struct st_job * job) {
	struct st_job_format_media_private * self = job->data;
	if (self != NULL) {
		job->db_connect->ops->close(job->db_connect);
		job->db_connect->ops->free(job->db_connect);
	}
	free(self);

	job->data = NULL;
}

static void st_job_format_media_init(void) {
	st_job_register_driver(&st_job_format_media_driver);
}

static void st_job_format_media_new_job(struct st_job * job) {
	struct st_job_format_media_private * self = malloc(sizeof(struct st_job_format_media_private));
	job->db_connect = job->db_config->ops->connect(job->db_config);
	self->media = st_media_get_by_job(job, job->db_connect);
	self->pool = st_pool_get_by_job(job, job->db_connect);

	job->data = self;
	job->ops = &st_job_format_media_ops;
}

static void st_job_format_media_on_error(struct st_job * j) {
	struct st_job_format_media_private * self = j->data;

	if (j->db_connect->ops->get_nb_scripts(j->db_connect, j->driver->name, st_script_type_on_error, self->pool) == 0)
		return;

	json_t * job = json_object();

	json_t * media = json_object();
	json_object_set_new(media, "uuid", json_string(self->media->uuid));
	if (self->media->label != NULL)
		json_object_set_new(media, "label", json_string(self->media->label));
	else
		json_object_set_new(media, "label", json_null());
	json_object_set_new(media, "name", json_string(self->media->name));

	json_t * pool = json_object();
	json_object_set_new(pool, "uuid", json_string(self->pool->uuid));
	json_object_set_new(pool, "name", json_string(self->pool->name));

	json_t * data = json_object();
	json_object_set_new(data, "job", job);
	json_object_set_new(data, "host", st_host_get_info());
	json_object_set_new(data, "media", media);
	json_object_set_new(data, "pool", pool);

	json_t * returned_data = st_script_run(j->db_connect, j, j->driver->name, st_script_type_on_error, self->pool, data);

	json_decref(returned_data);
	json_decref(data);
}

static void st_job_format_media_post_run(struct st_job * j) {
	struct st_job_format_media_private * self = j->data;

	if (j->db_connect->ops->get_nb_scripts(j->db_connect, j->driver->name, st_script_type_post, self->pool) == 0)
		return;

	json_t * job = json_object();

	json_t * media = json_object();
	json_object_set_new(media, "uuid", json_string(self->media->uuid));
	if (self->media->label != NULL)
		json_object_set_new(media, "label", json_string(self->media->label));
	else
		json_object_set_new(media, "label", json_null());
	json_object_set_new(media, "name", json_string(self->media->name));

	json_t * pool = json_object();
	json_object_set_new(pool, "uuid", json_string(self->pool->uuid));
	json_object_set_new(pool, "name", json_string(self->pool->name));

	json_t * data = json_object();
	json_object_set_new(data, "job", job);
	json_object_set_new(data, "host", st_host_get_info());
	json_object_set_new(data, "media", media);
	json_object_set_new(data, "pool", pool);

	json_t * returned_data = st_script_run(j->db_connect, j, j->driver->name, st_script_type_post, self->pool, data);

	json_decref(returned_data);
	json_decref(data);
}

static bool st_job_format_media_pre_run(struct st_job * j) {
	struct st_job_format_media_private * self = j->data;

	if (self->media == NULL) {
		st_job_add_record(j->db_connect, st_log_level_error, j, st_job_record_notif_important, "Error: no media associated with this job");
		return false;
	}
	if (self->pool == NULL) {
		st_job_add_record(j->db_connect, st_log_level_error, j, st_job_record_notif_important, "Error: no pool associated with this job");
		return false;
	}

	if (j->db_connect->ops->get_nb_scripts(j->db_connect, j->driver->name, st_script_type_pre, self->pool) == 0)
		return true;

	json_t * job = json_object();

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

	json_t * pool = json_object();
	json_object_set_new(pool, "uuid", json_string(self->pool->uuid));
	json_object_set_new(pool, "name", json_string(self->pool->name));

	json_t * sdata = json_object();
	json_object_set_new(sdata, "job", job);
	json_object_set_new(sdata, "host", st_host_get_info());
	json_object_set_new(sdata, "media", media);
	json_object_set_new(sdata, "pool", pool);

	json_t * returned_data = st_script_run(j->db_connect, j, j->driver->name, st_script_type_pre, self->pool, sdata);
	bool sr = st_io_json_should_run(returned_data);

	if (sr) {
		json_t * rdata = json_object_get(returned_data, "data");

		if (rdata != NULL) {
			size_t i, length = json_array_size(rdata);

			for (i = 0; i < length; i++) {
				json_t * elt = json_array_get(rdata, i);
				media = NULL;
				if (rdata != NULL)
					media = json_object_get(elt, "media");

				json_t * uuid = NULL;
				if (media != NULL)
					uuid = json_object_get(media, "uuid");

				if (uuid != NULL && json_is_string(uuid)) {
					const char * str_uuid = json_string_value(uuid);
					uuid_t id;
					if (uuid_parse(str_uuid, id) == 0) {
						st_job_add_record(j->db_connect, st_log_level_info, j, st_job_record_notif_normal, "script request to change uuid of media (old value: '%s') by '%s)", self->media->uuid, str_uuid);
						uuid_unparse_lower(id, self->media->uuid);
					} else {
						st_job_add_record(j->db_connect, st_log_level_warning, j, st_job_record_notif_normal, "script send an invalid uuid");
					}
				}

				json_t * name = NULL;
				if (media != NULL)
					name = json_object_get(media, "name");

				if (name != NULL && json_is_string(name)) {
					const char * str_name = json_string_value(name);
					st_job_add_record(j->db_connect, st_log_level_info, j, st_job_record_notif_normal, "script request to change name of media (old value: '%s') by '%s)", self->media->name, str_name);

					self->media->lock->ops->lock(self->media->lock);
					free(self->media->name);
					self->media->name = strdup(str_name);
					self->media->lock->ops->unlock(self->media->lock);
				}
			}
		}
	}

	json_decref(returned_data);
	json_decref(sdata);

	return sr;
}

int st_job_format_media_run(struct st_job * job) {
	struct st_job_format_media_private * self = job->data;

	st_job_add_record(job->db_connect, st_log_level_info, job, st_job_record_notif_important, "Start format media job (job name: %s), num runs %ld", job->name, job->num_runs);

	if (self->media == NULL) {
		st_job_add_record(job->db_connect, st_log_level_error, job, st_job_record_notif_important, "Media not found");
		job->sched_status = st_job_status_error;
		return 1;
	}
	if (self->media->type == st_media_type_cleaning) {
		st_job_add_record(job->db_connect, st_log_level_error, job, st_job_record_notif_important, "Try to format a cleaning media");
		job->sched_status = st_job_status_error;
		return 1;
	}
	if (self->media->type == st_media_type_worm && self->media->nb_volumes > 0) {
		st_job_add_record(job->db_connect, st_log_level_error, job, st_job_record_notif_important, "Try to format a worm media with data");
		job->sched_status = st_job_status_error;
		return 1;
	}
	if (self->media->status == st_media_status_error)
		st_job_add_record(job->db_connect, st_log_level_warning, job, st_job_record_notif_important, "Try to format a media with error status");

	if (self->pool == NULL) {
		st_job_add_record(job->db_connect, st_log_level_error, job, st_job_record_notif_important, "No pool specified");
		return 1;
	}
	st_pool_sync(self->pool, job->db_connect);
	if (self->pool->deleted) {
		st_job_add_record(job->db_connect, st_log_level_error, job, st_job_record_notif_important, "Try to format to a pool which is deleted");
		job->sched_status = st_job_status_error;
		return 1;
	}

	if (self->media->format != self->pool->format) {
		st_job_add_record(job->db_connect, st_log_level_error, job, st_job_record_notif_important, "Try to format a media whose type does not match the format of pool");
		job->sched_status = st_job_status_error;
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
				drive = slot->changer->ops->find_free_drive(slot->changer, self->pool->format, true, true);
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
			st_job_add_record(job->db_connect, st_log_level_error, job, st_job_record_notif_important, "Try to format a write protected media");
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

	// check blocksize
	enum update_blocksize {
		blocksize_nop,
		blocksize_set,
		blocksize_set_default,
	} do_update_block_size = blocksize_nop;
	ssize_t block_size = 0;
	char * blocksize = st_hashtable_get(job->option, "blocksize").value.string;
	if (blocksize != NULL) {
		if (sscanf(blocksize, "%zd", &block_size) == 1) {
			if (block_size > 0) {
				do_update_block_size = blocksize_set;

				// round block size to power of two
				ssize_t block_size_tmp = block_size;
				short p;
				for (p = 0; block_size > 1; p++, block_size >>= 1);
				block_size <<= p;

				if (block_size != block_size_tmp)
					st_job_add_record(job->db_connect, st_log_level_info, job, st_job_record_notif_normal, "Using block size %zd instead of %zd", block_size, block_size_tmp);
			} else {
				// ignore block size because bad value
				st_job_add_record(job->db_connect, st_log_level_info, job, st_job_record_notif_normal, "Wrong value of block size: %zd", block_size);
			}
		} else if (!strcmp(blocksize, "default"))
			do_update_block_size = blocksize_set_default;
	}

	// write header
	switch (do_update_block_size) {
		case blocksize_nop:
			st_job_add_record(job->db_connect, st_log_level_info, job, st_job_record_notif_normal, "Formatting new media");
			break;

		case blocksize_set:
			if (self->media->block_size != block_size) {
				st_job_add_record(job->db_connect, st_log_level_info, job, st_job_record_notif_normal, "Formatting new media (using block size: %zd bytes, previous value: %zd bytes)", block_size, self->media->block_size);
				self->media->block_size = block_size;
			} else
				st_job_add_record(job->db_connect, st_log_level_info, job, st_job_record_notif_normal, "Formatting new media (using block size: %zd bytes)", block_size);
			break;

		case blocksize_set_default:
			st_job_add_record(job->db_connect, st_log_level_info, job, st_job_record_notif_normal, "Formatting new media (using default block size: %zd bytes)", self->media->format->block_size);
			self->media->block_size = self->media->format->block_size;
			break;
	}

	int status = 0;
	if (job->db_status != st_job_status_stopped) {
		st_job_add_record(job->db_connect, st_log_level_info, job, st_job_record_notif_normal, "Formatting media in progress");
		status = st_media_write_header(drive, self->pool, job->db_connect);

		if (!status) {
			job->done = 0.8;
			st_job_add_record(job->db_connect, st_log_level_info, job, st_job_record_notif_important, "Checking media header in progress");
			status = !st_media_check_header(drive);
		}

		if (status) {
			st_job_add_record(job->db_connect, st_log_level_info, job, st_job_record_notif_important, "Job: format media finished with code = %d, num runs %ld", status, job->num_runs);
			job->sched_status = st_job_status_error;
			status = 1;
		} else {
			job->done = 1;
			st_job_add_record(job->db_connect, st_log_level_info, job, st_job_record_notif_important, "Job: format media finished with code = OK, num runs %ld", job->num_runs);
		}
	} else {
		st_job_add_record(job->db_connect, st_log_level_warning, job, st_job_record_notif_important, "Job: format media aborted by user before formatting media");
	}

	if (drive != NULL && has_lock_on_drive)
		drive->lock->ops->unlock(drive->lock);

	return status;
}

