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

// sleep
#include <unistd.h>

#include <libstoriqone/database.h>
#include <libstoriqone/drive.h>
#include <libstoriqone/media.h>
#include <libstoriqone/slot.h>
#include <libstoriqone/value.h>
#include <libstoriqone-job/changer.h>
#include <libstoriqone-job/media.h>

struct so_value * soj_medias = NULL;

static void soj_media_init(void) __attribute__((constructor));


static void soj_media_init() {
	soj_medias = so_value_pack("{}");
}

struct so_drive * soj_media_load(struct so_media * media) {
	struct so_slot * sl = soj_changer_find_slot(media);
	if (sl == NULL)
		return NULL;

	struct so_drive * dr = sl->drive;
	while (dr == NULL) {
		dr = sl->changer->ops->find_free_drive(sl->changer, media->format, true);

		if (dr == NULL)
			sleep(5);
	}

	if (sl->index != dr->slot->index && dr->slot->full)
		sl->changer->ops->unload(sl->changer, dr);

	if (!dr->slot->full)
		sl->changer->ops->load(sl->changer, sl, dr);

	return dr;
}

ssize_t soj_media_prepare(struct so_pool * pool, struct so_database_connection * db_connect) {
	if (!so_value_hashtable_has_key2(soj_medias, pool->uuid))
		so_value_hashtable_put2(soj_medias, pool->uuid, so_value_new_linked_list(), true);

	struct so_value * medias = so_value_hashtable_get2(soj_medias, pool->uuid, false, false);

	soj_changer_sync_all();

	ssize_t total_size_available = 0;

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

		total_size_available += media->free_block * media->block_size;
		so_value_list_push(medias, vmedia, false);
	}
	so_value_iterator_free(iter);
	so_value_free(selected_medias);

	return total_size_available;
}

ssize_t soj_media_prepare_unformatted(struct so_pool * pool, bool online, struct so_database_connection * db_connect) {
	struct so_value * medias = so_value_hashtable_get2(soj_medias, pool->uuid, false, false);
	if (medias == NULL)
		return 0;

	ssize_t total_size_available = 0;

	struct so_value * free_medias = db_connect->ops->get_free_medias(db_connect, pool->format, online);
	struct so_value_iterator * iter = so_value_list_get_iterator(free_medias);
	while (so_value_iterator_has_next(iter)) {
		struct so_value * vmedia = so_value_iterator_get_value(iter, false);
		struct so_media * media = so_value_custom_get(vmedia);

		total_size_available += media->total_block * media->block_size;
		so_value_list_push(medias, vmedia, false);
	}
	so_value_iterator_free(iter);
	so_value_free(free_medias);

	return total_size_available;
}

struct so_value * soj_media_reserve(struct so_pool * pool, size_t space_need, enum so_pool_unbreakable_level unbreakable_level) {
	struct so_value * medias = so_value_hashtable_get2(soj_medias, pool->uuid, false, false);
	if (medias == NULL)
		return NULL;

	struct so_value * reserved_medias = so_value_new_linked_list();
	bool ok = true;

	struct so_value_iterator * iter = so_value_list_get_iterator(medias);
	while (so_value_iterator_has_next(iter)) {
		struct so_value * vmedia = so_value_iterator_get_value(iter, false);
		struct so_media * media = so_value_custom_get(vmedia);

		bool reserve_media = false;
		if (unbreakable_level == so_pool_unbreakable_level_archive)
			reserve_media = space_need < media->free_block * media->block_size;
		else if (space_need < media->free_block * media->block_size || 10 * media->free_block >= media->total_block)
			reserve_media = true;

		if (!reserve_media)
			continue;

		struct so_slot * sl = soj_changer_find_slot(media);
		if (sl == NULL) {
			// TODO:
			break;
		}

		if (sl->changer->ops->reserve_media(sl->changer, sl, false, so_pool_autocheck_mode_none) == 0) {
			size_t free_size = media->free_block * media->block_size;
			if (free_size > space_need)
				space_need = 0;
			else
				space_need -= free_size;

			so_value_list_push(reserved_medias, vmedia, false);

			if (space_need > 0)
				continue;

			break;
		}
	}
	so_value_iterator_free(iter);

	if (ok)
		return reserved_medias;

	iter = so_value_list_get_iterator(reserved_medias);
	while (so_value_iterator_has_next(iter)) {
		struct so_value * vmedia = so_value_iterator_get_value(iter, false);
		struct so_media * media = so_value_custom_get(vmedia);

		struct so_slot * sl = soj_changer_find_slot(media);
		if (sl != NULL)
			sl->changer->ops->release_media(sl->changer, sl);
	}
	so_value_iterator_free(iter);
	so_value_free(reserved_medias);

	return NULL;
}

