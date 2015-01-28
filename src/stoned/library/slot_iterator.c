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
*  Last modified: Fri, 29 Nov 2013 18:18:32 +0100                            *
\****************************************************************************/

// free, malloc
#include <stdlib.h>

#include <libstone/library/drive.h>
#include <libstone/library/media.h>
#include <libstone/library/ressource.h>
#include <libstone/library/vtl.h>

#include "common.h"

struct st_slot_iterator_private {
	struct st_media_format * format;
	struct st_pool * pool;
	struct st_slot * next_media;

	struct st_changer * changers;
	unsigned int nb_changers;
	struct st_vtl_list * vtls;

	void (*move_to_next)(struct st_slot_iterator_private * self);

	unsigned int i_changer;
	unsigned int i_slot;
};

static void st_slot_iterator_private_free(struct st_slot_iterator * iterator);
static bool st_slot_iterator_private_has_next(struct st_slot_iterator * iterator);
static void st_slot_iterator_private_move_to_next_new_media(struct st_slot_iterator_private * self);
static bool st_slot_iterator_private_move_to_next_new_media2(struct st_slot_iterator_private * self, struct st_changer * ch);
static void st_slot_iterator_private_move_to_next_by_pool(struct st_slot_iterator_private * self);
static bool st_slot_iterator_private_move_to_next_by_pool2(struct st_slot_iterator_private * self, struct st_changer * ch);
static struct st_slot * st_slot_iterator_private_next(struct st_slot_iterator * iterator);

static struct st_slot_iterator_ops st_slot_iterator_private_ops_new_media = {
	.free     = st_slot_iterator_private_free,
	.has_next = st_slot_iterator_private_has_next,
	.next     = st_slot_iterator_private_next,
};

static struct st_slot_iterator_ops st_slot_iterator_private_ops_by_pool = {
	.free     = st_slot_iterator_private_free,
	.has_next = st_slot_iterator_private_has_next,
	.next     = st_slot_iterator_private_next,
};


struct st_slot_iterator * st_slot_iterator_by_new_media2(struct st_media_format * format, struct st_changer * changers, unsigned int nb_changers, struct st_vtl_list * vtls) {
	struct st_slot_iterator_private * self = malloc(sizeof(struct st_slot_iterator_private));
	self->format = format;
	self->pool = NULL;
	self->next_media = NULL;

	self->changers = changers;
	self->nb_changers = nb_changers;
	self->vtls = vtls;

	self->move_to_next = st_slot_iterator_private_move_to_next_new_media;

	self->i_changer = self->i_slot = 0;

	st_slot_iterator_private_move_to_next_new_media(self);

	struct st_slot_iterator * iter = malloc(sizeof(struct st_slot_iterator));
	iter->ops = &st_slot_iterator_private_ops_new_media;
	iter->data = self;

	return iter;
}

struct st_slot_iterator * st_slot_iterator_by_pool2(struct st_pool * pool, struct st_changer * changers, unsigned int nb_changers, struct st_vtl_list * vtls) {
	struct st_slot_iterator_private * self = malloc(sizeof(struct st_slot_iterator_private));
	self->format = NULL;
	self->pool = pool;
	self->next_media = NULL;

	self->changers = changers;
	self->nb_changers = nb_changers;
	self->vtls = vtls;

	self->move_to_next = st_slot_iterator_private_move_to_next_by_pool;

	self->i_changer = self->i_slot = 0;

	st_slot_iterator_private_move_to_next_by_pool(self);

	struct st_slot_iterator * iter = malloc(sizeof(struct st_slot_iterator));
	iter->ops = &st_slot_iterator_private_ops_by_pool;
	iter->data = self;

	return iter;
}

static void st_slot_iterator_private_free(struct st_slot_iterator * iterator) {
	if (iterator == NULL)
		return;

	struct st_slot_iterator_private * self = iterator->data;
	if (self != NULL) {
		struct st_slot * sl = self->next_media;
		if (sl != NULL) {
			if (sl->drive != NULL)
				sl->drive->lock->ops->unlock(sl->drive->lock);
			else
				sl->lock->ops->unlock(sl->lock);
		}
	}

	free(iterator->data);
	free(iterator);
}

static bool st_slot_iterator_private_has_next(struct st_slot_iterator * iterator) {
	struct st_slot_iterator_private * self = iterator->data;
	return self->next_media != NULL;
}

static void st_slot_iterator_private_move_to_next_new_media(struct st_slot_iterator_private * self) {
	for (; self->i_changer < self->nb_changers; self->i_changer++) {
		struct st_changer * ch = self->changers + self->i_changer;
		if (!ch->enabled)
			continue;

		if (st_slot_iterator_private_move_to_next_new_media2(self, ch))
			return;

		self->i_slot = 0;
	}

	while (self->vtls != NULL) {
		struct st_changer * ch = self->vtls->changer;
		if (ch->enabled && st_slot_iterator_private_move_to_next_new_media2(self, ch))
			return;

		self->vtls = self->vtls->next;
		self->i_slot = 0;
	}
}

static bool st_slot_iterator_private_move_to_next_new_media2(struct st_slot_iterator_private * self, struct st_changer * ch) {
	for (; self->i_slot < ch->nb_slots; self->i_slot++) {
		struct st_slot * sl = ch->slots + self->i_slot;
		if (!sl->enable)
			continue;

		struct st_drive * dr = sl->drive;
		if (dr != NULL && !dr->enabled)
			continue;

		if (dr != NULL && dr->lock->ops->timed_lock(dr->lock, 2000))
			continue;
		if (dr == NULL && sl->lock->ops->try_lock(sl->lock))
			continue;

		struct st_media * media = sl->media;
		if (media == NULL || media->status != st_media_status_new || media->format != self->format) {
			if (dr != NULL)
				dr->lock->ops->unlock(dr->lock);
			else
				sl->lock->ops->unlock(sl->lock);
			continue;
		}

		self->next_media = sl;
		self->i_slot++;

		return true;
	}

	return false;
}

static void st_slot_iterator_private_move_to_next_by_pool(struct st_slot_iterator_private * self) {
	for (; self->i_changer < self->nb_changers; self->i_changer++) {
		struct st_changer * ch = self->changers + self->i_changer;
		if (!ch->enabled)
			continue;

		if (st_slot_iterator_private_move_to_next_by_pool2(self, ch))
			return;

		self->i_slot = 0;
	}

	while (self->vtls != NULL) {
		struct st_changer * ch = self->vtls->changer;
		if (ch->enabled && st_slot_iterator_private_move_to_next_by_pool2(self, ch))
			return;

		self->vtls = self->vtls->next;
		self->i_slot = 0;
	}
}

static bool st_slot_iterator_private_move_to_next_by_pool2(struct st_slot_iterator_private * self, struct st_changer * ch) {
	for (; self->i_slot < ch->nb_slots; self->i_slot++) {
		struct st_slot * sl = ch->slots + self->i_slot;
		if (!sl->enable)
			continue;

		struct st_drive * dr = sl->drive;
		if (dr != NULL && !dr->enabled)
			continue;

		if (dr != NULL && dr->lock->ops->timed_lock(dr->lock, 2000))
			continue;
		if (dr == NULL && sl->lock->ops->try_lock(sl->lock))
			continue;

		struct st_media * media = sl->media;
		if (media == NULL || media->status != st_media_status_in_use || media->pool != self->pool) {
			if (dr != NULL)
				dr->lock->ops->unlock(dr->lock);
			else
				sl->lock->ops->unlock(sl->lock);
			continue;
		}

		self->next_media = sl;
		self->i_slot++;

		return true;
	}

	return false;
}

static struct st_slot * st_slot_iterator_private_next(struct st_slot_iterator * iterator) {
	struct st_slot_iterator_private * self = iterator->data;
	struct st_slot * sl = self->next_media;

	self->next_media = NULL;
	self->move_to_next(self);

	return sl;
}

