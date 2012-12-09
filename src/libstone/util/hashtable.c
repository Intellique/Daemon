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
*  Copyright (C) 2012, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Sun, 09 Dec 2012 11:33:25 +0100                         *
\*************************************************************************/

// calloc, free, malloc
#include <malloc.h>

#include <libstone/util/hashtable.h>

static struct st_hashtable_value mhv_null = { .type = st_hashtable_value_null };

static void st_hashtable_put2(struct st_hashtable * hashtable, unsigned int index, struct st_hashtable_node * new_node);
static void st_hashtable_rehash(struct st_hashtable * hashtable);


struct st_hashtable_value st_hashtable_val_boolean(bool val) {
	struct st_hashtable_value v = {
		.type          = st_hashtable_value_boolean,
		.value.boolean = val,
	};
	return v;
}

struct st_hashtable_value st_hashtable_val_custom(void * val) {
	struct st_hashtable_value v = {
		.type         = st_hashtable_value_custom,
		.value.custom = val,
	};
	return v;
}

struct st_hashtable_value st_hashtable_val_float(double val) {
	struct st_hashtable_value v = {
		.type           = st_hashtable_value_float,
		.value.floating = val,
	};
	return v;
}

struct st_hashtable_value st_hashtable_val_null() {
	struct st_hashtable_value v = {
		.type          = st_hashtable_value_null,
	};
	return v;
}

struct st_hashtable_value st_hashtable_val_signed_integer(int64_t val) {
	struct st_hashtable_value v = {
		.type                 = st_hashtable_value_signed_integer,
		.value.signed_integer = val,
	};
	return v;
}

struct st_hashtable_value st_hashtable_val_string(char * string) {
	struct st_hashtable_value v = {
		.type         = st_hashtable_value_string,
		.value.string = string,
	};
	return v;
}

struct st_hashtable_value st_hashtable_val_unsigned_integer(uint64_t val) {
	struct st_hashtable_value v = {
		.type                   = st_hashtable_value_unsigned_integer,
		.value.unsigned_integer = val,
	};
	return v;
}


void st_hashtable_clear(struct st_hashtable * hashtable) {
	if (hashtable == NULL)
		return;

	uint32_t i;
	for (i = 0; i < hashtable->size_node; i++) {
		struct st_hashtable_node * ptr = hashtable->nodes[i];
		hashtable->nodes[i] = NULL;

		while (ptr != NULL) {
			struct st_hashtable_node * tmp = ptr;
			ptr = ptr->next;

			if (hashtable->release_key_value) {
				if (tmp->value.type == st_hashtable_value_custom)
					hashtable->release_key_value(tmp->key, tmp->value.value.custom);
				else if (tmp->value.type == st_hashtable_value_string)
					hashtable->release_key_value(tmp->key, tmp->value.value.string);
				else
					hashtable->release_key_value(tmp->key, NULL);

				tmp->value.value.floating = 0;
			}

			tmp->key = tmp->next = NULL;
			free(tmp);
		}
	}

	hashtable->nb_elements = 0;
}

void st_hashtable_free(struct st_hashtable * hashtable) {
	if (hashtable == NULL)
		return;

	st_hashtable_clear(hashtable);

	free(hashtable->nodes);
	hashtable->nodes = NULL;
	hashtable->compute_hash = NULL;
	hashtable->release_key_value = NULL;

	free(hashtable);
}

struct st_hashtable_value st_hashtable_get(const struct st_hashtable * hashtable, const void * key) {
	if (hashtable == NULL || key == NULL)
		return mhv_null;

	uint64_t hash = hashtable->compute_hash(key);
	uint32_t index = hash % hashtable->size_node;

	struct st_hashtable_node * node = hashtable->nodes[index];
	while (node != NULL) {
		if (node->hash == hash)
			return node->value;
		node = node->next;
	}

	return mhv_null;
}

bool st_hashtable_has_key(const struct st_hashtable * hashtable, const void * key) {
	if (hashtable == NULL || key == NULL)
		return false;

	uint64_t hash = hashtable->compute_hash(key);
	uint32_t index = hash % hashtable->size_node;

	const struct st_hashtable_node * node = hashtable->nodes[index];
	while (node != NULL) {
		if (node->hash == hash)
			return true;

		node = node->next;
	}

	return false;
}

const void ** st_hashtable_keys(struct st_hashtable * hashtable, uint32_t * nb_value) {
	if (hashtable == NULL)
		return NULL;

	const void ** keys = calloc(sizeof(void *), hashtable->nb_elements + 1);
	uint32_t i_node = 0, index = 0;

	while (i_node < hashtable->size_node) {
		struct st_hashtable_node * node = hashtable->nodes[i_node];
		while (node != NULL) {
			keys[index] = node->key;
			index++;
			node = node->next;
		}
		i_node++;
	}
	keys[index] = 0;

	if (nb_value != NULL)
		*nb_value = hashtable->nb_elements;

	return keys;
}

struct st_hashtable * st_hashtable_new(st_hashtable_compute_hash_f ch) {
	return st_hashtable_new2(ch, NULL);
}

struct st_hashtable * st_hashtable_new2(st_hashtable_compute_hash_f ch, st_hashtable_free_f release) {
	if (ch == NULL)
		return NULL;

	struct st_hashtable * l_hash = malloc(sizeof(struct st_hashtable));

	l_hash->nodes = calloc(sizeof(struct st_hashtable_node *), 16);
	l_hash->nb_elements = 0;
	l_hash->size_node = 16;
	l_hash->allow_rehash = 1;
	l_hash->compute_hash = ch;
	l_hash->release_key_value = release;

	return l_hash;
}

void st_hashtable_put(struct st_hashtable * hashtable, void * key, struct st_hashtable_value value) {
	if (hashtable == NULL || key == NULL)
		return;

	uint64_t hash = hashtable->compute_hash(key);
	uint32_t index = hash % hashtable->size_node;

	struct st_hashtable_node * new_node = malloc(sizeof(struct st_hashtable_node));
	new_node->hash = hash;
	new_node->key = key;
	new_node->value = value;
	new_node->next = NULL;

	st_hashtable_put2(hashtable, index, new_node);
	hashtable->nb_elements++;
}

static void st_hashtable_put2(struct st_hashtable * hashtable, unsigned int index, struct st_hashtable_node * new_node) {
	struct st_hashtable_node * node = hashtable->nodes[index];
	if (node != NULL) {
		if (node->hash == new_node->hash) {
			if (hashtable->release_key_value != NULL)
				hashtable->release_key_value(node->key, node->value.type == st_hashtable_value_custom ? node->value.value.custom : NULL);

			hashtable->nodes[index] = new_node;
			new_node->next = node->next;
			free(node);
			hashtable->nb_elements--;
		} else {
			short nb_element = 1;
			while (node->next != NULL) {
				if (node->next->hash == new_node->hash) {
					struct st_hashtable_node * next = node->next;

					if (hashtable->release_key_value != NULL)
						hashtable->release_key_value(node->key, node->value.type == st_hashtable_value_custom ? node->value.value.custom : NULL);

					new_node->next = next->next;
					node->next = new_node;
					free(next);
					hashtable->nb_elements--;
					return;
				}
				node = node->next;
				nb_element++;
			}
			node->next = new_node;

			if (nb_element > 4)
				st_hashtable_rehash(hashtable);
		}
	} else
		hashtable->nodes[index] = new_node;
}

static void st_hashtable_rehash(struct st_hashtable * hashtable) {
	if (!hashtable->allow_rehash)
		return;

	hashtable->allow_rehash = 0;

	unsigned int old_size_node = hashtable->size_node;
	struct st_hashtable_node ** old_nodes = hashtable->nodes;
	hashtable->nodes = calloc(sizeof(struct st_hashtable_node *), hashtable->size_node << 1);
	hashtable->size_node <<= 1;

	unsigned int i;
	for (i = 0; i < old_size_node; i++) {
		struct st_hashtable_node * node = old_nodes[i];
		while (node != NULL) {
			struct st_hashtable_node * current_node = node;
			node = node->next;
			current_node->next = NULL;

			unsigned int index = current_node->hash % hashtable->size_node;
			st_hashtable_put2(hashtable, index, current_node);
		}
	}
	free(old_nodes);

	hashtable->allow_rehash = 1;
}

void st_hashtable_remove(struct st_hashtable * hashtable, const void * key) {
	if (hashtable == NULL || key == NULL)
		return;

	uint64_t hash = hashtable->compute_hash(key);
	uint32_t index = hash % hashtable->size_node;

	struct st_hashtable_node * node = hashtable->nodes[index];
	if (node == NULL)
		return;

	if (node->hash == hash) {
		hashtable->nodes[index] = node->next;

		if (node->value.type == st_hashtable_value_custom)
			hashtable->release_key_value(node->key, node->value.value.custom);
		else if (node->value.type == st_hashtable_value_string)
			hashtable->release_key_value(node->key, node->value.value.string);
		else
			hashtable->release_key_value(node->key, NULL);

		node->key = node->next = NULL;
		free(node);

		hashtable->nb_elements--;
		return;
	}

	while (node->next != NULL) {
		if (node->next->hash == hash) {
			struct st_hashtable_node * current_node = node->next;
			node->next = current_node->next;

			if (hashtable->release_key_value)
				hashtable->release_key_value(current_node->key, current_node->value.type == st_hashtable_value_custom ? current_node->value.value.custom : NULL);
			current_node->key = current_node->next = NULL;
			free(current_node);

			hashtable->nb_elements--;
			return;
		}
		node->next = node->next->next;
	}

	return;
}

struct st_hashtable_value * st_hashtable_values(struct st_hashtable * hashtable, uint32_t * nb_value) {
	if (hashtable == NULL)
		return NULL;

	struct st_hashtable_value * values = calloc(sizeof(struct st_hashtable_value), hashtable->nb_elements + 1);
	uint32_t i_node = 0, index = 0;
	while (i_node < hashtable->size_node) {
		struct st_hashtable_node * node = hashtable->nodes[i_node];
		while (node != NULL) {
			values[index] = node->value;
			index++;
			node = node->next;
		}
		i_node++;
	}
	values[index] = mhv_null;

	if (nb_value != NULL)
		*nb_value = hashtable->nb_elements;

	return values;
}

