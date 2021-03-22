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

// dgettext
#include <libintl.h>
// free, malloc
#include <stdlib.h>
// strcmp
#include <string.h>
// bzero
#include <strings.h>
// sleep
#include <unistd.h>

#include <libstoriqone/json.h>
#include <libstoriqone/poll.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/socket.h>
#include <libstoriqone/string.h>
#include <libstoriqone/value.h>
#include <libstoriqone-changer/log.h>

#include "changer.h"
#include "drive.h"
#include "listen.h"
#include "media.h"
#include "peer.h"

static struct so_database_connection * sochgr_db = NULL;
static struct sochgr_peer * first_peer = NULL, * last_peer = NULL;
static unsigned int sochgr_nb_clients = 0;

static void sochgr_socket_accept(int fd_server, int fd_client, struct so_value * client);
static struct so_slot * sochgr_socket_find_slot_by_media(const char * medium_serial_number);
static void sochgr_socket_message(int fd, short event, void * data);
static void sochgr_socket_remove_peer(struct sochgr_peer * peer);

static void sochgr_socket_command_get_drives_config(struct sochgr_peer * peer, struct so_value * request, int fd);
static void sochgr_socket_command_get_media(struct sochgr_peer * peer, struct so_value * request, int fd);
static void sochgr_socket_command_release_media(struct sochgr_peer * peer, struct so_value * request, int fd);
static void sochgr_socket_command_reserve_media(struct sochgr_peer * peer, struct so_value * request, int fd);
static void sochgr_socket_command_sync(struct sochgr_peer * peer, struct so_value * request, int fd);

struct sochgr_socket_command {
	unsigned long hash;
	char * name;
	void (*function)(struct sochgr_peer * peer, struct so_value * request, int fd);
} commands[] = {
	{ 0, "get drives config", sochgr_socket_command_get_drives_config },
	{ 0, "get media",         sochgr_socket_command_get_media },
	{ 0, "release media",     sochgr_socket_command_release_media },
	{ 0, "reserve media",     sochgr_socket_command_reserve_media },
	{ 0, "sync",              sochgr_socket_command_sync },

	{ 0, NULL, NULL }
};


void sochgr_listen_configure(struct so_value * config) {
	unsigned int i;
	for (i = 0; commands[i].name != NULL; i++)
		commands[i].hash = so_string_compute_hash2(commands[i].name);

	so_socket_server(config, sochgr_socket_accept);
}

unsigned int sochgr_listen_nb_clients() {
	return sochgr_nb_clients;
}

void sochgr_listen_set_db_connection(struct so_database_connection * db) {
	sochgr_db = db;
}


static void sochgr_socket_accept(int fd_server __attribute__((unused)), int fd_client, struct so_value * client __attribute__((unused))) {
	struct so_changer_driver * driver = sochgr_changer_get();
	struct so_changer * changer = driver->device;

	struct sochgr_peer * peer = sochgr_peer_new(fd_client, changer->next_action == so_changer_action_put_offline);

	so_poll_register(fd_client, POLLIN | POLLHUP, sochgr_socket_message, peer, NULL);
	sochgr_nb_clients++;
}

static struct so_slot * sochgr_socket_find_slot_by_media(const char * medium_serial_number) {
	if (medium_serial_number == NULL)
		return NULL;

	struct so_changer_driver * driver = sochgr_changer_get();
	struct so_changer * changer = driver->device;

	unsigned int i;
	for (i = 0; i < changer->nb_slots; i++) {
		struct so_slot * sl = changer->slots + i;

		if (!sl->enable || sl->media == NULL || !sl->full || sl->media->type == so_media_type_cleaning)
			continue;

		if (strcmp(medium_serial_number, sl->media->medium_serial_number) == 0)
			return sl;
	}

	return NULL;
}

static void sochgr_socket_message(int fd, short event, void * data) {
	struct sochgr_peer * peer = data;

	if (event & POLLHUP) {
		sochgr_nb_clients--;

		struct so_changer_driver * driver = sochgr_changer_get();
		struct so_changer * changer = driver->device;
		unsigned int i;
		for (i = 0; i < changer->nb_slots; i++) {
			struct so_slot * sl = changer->slots + i;
			if (sl->media == NULL)
				continue;

			sochgr_media_remove_peer(sl->media->private_data, peer);
		}

		sochgr_socket_remove_peer(peer);
		sochgr_peer_free(peer);

		if (first_peer != NULL) {
			sleep(1);
			sochgr_socket_unlock(NULL, false);
		}

		return;
	}

	struct so_value * request = so_json_parse_fd(fd, -1);
	const char * command = NULL;
	if (request == NULL || so_value_unpack(request, "{sS}", "command", &command) < 0) {
		if (request != NULL)
			so_value_free(request);
		return;
	}

	const unsigned long hash = so_string_compute_hash2(command);
	unsigned int i;
	for (i = 0; commands[i].name != NULL; i++)
		if (hash == commands[i].hash) {
			if (peer->defer) {
				peer->command = commands[i].function;
				peer->request = request;
			} else {
				commands[i].function(peer, request, fd);
				so_value_free(request);
			}
			break;
		}

	if (commands[i].name == NULL && !peer->defer) {
		struct so_value * response = so_value_new_boolean(true);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
	}
}

void sochgr_listen_online() {
	struct sochgr_peer * peer;
	for (peer = first_peer; peer != NULL; peer = peer->next)
		if (peer->defer) {
			peer->defer = false;

			if (peer->command != NULL)
				peer->command(peer, peer->request, peer->fd);

			peer->command = NULL;
			so_value_free(peer->request);
			peer->request = NULL;
		}
}

static void sochgr_socket_remove_peer(struct sochgr_peer * peer) {
	if (first_peer == NULL)
		return;

	if (first_peer == peer)
		first_peer = peer->next;
	else {
		struct sochgr_peer * ptr;
		for (ptr = first_peer; ptr != NULL; ptr = ptr->next)
			if (ptr->next == peer) {
				ptr->next = peer->next;
				if (peer == last_peer)
					last_peer = ptr;
			}
	}
}

bool sochgr_socket_unlock(struct sochgr_peer * current_peer, bool no_wait) {
	struct so_changer_driver * driver = sochgr_changer_get();
	struct so_changer * changer = driver->device;

	unsigned int i, nb_free_drives = 0;
	for (i = 0; i < changer->nb_drives; i++) {
		struct so_drive * drive = changer->drives + i;
		struct so_slot * sl = drive->slot;

		if (!drive->enable || !sl->enable)
			continue;

		drive->ops->update_status(drive);

		if (!drive->ops->is_free(drive))
			continue;

		if (!sl->full) {
			nb_free_drives++;
			continue;
		}

		struct sochgr_media * mp = sl->media->private_data;
		struct sochgr_peer_list * lp;
		struct sochgr_peer * peer = NULL;
		for (lp = mp->first; lp != NULL; lp = lp->next)
			if (lp->waiting) {
				if (lp->append && !sl->media->append) {
					lp->peer->nb_waiting_medias--;
					lp->waiting = false;

					struct so_value * response = so_value_pack("{sbsbsiso}",
						"error", false,
						"append ok", false,
						"index", -1,
						"changer", so_changer_convert(changer)
					);
					so_json_encode_to_fd(response, peer->fd, true);
					so_value_free(response);
				} else {
					peer = lp->peer;
					break;
				}
			}

		if (peer == NULL) {
			nb_free_drives++;
			continue;
		}

		if (!drive->ops->check_support(drive, sl->media->media_format, lp->size_need > 0)) {
			if (changer->nb_drives < changer->nb_slots) {
				const char * volume_name = sl->volume_name;

				sochgr_log_add_record(peer, so_job_status_waiting, sochgr_db, so_log_level_notice, so_job_record_notif_normal,
					dgettext("libstoriqone-changer", "[%s | %s]: unloading media '%s' from drive #%d"),
					changer->vendor, changer->model, volume_name, drive->index);

				int failed = changer->ops->unload(peer, drive, sochgr_db);
				if (failed != 0) {
					sochgr_log_add_record(peer, so_job_status_waiting, sochgr_db, so_log_level_error, so_job_record_notif_important,
						dgettext("libstoriqone-changer", "[%s | %s]: unloading media '%s' from drive #%d completed with code = %d"),
						changer->vendor, changer->model, volume_name, drive->index, failed);

					struct so_value * response = so_value_pack("{sbsbsiso}",
						"error", true,
						"append ok", true,
						"index", -1,
						"changer", so_changer_convert(changer)
					);
					so_json_encode_to_fd(response, peer->fd, true);
					so_value_free(response);

					return false;
				} else
					sochgr_log_add_record(peer, so_job_status_waiting, sochgr_db, so_log_level_notice, so_job_record_notif_normal,
						dgettext("libstoriqone-changer", "[%s | %s]: unloading media '%s' from drive #%d completed with code = OK"),
						changer->vendor, changer->model, volume_name, drive->index);
			} else {
				sochgr_log_add_record(peer, so_job_status_waiting, sochgr_db, so_log_level_warning, so_job_record_notif_important,
					dgettext("libstoriqone-changer", "[%s | %s]: the drive (%s#%u) doesn't support media format (%s)"),
					changer->vendor, changer->model, drive->model, drive->index, sl->media->media_format->name);
			}
		}

		drive->ops->lock(drive, peer->job_id);

		peer->nb_waiting_medias--;
		lp->waiting = false;
		if (mp->future_pool != NULL)
			so_pool_free(mp->future_pool);
		mp->future_pool = NULL;

		sochgr_log_add_record(peer, so_job_status_waiting, sochgr_db, so_log_level_notice, so_job_record_notif_normal,
			dgettext("libstoriqone-changer", "[%s | %s]: job (id: %s) gets media '%s'"),
			changer->vendor, changer->model, peer->job_id, sl->media->name);

		struct so_value * response = so_value_pack("{sbsbsuso}",
			"error", false,
			"append ok", true,
			"index", drive->index,
			"changer", so_changer_convert(changer)
		);
		so_json_encode_to_fd(response, peer->fd, true);
		so_value_free(response);

		if (peer == current_peer)
			return true;
	}

	if (nb_free_drives == 0) {
		if (current_peer != NULL && no_wait) {
			sochgr_log_add_record(current_peer, so_job_status_waiting, sochgr_db, so_log_level_warning, so_job_record_notif_important,
				dgettext("libstoriqone-changer", "[%s | %s]: no free drive available"),
				changer->vendor, changer->model);

			struct so_value * response = so_value_pack("{sbsbsiso}",
				"error", false,
				"append ok", true,
				"index", -1,
				"changer", so_changer_convert(changer)
			);
			so_json_encode_to_fd(response, current_peer->fd, true);
			so_value_free(response);
		}
		return false;
	}

	struct sochgr_peer * peer;
	for (peer = first_peer; peer != NULL && nb_free_drives > 0; peer = peer->next) {
		if (peer->nb_waiting_medias == 0)
			continue;

		for (i = changer->nb_drives; i < changer->nb_slots && nb_free_drives > 0; i++) {
			struct so_slot * sl = changer->slots + i;
			if (!sl->enable || !sl->full)
				continue;

			struct so_media * media = sl->media;
			struct sochgr_media * mp = media->private_data;
			struct sochgr_peer_list * lp = sochgr_media_find_peer(mp, peer);
			if (lp == NULL || !lp->waiting)
				continue;

			if (lp->append && !media->append) {
				lp->peer->nb_waiting_medias--;
				lp->waiting = false;

				struct so_value * response = so_value_pack("{sbsbsiso}",
					"error", false,
					"append ok", false,
					"index", -1,
					"changer", so_changer_convert(changer)
				);
				so_json_encode_to_fd(response, peer->fd, true);
				so_value_free(response);

				continue;
			}

			unsigned int j;
			struct so_drive * drive = NULL;
			for (j = 0; j < changer->nb_drives && drive == NULL; j++) {
				struct so_drive * dr = changer->drives + j;
				if (!dr->enable || !dr->ops->check_support(dr, media->media_format, lp->size_need > 0))
					continue;

				if (dr->ops->is_free(dr) && !dr->slot->full)
					drive = dr;
			}
			for (j = 0; j < changer->nb_drives && drive == NULL; j++) {
				struct so_drive * dr = changer->drives + j;
				if (!dr->enable || !dr->ops->check_support(dr, media->media_format, lp->size_need > 0))
					continue;

				if (dr->ops->is_free(dr))
					drive = dr;
			}
			if (drive == NULL)
				break;

			if (drive->slot->full) {
				char * volume_name = strdup(drive->slot->volume_name);

				sochgr_log_add_record(peer, so_job_status_waiting, sochgr_db, so_log_level_notice, so_job_record_notif_normal,
					dgettext("libstoriqone-changer", "[%s | %s]: unloading media '%s' from drive #%d"),
					changer->vendor, changer->model, volume_name, drive->index);

				int failed = changer->ops->unload(peer, drive, sochgr_db);
				if (failed != 0) {
					sochgr_log_add_record(peer, so_job_status_waiting, sochgr_db, so_log_level_error, so_job_record_notif_normal,
						dgettext("libstoriqone-changer", "[%s | %s]: unloading media '%s' from drive #%d completed with code = %d"),
						changer->vendor, changer->model, volume_name, drive->index, failed);

					struct so_value * response = so_value_pack("{sbsbsiso}",
						"error", true,
						"append ok", true,
						"index", -1,
						"changer", so_changer_convert(changer)
					);
					so_json_encode_to_fd(response, peer->fd, true);
					so_value_free(response);

					free(volume_name);

					if (current_peer == peer)
						return false;
					else
						continue;
				} else
					sochgr_log_add_record(peer, so_job_status_waiting, sochgr_db, so_log_level_notice, so_job_record_notif_normal,
						dgettext("libstoriqone-changer", "[%s | %s]: unloading media '%s' from drive #%d completed with code = OK"),
						changer->vendor, changer->model, volume_name, drive->index);

				free(volume_name);
			}

			char * volume_name = strdup(sl->volume_name);
			sochgr_log_add_record(peer, so_job_status_waiting, sochgr_db, so_log_level_notice, so_job_record_notif_normal,
				dgettext("libstoriqone-changer", "[%s | %s]: loading media '%s' from slot #%u to drive #%d"),
				changer->vendor, changer->model, sl->volume_name, sl->index, drive->index);

			int failed = changer->ops->load(peer, sl, drive, sochgr_db);
			if (failed != 0) {
				sochgr_log_add_record(peer, so_job_status_waiting, sochgr_db, so_log_level_error, so_job_record_notif_important,
					dgettext("libstoriqone-changer", "[%s | %s]: loading media '%s' from slot #%u to drive #%d completed with code = %d"),
					changer->vendor, changer->model, sl->volume_name, sl->index, drive->index, failed);

				struct so_value * response = so_value_pack("{sbsbsiso}",
					"error", true,
					"append ok", true,
					"index", -1,
					"changer", so_changer_convert(changer)
				);
				so_json_encode_to_fd(response, peer->fd, true);
				so_value_free(response);

				free(volume_name);

				if (current_peer == peer)
					return false;
				else
					continue;
			} else
				sochgr_log_add_record(peer, so_job_status_waiting, sochgr_db, so_log_level_notice, so_job_record_notif_normal,
					dgettext("libstoriqone-changer", "[%s | %s]: loading media '%s' from slot #%u to drive #%d completed with code = OK"),
					changer->vendor, changer->model, drive->slot->volume_name, sl->index, drive->index);

			free(volume_name);

			drive->ops->lock(drive, peer->job_id);

			struct so_value * response = so_value_pack("{sbsbsuso}",
				"error", false,
				"append ok", true,
				"index", drive->index,
				"changer", so_changer_convert(changer)
			);
			so_json_encode_to_fd(response, peer->fd, true);
			so_value_free(response);

			peer->nb_waiting_medias--;
			lp->waiting = false;
			if (mp->future_pool != NULL)
				so_pool_free(mp->future_pool);
			mp->future_pool = NULL;

			sochgr_log_add_record(peer, so_job_status_waiting, sochgr_db, so_log_level_notice, so_job_record_notif_normal,
				dgettext("libstoriqone-changer", "[%s | %s]: job (id: %s) gets media '%s'"),
				changer->vendor, changer->model, peer->job_id, media->name);

			if (current_peer == peer)
				return true;
			else
				continue;
		}
	}

	if (current_peer != NULL && no_wait) {
		struct so_value * response = so_value_pack("{sbsbsiso}",
			"error", false,
			"append ok", true,
			"index", -1,
			"changer", so_changer_convert(changer)
		);
		so_json_encode_to_fd(response, current_peer->fd, true);
		so_value_free(response);
	}

	return false;
}


static void sochgr_socket_command_get_drives_config(struct sochgr_peer * peer, struct so_value * request, int fd) {
	free(peer->job_id);
	peer->job_id = NULL;

	so_value_unpack(request, "{s{s{sssi}}}",
		"params",
			"job",
				"id", &peer->job_id,
				"num run", &peer->job_num_run
	);

	so_json_encode_to_fd(sochgr_drive_get_configs(), fd, true);
}

static void sochgr_socket_command_get_media(struct sochgr_peer * peer, struct so_value * request, int fd) {
	const char * medium_serial_number = NULL;
	bool no_wait = false;

	so_value_unpack(request, "{s{sSsb}}",
		"params",
			"medium serial number", &medium_serial_number,
			"no wait", &no_wait
	);

	struct so_slot * sl = sochgr_socket_find_slot_by_media(medium_serial_number);

	struct so_media * media = sl != NULL ? sl->media : NULL;
	if (media == NULL) {
		struct so_value * response = so_value_pack("{sisb}",
			"index", -1,
			"error", true
		);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	struct sochgr_media * mp = media->private_data;
	struct sochgr_peer_list * lp = sochgr_media_find_peer(mp, peer);
	if (lp == NULL)
		goto error;

	peer->nb_waiting_medias++;
	lp->waiting = true;

	bool found = false;
	struct sochgr_peer * ptr;
	for (ptr = first_peer; !found && ptr != NULL; ptr = ptr->next)
		found = ptr == peer;

	if (!found) {
		if (first_peer == NULL)
			first_peer = last_peer = peer;
		else
			last_peer = last_peer->next = peer;
	}

	if (!sochgr_socket_unlock(peer, no_wait) && no_wait) {
		peer->nb_waiting_medias--;
		lp->waiting = false;
	}
	return;

	struct so_value * response;
error:
	response = so_value_pack("{sbsi}",
		"error", true,
		"index", -1
	);
	so_json_encode_to_fd(response, fd, true);
	so_value_free(response);
	return;
}

static void sochgr_socket_command_release_media(struct sochgr_peer * peer, struct so_value * request, int fd) {
	const char * medium_serial_number = NULL;
	so_value_unpack(request, "{s{sS}}", "params", "medium serial number", &medium_serial_number);

	if (medium_serial_number == NULL) {
		struct so_value * response = so_value_pack("{sb}", "status", false);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	struct so_slot * sl = sochgr_socket_find_slot_by_media(medium_serial_number);

	if (sl == NULL || sl->media == NULL) {
		struct so_value * response = so_value_pack("{sb}", "status", false);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	struct sochgr_media * mp = sl->media->private_data;
	sochgr_media_remove_peer(mp, peer);

	struct so_value * response = so_value_pack("{sb}", "status", true);
	so_json_encode_to_fd(response, fd, true);
	so_value_free(response);
}

static void sochgr_socket_command_reserve_media(struct sochgr_peer * peer, struct so_value * request, int fd) {
	const char * medium_serial_number = NULL;
	size_t size_need = 0;
	bool append = false;
	const char * archive_uuid = NULL;
	struct so_value * vpool = NULL;

	int nb_parsed = so_value_unpack(request, "{s{sSszsbsSso}}",
		"params",
			"medium serial number", &medium_serial_number,
			"size need", &size_need,
			"append", &append,
			"archive uuid", &archive_uuid,
			"pool", &vpool
	);

	if (nb_parsed < 4 || vpool == NULL) {
		struct so_value * response = so_value_pack("{si}", "returned", -1);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	struct so_changer_driver * driver = sochgr_changer_get();
	struct so_changer * changer = driver->device;

	struct so_slot * sl = sochgr_socket_find_slot_by_media(medium_serial_number);
	if (sl == NULL) {
		unsigned int i;
		for (i = 0; i < changer->nb_drives; i++) {
			struct so_drive * drive = changer->drives + i;
			if (drive->enable)
				drive->ops->update_status(drive);
		}

		sl = sochgr_socket_find_slot_by_media(medium_serial_number);
		if (sl == NULL) {
			struct so_value * response = so_value_pack("{si}", "returned", -1);
			so_json_encode_to_fd(response, fd, true);
			so_value_free(response);
			return;
		}
	}

	if (sl->drive != NULL)
		sl->drive->ops->update_status(sl->drive);

	struct so_media * media = sl->media;
	if (media == NULL) {
		struct so_value * response = so_value_pack("{si}", "returned", -1);
		so_json_encode_to_fd(response, fd, true);
		so_value_free(response);
		return;
	}

	struct so_pool * pool = NULL;
	if (vpool != NULL && vpool->type != so_value_null) {
		pool = malloc(sizeof(struct so_pool));
		bzero(pool, sizeof(struct so_pool));
		so_pool_sync(pool, vpool);
	} else
		pool = so_pool_dup(sl->media->pool);

	ssize_t result = -1L;
	struct sochgr_media * mp = media->private_data;
	ssize_t reserved_space = changer->ops->get_reserved_space(media->media_format);

	switch (media->status) {
		case so_media_status_new:
		case so_media_status_foreign:
			if (mp->future_pool != NULL && strcmp(mp->future_pool->uuid, pool->uuid) != 0)
				break;

			/**
			 * Assume that media is empty to allow formatting media
			 * See redmine issue #203
			 */
			media->free_block = media->total_block;
			mp->future_pool = pool;

		case so_media_status_in_use:
			if (size_need == 0) {
				result = 0;
				sochgr_media_add_reader(mp, peer);
			} else {
				bool supported_format = false;

				unsigned int i;
				for (i = 0; i < changer->nb_drives && !supported_format; i++) {
					struct so_drive * dr = changer->drives + i;
					if (dr->enable && dr->ops->check_format(dr, media, pool, archive_uuid))
						supported_format = true;
				}

				if (!supported_format)
					break;

				if (append && !media->append)
					break;

				if (pool->unbreakable_level == so_pool_unbreakable_level_archive) {
					if (media->free_block * media->block_size - mp->size_reserved >= size_need + reserved_space) {
						result = size_need;
						sochgr_media_add_writer(mp, so_media_get_name(media), peer, size_need, append, sochgr_db);
					}
				} else if (media->free_block * media->block_size - mp->size_reserved >= size_need + reserved_space) {
					result = size_need;
					sochgr_media_add_writer(mp, so_media_get_name(media), peer, size_need, append, sochgr_db);
				} else if (10 * media->free_block >= media->total_block) {
					result = media->free_block * media->block_size - mp->size_reserved - reserved_space;
					sochgr_media_add_writer(mp, so_media_get_name(media), peer, result, append, sochgr_db);
				}
			}
			break;

		default:
			break;
	}

	if (mp->future_pool != pool)
		so_pool_free(pool);

	struct so_value * response = so_value_pack("{sz}", "returned", result);
	so_json_encode_to_fd(response, fd, true);
	so_value_free(response);
}

static void sochgr_socket_command_sync(struct sochgr_peer * peer __attribute__((unused)), struct so_value * request __attribute__((unused)), int fd) {
	struct so_changer_driver * driver = sochgr_changer_get();
	struct so_changer * changer = driver->device;

	struct so_value * response = so_value_pack("{so}", "changer", so_changer_convert(changer));
	so_json_encode_to_fd(response, fd, true);
	so_value_free(response);
}
