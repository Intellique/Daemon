/***********************************************************************\
*       ______           ____     ___           __   _                  *
*      / __/ /____  ____/  _/__ _/ _ | ________/ /  (_)  _____ ____     *
*     _\ \/ __/ _ \/ __// // _ `/ __ |/ __/ __/ _ \/ / |/ / -_) __/     *
*    /___/\__/\___/_/ /___/\_, /_/ |_/_/  \__/_//_/_/|___/\__/_/        *
*                           /_/                                         *
*  -------------------------------------------------------------------  *
*  This file is a part of StorIqArchiver                                *
*                                                                       *
*  StorIqArchiver is free software; you can redistribute it and/or      *
*  modify it under the terms of the GNU General Public License          *
*  as published by the Free Software Foundation; either version 3       *
*  of the License, or (at your option) any later version.               *
*                                                                       *
*  This program is distributed in the hope that it will be useful,      *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of       *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
*  GNU General Public License for more details.                         *
*                                                                       *
*  You should have received a copy of the GNU General Public License    *
*  along with this program; if not, write to the Free Software          *
*  Foundation, Inc., 51 Franklin Street, Fifth Floor,                   *
*  Boston, MA  02110-1301, USA.                                         *
*                                                                       *
*  -------------------------------------------------------------------  *
*  Copyright (C) 2010, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Mon, 27 Sep 2010 16:28:35 +0200                       *
\***********************************************************************/

#include <malloc.h>

#include "storiqArchiver/hashtable.h"

void hashtable_put2(struct hashtable * hashtable, unsigned int index, struct hashtableNode * new_node);
void hashtable_rehash(struct hashtable * hashtable);


struct hashtable * hashtable_new(unsigned long long (*computeHash)(const void *)) {
	return hashtable_new2(computeHash, 0);
}

struct hashtable * hashtable_new2(unsigned long long (*computeHash)(const void * key), void (*releaseKeyValue)(void * key, void * value)) {
	if (!computeHash)
		return 0;

	struct hashtable * l_hash = malloc(sizeof(struct hashtable));

	l_hash->nodes = calloc(sizeof(struct hashtableNode *), 16);
	l_hash->nbElements = 0;
	l_hash->sizeNode = 16;
	l_hash->allowRehash = 1;
	l_hash->computeHash = computeHash;
	l_hash->releaseKeyValue = releaseKeyValue;

	return l_hash;
}

void hashtable_free(struct hashtable * hashtable) {
	if (!hashtable)
		return;

	unsigned int i;
	for (i = 0; i < hashtable->sizeNode; i++) {
		struct hashtableNode * ptr = hashtable->nodes[i];
		while (ptr != 0) {
			struct hashtableNode * tmp = ptr;
			ptr = ptr->next;
			if (hashtable->releaseKeyValue)
				hashtable->releaseKeyValue(tmp->key, tmp->value);
			tmp->key = tmp->value = tmp->next = 0;
			free(tmp);
		}
	}
	free(hashtable->nodes);
	hashtable->nodes = 0;
	hashtable->computeHash = 0;

	free(hashtable);
}


short hashtable_hasKey(struct hashtable * hashtable, const void * key) {
	if (!hashtable || !key)
		return 0;

	unsigned long long hash = hashtable->computeHash(key);
	unsigned int index = hash % hashtable->sizeNode;

	const struct hashtableNode * node = hashtable->nodes[index];
	while (node) {
		if (node->hash == hash)
			return 1;
		node = node->next;
	}

	return 0;
}

const void ** hashtable_keys(struct hashtable * hashtable) {
	if (!hashtable)
		return 0;

	const void ** keys = calloc(sizeof(void *), hashtable->nbElements + 1);
	unsigned int iNode = 0, index = 0;
	while (iNode < hashtable->sizeNode) {
		struct hashtableNode * node = hashtable->nodes[iNode];
		while (node) {
			keys[index] = node->key;
			index++;
			node = node->next;
		}
		iNode++;
	}
	keys[index] = 0;

	return keys;
}

void hashtable_put(struct hashtable * hashtable, void * key, void * value) {
	if (!hashtable || !key || !value)
		return;

	unsigned long long hash = hashtable->computeHash(key);
	unsigned int index = hash % hashtable->sizeNode;

	struct hashtableNode * new_node = malloc(sizeof(struct hashtableNode));
	new_node->hash = hash;
	new_node->key = key;
	new_node->value = value;
	new_node->next = 0;

	hashtable_put2(hashtable, index, new_node);
	hashtable->nbElements++;
}

void hashtable_put2(struct hashtable * hashtable, unsigned int index, struct hashtableNode * new_node) {
	struct hashtableNode * node = hashtable->nodes[index];
	if (node) {
		if (node->hash == new_node->hash) {
			if (hashtable->releaseKeyValue && node->key != new_node->key && node->value != new_node->value)
				hashtable->releaseKeyValue(node->key, node->value);
			hashtable->nodes[index] = new_node;
			new_node->next = node->next;
			free(node);
			hashtable->nbElements--;
		} else {
			short nbElement = 1;
			while (node->next) {
				if (node->next->hash == new_node->hash) {
					struct hashtableNode * next = node->next;
					if (hashtable->releaseKeyValue && next->key != new_node->key && next->value != new_node->value)
						hashtable->releaseKeyValue(next->key, next->value);
					new_node->next = next->next;
					node->next = new_node;
					free(next);
					hashtable->nbElements--;
					return;
				}
				node = node->next;
				nbElement++;
			}
			node->next = new_node;
			if (nbElement > 4)
				hashtable_rehash(hashtable);
		}
	} else
		hashtable->nodes[index] = new_node;
}

void hashtable_rehash(struct hashtable * hashtable) {
	if (!hashtable->allowRehash)
		return;

	hashtable->allowRehash = 0;

	unsigned int old_sizeNode = hashtable->sizeNode;
	struct hashtableNode ** old_nodes = hashtable->nodes;
	hashtable->nodes = calloc(sizeof(struct hashtableNode *), hashtable->sizeNode << 1);
	hashtable->sizeNode <<= 1;

	unsigned int i;
	for (i = 0; i < old_sizeNode; i++) {
		struct hashtableNode * node = old_nodes[i];
		while (node) {
			struct hashtableNode * current_node = node;
			node = node->next;
			current_node->next = 0;

			unsigned int index = current_node->hash % hashtable->sizeNode;
			hashtable_put2(hashtable, index, current_node);
		}
	}
	free(old_nodes);

	hashtable->allowRehash = 1;
}

void * hashtable_remove(struct hashtable * hashtable, const void * key) {
	if (!hashtable || !key)
		return 0;

	unsigned long long hash = hashtable->computeHash(key);
	unsigned int index = hash % hashtable->sizeNode;

	struct hashtableNode * node = hashtable->nodes[index];
	if (node && node->hash == hash) {
		hashtable->nodes[index] = node->next;
		void * value = node->value;
		free(node);
		hashtable->nbElements--;
		return value;
	}
	while (node->next) {
		if (node->next->hash == hash) {
			struct hashtableNode * current_node = node->next;
			node->next = current_node->next;

			void * value = current_node->value;
			free(current_node);
			hashtable->nbElements--;
			return value;
		}
		node->next = node->next->next;
	}

	return 0;
}

void * hashtable_value(struct hashtable * hashtable, const void * key) {
	if (!hashtable || !key)
		return 0;

	unsigned long long hash = hashtable->computeHash(key);
	unsigned int index = hash % hashtable->sizeNode;

	struct hashtableNode * node = hashtable->nodes[index];
	while (node) {
		if (node->hash == hash)
			return node->value;
		node = node->next;
	}

	return 0;
}

void ** hashtable_values(struct hashtable * hashtable) {
	if (!hashtable)
		return 0;

	void ** values = calloc(sizeof(void *), hashtable->nbElements + 1);
	unsigned int iNode = 0, index = 0;
	while (iNode < hashtable->sizeNode) {
		struct hashtableNode * node = hashtable->nodes[iNode];
		while (node) {
			values[index] = node->value;
			index++;
			node = node->next;
		}
		iNode++;
	}
	values[index] = 0;

	return values;
}

