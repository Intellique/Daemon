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
*  Copyright (C) 2013-2016, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// dgettext
#include <libintl.h>
// sleep
#include <unistd.h>

#include <libstoriqone/database.h>
#include <libstoriqone/drive.h>
#include <libstoriqone/log.h>
#include <libstoriqone/media.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/value.h>
#include <libstoriqone-job/changer.h>
#include <libstoriqone-job/drive.h>
#include <libstoriqone-job/job.h>

#include "media.h"

struct so_value * soj_media_available_by_pool = NULL;
struct so_value * soj_media_reserved_by_pool = NULL;

static void soj_media_exit(void) __attribute__((destructor));
static void soj_media_init(void) __attribute__((constructor));


void soj_media_check_reserved() {
	struct so_value_iterator * iter_pool = so_value_hashtable_get_iterator(soj_media_reserved_by_pool);
	while (so_value_iterator_has_next(iter_pool)) {
		struct so_value * medias = so_value_iterator_get_value(iter_pool, false);

		struct so_value_iterator * iter_media = so_value_list_get_iterator(medias);
		while (so_value_iterator_has_next(iter_media)) {
			struct so_value * vmedia = so_value_iterator_get_value(iter_media, false);
			struct so_media * media = so_value_custom_get(vmedia);

			struct so_slot * sl = soj_changer_find_slot(media);
			if (sl == NULL) {
				so_value_iterator_detach_previous(iter_media);
				so_value_list_push(soj_media_available_by_pool, vmedia, true);
			}
		}
		so_value_iterator_free(iter_media);
	}
	so_value_iterator_free(iter_pool);
}

struct so_drive * soj_media_find_and_load(struct so_media * media, bool no_wait, size_t size_need, bool * error, struct so_database_connection * db_connect) {
	return soj_media_find_and_load2(media, media->pool, no_wait, size_need, error, db_connect);
}

struct so_drive * soj_media_find_and_load2(struct so_media * media, struct so_pool * pool, bool no_wait, size_t size_need, bool * error, struct so_database_connection * db_connect) {
	struct so_job * job = soj_job_get();

	enum {
		alert_user,
		get_media,
		look_for_media,
		reserve_media,
	} state = look_for_media;
	bool stop = false, has_alert_user = false;
	struct so_slot * slot = NULL;
	ssize_t reserved_size = 0;
	struct so_drive * drive = NULL;

	while (!stop && !job->stopped_by_user) {
		switch (state) {
			case alert_user:
				job->status = so_job_status_waiting;
				if (!has_alert_user)
					soj_job_add_record(job, db_connect, so_log_level_warning, so_job_record_notif_important,
						dgettext("libstoriqone-job", "Media not found (named: %s)"), media->name);
				has_alert_user = true;

				sleep(15);

				state = look_for_media;
				job->status = so_job_status_running;
				soj_changer_sync_all();
				break;

			case get_media:
				drive = slot->changer->ops->get_media(slot->changer, media, no_wait, error);
				if (drive == NULL) {
					job->status = so_job_status_waiting;

					sleep(15);

					state = look_for_media;
					job->status = so_job_status_running;
					soj_changer_sync_all();
				} else
					stop = true;
				break;

			case look_for_media:
				slot = soj_changer_find_slot(media);
				state = slot != NULL ? reserve_media : alert_user;
				break;

			case reserve_media:
				reserved_size = slot->changer->ops->reserve_media(slot->changer, media, size_need, NULL, pool);
				if (reserved_size < 0) {
					job->status = so_job_status_waiting;

					sleep(15);

					state = look_for_media;
					job->status = so_job_status_running;
					soj_changer_sync_all();
				} else
					state = get_media;
				break;
		}
	}

	job->status = so_job_status_running;

	return drive;
}

struct so_drive * soj_media_find_and_load_next(struct so_pool * pool, bool no_wait, bool * error, struct so_database_connection * db_connect) {
	struct so_job * job = soj_job_get();

	struct so_value * medias = so_value_hashtable_get2(soj_media_reserved_by_pool, pool->uuid, false, false);
	struct so_media * media = NULL;

	if (so_value_list_get_length(medias) > 0) {
		struct so_value * vmedia = so_value_list_shift(medias);
		media = so_value_custom_get(vmedia);

		enum {
			alert_user,
			get_media,
			look_for_media,
		} state = look_for_media;

		bool stop = false, has_alert_user = false;
		struct so_slot * slot = NULL;
		struct so_drive * drive = NULL;

		while (!stop && !job->stopped_by_user) {
			switch (state) {
				case alert_user:
					job->status = so_job_status_waiting;
					if (!has_alert_user)
						soj_job_add_record(job, db_connect, so_log_level_warning, so_job_record_notif_important,
							dgettext("libstoriqone-job", "Media not found (named: %s)"), media->name);
					has_alert_user = true;

					sleep(15);

					state = look_for_media;
					job->status = so_job_status_running;
					soj_changer_sync_all();
					break;

				case get_media:
					drive = slot->changer->ops->get_media(slot->changer, media, no_wait, error);

					if (drive != NULL || no_wait)
						stop = true;
					else {
						job->status = so_job_status_waiting;

						sleep(15);

						state = look_for_media;
						job->status = so_job_status_running;
						soj_changer_sync_all();
					}
					break;

				case look_for_media:
					slot = soj_changer_find_slot(media);
					state = slot != NULL ? get_media : alert_user;
					break;
			}
		}

		if (drive == NULL)
			so_value_list_unshift(medias, vmedia, false);
		else {
			so_value_free(vmedia);

			drive->ops->sync(drive);

			media = drive->slot->media;
			if (media->status == so_media_status_new || media->status == so_media_status_foreign) {
				soj_job_add_record(job, db_connect, so_log_level_warning, so_job_record_notif_important,
					dgettext("libstoriqone-job", "Automatic formatting media '%s'"),
					media->name);

				int failed = drive->ops->format_media(drive, pool, NULL);
				if (failed != 0) {
					soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
						dgettext("libstoriqone-job", "Failed to format media"));

					return NULL;
				}

				drive->ops->sync(drive);

				if (drive->ops->check_header(drive)) {
					soj_job_add_record(job, db_connect, so_log_level_info, so_job_record_notif_important,
						dgettext("libstoriqone-job", "Checking media header '%s' was successful"),
						media->name);
				} else {
					soj_job_add_record(job, db_connect, so_log_level_error, so_job_record_notif_important,
						dgettext("storiqone-job-format-media", "Checking media header '%s': failed"),
						media->name);

					return NULL;
				}
			}
		}

		job->status = so_job_status_running;

		return drive;
	}

	return NULL;
}

static void soj_media_exit() {
	so_value_free(soj_media_available_by_pool);
	soj_media_available_by_pool = NULL;
	so_value_free(soj_media_reserved_by_pool);
	soj_media_reserved_by_pool = NULL;
}

static void soj_media_init() {
	soj_media_available_by_pool = so_value_new_hashtable2();
	soj_media_reserved_by_pool = so_value_new_hashtable2();
}

ssize_t soj_media_prepare(struct so_pool * pool, ssize_t size_needed, const char * archive_uuid, struct so_database_connection * db_connect) {
	if (!so_value_hashtable_has_key2(soj_media_available_by_pool, pool->uuid))
		so_value_hashtable_put2(soj_media_available_by_pool, pool->uuid, so_value_new_linked_list(), true);
	if (!so_value_hashtable_has_key2(soj_media_reserved_by_pool, pool->uuid))
		so_value_hashtable_put2(soj_media_reserved_by_pool, pool->uuid, so_value_new_linked_list(), true);

	struct so_value * available_medias = so_value_hashtable_get2(soj_media_available_by_pool, pool->uuid, false, false);
	struct so_value * reserved_medias = so_value_hashtable_get2(soj_media_reserved_by_pool, pool->uuid, false, false);

	soj_changer_sync_all();

	ssize_t total_reserved_size = 0;

	struct so_value * selected_medias = db_connect->ops->get_medias_of_pool(db_connect, pool);
	struct so_value_iterator * iter = so_value_list_get_iterator(selected_medias);
	while (so_value_iterator_has_next(iter)) {
		struct so_value * vmedia = so_value_iterator_get_value(iter, false);
		struct so_media * media = so_value_custom_get(vmedia);

		if (!media->append)
			continue;

		struct so_slot * sl = soj_changer_find_slot(media);
		if (sl == NULL)
			continue;

		if (total_reserved_size < size_needed) {
			ssize_t reserved_size = sl->changer->ops->reserve_media(sl->changer, media, size_needed - total_reserved_size, archive_uuid, pool);
			if (reserved_size > 0) {
				total_reserved_size += reserved_size;
				so_value_list_push(reserved_medias, vmedia, false);
			}
		} else
			so_value_list_push(available_medias, vmedia, false);
	}
	so_value_iterator_free(iter);
	so_value_free(selected_medias);

	if (pool->growable) {
		selected_medias = db_connect->ops->get_free_medias(db_connect, pool->media_format, true);
		iter = so_value_list_get_iterator(selected_medias);
		while (so_value_iterator_has_next(iter)) {
			struct so_value * vmedia = so_value_iterator_get_value(iter, false);
			struct so_media * media = so_value_custom_get(vmedia);

			struct so_slot * sl = soj_changer_find_slot(media);
			if (sl == NULL)
				continue;

			if (total_reserved_size < size_needed) {
				ssize_t reserved_size = sl->changer->ops->reserve_media(sl->changer, media, size_needed - total_reserved_size, archive_uuid, pool);
				if (reserved_size > 0) {
					total_reserved_size += reserved_size;
					so_value_list_push(reserved_medias, vmedia, false);
				}
			} else
				so_value_list_push(available_medias, vmedia, false);
		}
		so_value_iterator_free(iter);
		so_value_free(selected_medias);
	}

	selected_medias = db_connect->ops->get_medias_of_pool(db_connect, pool);
	iter = so_value_list_get_iterator(selected_medias);
	while (so_value_iterator_has_next(iter)) {
		struct so_value * vmedia = so_value_iterator_get_value(iter, false);
		struct so_media * media = so_value_custom_get(vmedia);

		if (!media->append)
			continue;

		struct so_slot * sl = soj_changer_find_slot(media);
		if (sl != NULL)
			continue;

		so_value_list_push(available_medias, vmedia, false);
	}
	so_value_iterator_free(iter);
	so_value_free(selected_medias);

	if (pool->growable) {
		selected_medias = db_connect->ops->get_free_medias(db_connect, pool->media_format, true);
		iter = so_value_list_get_iterator(selected_medias);
		while (so_value_iterator_has_next(iter)) {
			struct so_value * vmedia = so_value_iterator_get_value(iter, false);
			struct so_media * media = so_value_custom_get(vmedia);

			struct so_slot * sl = soj_changer_find_slot(media);
			if (sl != NULL)
				continue;

			so_value_list_push(available_medias, vmedia, false);
		}
		so_value_iterator_free(iter);
		so_value_free(selected_medias);
	}

	return total_reserved_size;
}

void soj_media_release_all_medias(struct so_pool * pool) {
	struct so_value * medias = so_value_hashtable_get2(soj_media_reserved_by_pool, pool->uuid, false, false);
	if (medias == NULL)
		return;

	struct so_value_iterator * iter = so_value_list_get_iterator(medias);
	while (so_value_iterator_has_next(iter)) {
		struct so_value * vmedia = so_value_iterator_get_value(iter, false);
		struct so_media * media = so_value_custom_get(vmedia);
		struct so_slot * sl = soj_changer_find_slot(media);
		sl->changer->ops->release_media(sl->changer, media);
	}
	so_value_iterator_free(iter);
}

void soj_media_reserve_new_media() {
	struct so_value_iterator * iter = so_value_hashtable_get_iterator(soj_media_available_by_pool);
	while (so_value_iterator_has_next(iter)) {
		struct so_value * medias = so_value_iterator_get_value(iter, false);

		struct so_value_iterator * iter_media = so_value_list_get_iterator(medias);
		while (so_value_iterator_has_next(iter_media)) {
			struct so_value * vmedia = so_value_iterator_get_value(iter_media, false);
			struct so_media * media = so_value_custom_get(vmedia);

			struct so_slot * sl = soj_changer_find_slot(media);
			if (sl == NULL) {
				so_value_iterator_detach_previous(iter);
				so_value_list_push(soj_media_available_by_pool, vmedia, true);
			}
		}
		so_value_iterator_free(iter_media);
	}
	so_value_iterator_free(iter);
}

