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

#ifndef __LIBSTORIQONE_CHANGER_MEDIA_P_H__
#define __LIBSTORIQONE_CHANGER_MEDIA_P_H__

// bool
#include <stdbool.h>
// size_t
#include <sys/types.h>

#include <libstoriqone/media.h>

struct so_changer;
struct so_slot;
struct sochgr_peer;

struct sochgr_media {
	struct sochgr_peer_list {
		struct sochgr_peer * peer;
		size_t size_need;
		bool waiting;
		struct sochgr_peer_list * next;
	} * first, * last;
	size_t size_reserved;
};

void sochgr_media_add_reader(struct sochgr_media * media, struct sochgr_peer * peer);
void sochgr_media_add_writer(struct sochgr_media * media, struct sochgr_peer * peer, size_t size_need);
struct sochgr_peer_list * sochgr_media_find_peer(struct sochgr_media * media, struct sochgr_peer * peer);
void sochgr_media_init(struct so_changer * changer);
void sochgr_media_init_slot(struct so_slot * slot);
void sochgr_media_remove_peer(struct sochgr_media * media, struct sochgr_peer * peer);

#endif

