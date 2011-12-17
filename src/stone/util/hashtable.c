/*************************************************************************\
*                            __________                                   *
*                           / __/_  __/__  ___  ___                       *
*                          _\ \  / / / _ \/ _ \/ -_)                      *
*                         /___/ /_/  \___/_//_/\__/                       *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  This file is a part of STone                                           *
*                                                                         *
*  STone is free software; you can redistribute it and/or                 *
*  modify it under the terms of the GNU General Public License            *
*  as published by the Free Software Foundation; either version 3         *
*  of the License, or (at your option) any later version.                 *
*                                                                         *
*  This program is distributed in the hope that it will be useful,        *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
*  GNU General Public License for more details.                           *
*                                                                         *
*  You should have received a copy of the GNU General Public License      *
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  Copyright (C) 2011, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Sat, 17 Dec 2011 17:36:32 +0100                         *
\*************************************************************************/

// calloc, free, malloc
#include <malloc.h>

#include "stone/util/hashtable.h"

void sa_hashtable_put2(struct sa_hashtable * hashtable, unsigned int index, struct sa_hashtable_node * new_node);
void sa_hashtable_rehash(struct sa_hashtable * hashtable);


struct sa_hashtable * sa_hashtable_new(unsigned long long (*compute_hash)(const void *)) {
	return sa_hashtable_new2(compute_hash, 0);
}

struct sa_hashtable * sa_hashtable_new2(unsigned long long (*compute_hash)(const void * key), void (*release_key_value)(void * key, void * value)) {
	if (!compute_hash)
		return 0;

	struct sa_hashtable * l_hash = malloc(sizeof(struct sa_hashtable));

	l_hash->nodes = calloc(sizeof(struct sa_hashtable_node *), 16);
	l_hash->nb_elements = 0;
	l_hash->size_node = 16;
	l_hash->allow_rehash = 1;
	l_hash->compute_hash = compute_hash;
	l_hash->release_key_value = release_key_value;

	return l_hash;
}

void sa_hashtable_free(struct sa_hashtable * hashtable) {
	if (!hashtable)
		return;

	sa_hashtable_clear(hashtable);
	free(hashtable->nodes);
	hashtable->nodes = 0;
	hashtable->compute_hash = 0;

	free(hashtable);
}


void sa_hashtable_clear(struct sa_hashtable * hashtable) {
	if (!hashtable)
		return;

	unsigned int i;
	for (i = 0; i < hashtable->size_node; i++) {
		struct sa_hashtable_node * ptr = hashtable->nodes[i];
		while (ptr != 0) {
			struct sa_hashtable_node * tmp = ptr;
			ptr = ptr->next;
			if (hashtable->release_key_value)
				hashtable->release_key_value(tmp->key, tmp->value);
			tmp->key = tmp->value = tmp->next = 0;
			free(tmp);
		}
		hashtable->nodes[i] = 0;
	}
	hashtable->nb_elements = 0;
}

short sa_hashtable_has_key(struct sa_hashtable * hashtable, const void * key) {
	if (!hashtable || !key)
		return 0;

	unsigned long long hash = hashtable->compute_hash(key);
	unsigned int index = hash % hashtable->size_node;

	const struct sa_hashtable_node * node = hashtable->nodes[index];
	while (node) {
		if (node->hash == hash)
			return 1;
		node = node->next;
	}

	return 0;
}

const void ** sa_hashtable_keys(struct sa_hashtable * hashtable) {
	if (!hashtable)
		return 0;

	const void ** keys = calloc(sizeof(void *), hashtable->nb_elements + 1);
	unsigned int iNode = 0, index = 0;
	while (iNode < hashtable->size_node) {
		struct sa_hashtable_node * node = hashtable->nodes[iNode];
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

void sa_hashtable_put(struct sa_hashtable * hashtable, void * key, void * value) {
	if (!hashtable || !key || !value)
		return;

	unsigned long long hash = hashtable->compute_hash(key);
	unsigned int index = hash % hashtable->size_node;

	struct sa_hashtable_node * new_node = malloc(sizeof(struct sa_hashtable_node));
	new_node->hash = hash;
	new_node->key = key;
	new_node->value = value;
	new_node->next = 0;

	sa_hashtable_put2(hashtable, index, new_node);
	hashtable->nb_elements++;
}

void sa_hashtable_put2(struct sa_hashtable * hashtable, unsigned int index, struct sa_hashtable_node * new_node) {
	struct sa_hashtable_node * node = hashtable->nodes[index];
	if (node) {
		if (node->hash == new_node->hash) {
			if (hashtable->release_key_value && node->key != new_node->key && node->value != new_node->value)
				hashtable->release_key_value(node->key, node->value);
			hashtable->nodes[index] = new_node;
			new_node->next = node->next;
			free(node);
			hashtable->nb_elements--;
		} else {
			short nbElement = 1;
			while (node->next) {
				if (node->next->hash == new_node->hash) {
					struct sa_hashtable_node * next = node->next;
					if (hashtable->release_key_value && next->key != new_node->key && next->value != new_node->value)
						hashtable->release_key_value(next->key, next->value);
					new_node->next = next->next;
					node->next = new_node;
					free(next);
					hashtable->nb_elements--;
					return;
				}
				node = node->next;
				nbElement++;
			}
			node->next = new_node;
			if (nbElement > 4)
				sa_hashtable_rehash(hashtable);
		}
	} else
		hashtable->nodes[index] = new_node;
}

void sa_hashtable_rehash(struct sa_hashtable * hashtable) {
	if (!hashtable->allow_rehash)
		return;

	hashtable->allow_rehash = 0;

	unsigned int old_size_node = hashtable->size_node;
	struct sa_hashtable_node ** old_nodes = hashtable->nodes;
	hashtable->nodes = calloc(sizeof(struct sa_hashtable_node *), hashtable->size_node << 1);
	hashtable->size_node <<= 1;

	unsigned int i;
	for (i = 0; i < old_size_node; i++) {
		struct sa_hashtable_node * node = old_nodes[i];
		while (node) {
			struct sa_hashtable_node * current_node = node;
			node = node->next;
			current_node->next = 0;

			unsigned int index = current_node->hash % hashtable->size_node;
			sa_hashtable_put2(hashtable, index, current_node);
		}
	}
	free(old_nodes);

	hashtable->allow_rehash = 1;
}

void * sa_hashtable_remove(struct sa_hashtable * hashtable, const void * key) {
	if (!hashtable || !key)
		return 0;

	unsigned long long hash = hashtable->compute_hash(key);
	unsigned int index = hash % hashtable->size_node;

	struct sa_hashtable_node * node = hashtable->nodes[index];
	if (node && node->hash == hash) {
		hashtable->nodes[index] = node->next;
		void * value = node->value;
		free(node);
		hashtable->nb_elements--;
		return value;
	}
	while (node->next) {
		if (node->next->hash == hash) {
			struct sa_hashtable_node * current_node = node->next;
			node->next = current_node->next;

			void * value = current_node->value;
			free(current_node);
			hashtable->nb_elements--;
			return value;
		}
		node->next = node->next->next;
	}

	return 0;
}

void * sa_hashtable_value(struct sa_hashtable * hashtable, const void * key) {
	if (!hashtable || !key)
		return 0;

	unsigned long long hash = hashtable->compute_hash(key);
	unsigned int index = hash % hashtable->size_node;

	struct sa_hashtable_node * node = hashtable->nodes[index];
	while (node) {
		if (node->hash == hash)
			return node->value;
		node = node->next;
	}

	return 0;
}

void ** sa_hashtable_values(struct sa_hashtable * hashtable) {
	if (!hashtable)
		return 0;

	void ** values = calloc(sizeof(void *), hashtable->nb_elements + 1);
	unsigned int iNode = 0, index = 0;
	while (iNode < hashtable->size_node) {
		struct sa_hashtable_node * node = hashtable->nodes[iNode];
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

