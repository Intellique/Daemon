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
*  Copyright (C) 2013-2019, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// dgettext
#include <libintl.h>
// malloc
#include <stdlib.h>
// bzero
#include <strings.h>

#include <libstoriqone/changer.h>
#include <libstoriqone/media.h>
#include <libstoriqone/slot.h>
#include <libstoriqone-changer/log.h>

#include "changer.h"
#include "media.h"
#include "peer.h"

void sochgr_media_add_reader(struct sochgr_media * media, struct sochgr_peer * peer) {
	struct sochgr_peer_list * lp = sochgr_media_find_peer(media, peer);
	if (lp != NULL)
		return;

	struct sochgr_peer_list * pl = malloc(sizeof(struct sochgr_peer_list));
	bzero(pl, sizeof(struct sochgr_peer_list));

	pl->peer = peer;

	if (media->first == NULL)
		media->first = media->last = pl;
	else
		media->last = media->last->next = pl;
}

void sochgr_media_add_writer(struct sochgr_media * media, const char * media_name, struct sochgr_peer * peer, size_t size_need, bool append, struct so_database_connection * db) {
	struct sochgr_peer_list * pl = malloc(sizeof(struct sochgr_peer_list));
	bzero(pl, sizeof(struct sochgr_peer_list));

	pl->peer = peer;
	pl->size_need = size_need;
	pl->append = append;
	media->size_reserved += size_need;

	if (media->first == NULL)
		media->first = media->last = pl;
	else
		media->last = media->last->next = pl;

	struct so_changer_driver * driver = sochgr_changer_get();
	struct so_changer * changer = driver->device;

	sochgr_log_add_record(peer, so_job_status_waiting, db, so_log_level_notice, so_job_record_notif_normal,
		dgettext("libstoriqone-changer", "[%s | %s]: job (id: %s) reserve media '%s' for writing"),
		changer->vendor, changer->model, peer->job_id, media_name);
}

struct sochgr_peer_list * sochgr_media_find_peer(struct sochgr_media * media, struct sochgr_peer * peer) {
	struct sochgr_peer_list * ptr;
	for (ptr = media->first; ptr != NULL; ptr = ptr->next)
		if (ptr->peer == peer)
			return ptr;

	return NULL;
}

void sochgr_media_init(struct so_changer * changer) {
	unsigned int i;
	for (i = 0; i < changer->nb_slots; i++)
		sochgr_media_init_slot(changer->slots + i);
}

void sochgr_media_init_slot(struct so_slot * slot) {
	struct so_media * media = slot->media;
	if (media != NULL && media->private_data == NULL) {
		struct sochgr_media * md = media->private_data = malloc(sizeof(struct sochgr_media));
		bzero(md, sizeof(struct sochgr_media));
		md->first = md->last = NULL;
		md->size_reserved = 0;
	}
}

void sochgr_media_release(struct so_changer * changer) {
	unsigned int i;
	for (i = 0; i < changer->nb_slots; i++) {
		struct so_slot * sl = changer->slots + i;

		if (sl->media == NULL)
			continue;

		struct sochgr_media * md = sl->media->private_data;
		sl->media->private_data = NULL;

		struct sochgr_peer_list * peer, * next;
		for (peer = md->first; peer != NULL; peer = next) {
			next = peer->next;
			free(peer);
		}
		so_pool_free(md->future_pool);
		free(md);
	}
}

void sochgr_media_remove_peer(struct sochgr_media * media, struct sochgr_peer * peer) {
	if (media == NULL || media->first == NULL)
		return;

	struct sochgr_peer_list * old = NULL;

	if (media->first->peer == peer) {
		old = media->first;
		media->first = old->next;
		if (media->first == NULL)
			media->last = NULL;
	} else {
		struct sochgr_peer_list * ptr;
		for (ptr = media->first; ptr->next != NULL; ptr = ptr->next)
			if (ptr->next->peer == peer) {
				old = ptr->next;
				ptr->next = old->next;
				if (ptr->next == NULL)
					media->last = ptr;
				break;
			}
	}

	if (old == NULL)
		return;

	media->size_reserved -= old->size_need;
	free(old);
}
