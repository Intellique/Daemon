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
*  Copyright (C) 2014, Clercin guillaume <gclercin@intellique.com>           *
\****************************************************************************/

// va_end, va_start
#include <stdarg.h>
// calloc, free, malloc
#include <stdlib.h>
// memmove, strcpy, strlen, strdup
#include <string.h>
// bzero
#include <strings.h>

#include "string.h"
#include "value.h"

struct st_value_ref {
	unsigned int nb_ref;
	unsigned int nb_total_ref;
};

static bool st_value_count_ref(struct st_value * base);
static void st_value_count_ref2(struct st_value * elt, struct st_value * found, struct st_value_ref * refs);
static struct st_value * st_value_new(enum st_value_type type, size_t extra);
static struct st_value * st_value_new_hashtable3(st_value_hashtable_compupte_hash_f compute_hash, bool weak_ref) __attribute__((warn_unused_result));
static struct st_value * st_value_pack_inner(const char ** format, va_list params);
static int st_value_unpack_inner(struct st_value * root, const char ** format, va_list params);
static bool st_value_valid_inner(struct st_value * value, const char ** format, va_list params);

static struct st_value_iterator * st_value_hashtable_get_iterator2(struct st_value * hash, bool weak_ref) __attribute__((nonnull,warn_unused_result));
static void st_value_hashtable_put_inner(struct st_value_hashtable * hash, unsigned int index, struct st_value_hashtable_node * new_node);
static void st_value_hashtable_rehash(struct st_value_hashtable * hash);
static void st_value_hashtable_release_node(struct st_value_hashtable_node * node, bool weak_ref);

static struct st_value_iterator * st_value_list_get_iterator2(struct st_value * list, bool weak_ref) __attribute__((nonnull,warn_unused_result));

static unsigned long long st_value_compute_addr(const struct st_value * key);

static struct st_value null_value = {
	.type = st_value_null,
	.shared = 0,
};


__asm__(".symver st_value_can_convert_v1, st_value_can_convert@@LIBSTONE_1.2");
bool st_value_can_convert_v1(struct st_value * val, enum st_value_type type) {
	if (val == NULL)
		return type == st_value_null;

	switch (val->type) {
		case st_value_array:
			switch (type) {
				case st_value_array:
				case st_value_linked_list:
					return true;

				default:
					return false;
			}

		case st_value_boolean:
			switch (type) {
				case st_value_boolean:
				case st_value_integer:
					return true;

				default:
					return false;
			}

		case st_value_custom:
			switch (type) {
				case st_value_custom:
					return true;

				default:
					return false;
			}

		case st_value_float:
			switch (type) {
				case st_value_integer:
				case st_value_float:
					return true;

				default:
					return false;
			}

		case st_value_hashtable:
			switch (type) {
				case st_value_hashtable:
					return true;

				default:
					return false;
			}

		case st_value_integer:
			switch (type) {
				case st_value_boolean:
				case st_value_integer:
				case st_value_float:
					return true;

				default:
					return false;
			}

		case st_value_linked_list:
			switch (type) {
				case st_value_array:
				case st_value_linked_list:
					return true;

				default:
					return false;
			}

		case st_value_null:
			switch (type) {
				case st_value_null:
					return true;

				default:
					return false;
			}

		case st_value_string:
			switch (type) {
				case st_value_null:
					return false;

				default:
					return true;
			}
	}

	return false;
}

__asm__(".symver st_value_convert_v1, st_value_convert@@LIBSTONE_1.2");
struct st_value * st_value_convert_v1(struct st_value * val, enum st_value_type type) {
	if (val == NULL)
		return &null_value;

	switch (val->type) {
		case st_value_array:
			switch (type) {
				case st_value_array:
					return st_value_share_v1(val);

				case st_value_linked_list: {
						struct st_value * ret = st_value_new_linked_list_v1();
						struct st_value_iterator * iter = st_value_list_get_iterator_v1(val);
						while (st_value_iterator_has_next_v1(iter)) {
							struct st_value * elt = st_value_iterator_get_value_v1(iter, true);
							st_value_list_push_v1(ret, elt, true);
						}
						st_value_iterator_free_v1(iter);
						return ret;
					}

				default:
					return &null_value;
			}

		case st_value_boolean:
			switch (type) {
				case st_value_boolean:
					return st_value_share_v1(val);

				case st_value_integer:
					return st_value_new_integer_v1(st_value_boolean_get_v1(val));

				default:
					return &null_value;
			}

		case st_value_custom:
			switch (type) {
				case st_value_custom:
					return st_value_share_v1(val);

				default:
					return &null_value;
			}

		case st_value_float:
			switch (type) {
				case st_value_integer:
					return st_value_new_integer_v1(st_value_float_get_v1(val));

				case st_value_float:
					return st_value_share_v1(val);

				default:
					return &null_value;
			}

		case st_value_hashtable:
			switch (type) {
				case st_value_hashtable:
					return st_value_share_v1(val);

				default:
					return &null_value;
			}

		case st_value_integer:
			switch (type) {
				case st_value_boolean:
					return st_value_new_boolean_v1(st_value_integer_get_v1(val) != 0);

				case st_value_integer:
					return st_value_share_v1(val);

				case st_value_float:
					return st_value_new_float_v1(st_value_integer_get_v1(val));

				default:
					return &null_value;
			}

		case st_value_linked_list:
			switch (type) {
				case st_value_array: {
					struct st_value * ret = st_value_new_array_v1(st_value_list_get_length_v1(val));
					struct st_value_iterator * iter = st_value_list_get_iterator_v1(val);
					while (st_value_iterator_has_next_v1(iter)) {
						struct st_value * elt = st_value_iterator_get_value_v1(iter, true);
						st_value_list_push_v1(ret, elt, true);
					}
					st_value_iterator_free_v1(iter);
					return ret;
				}

				case st_value_linked_list:
					return st_value_share_v1(val);

				default:
					return &null_value;
			}

		case st_value_null:
			return &null_value;

		case st_value_string:
			switch (type) {
				case st_value_string:
					return st_value_share_v1(val);

				default:
					return &null_value;
			}
	}

	return &null_value;
}

__asm__(".symver st_value_copy_v1, st_value_copy@@LIBSTONE_1.2");
struct st_value * st_value_copy_v1(struct st_value * val, bool deep_copy) {
	struct st_value * ret = &null_value;

	if (val == NULL)
		return ret;

	switch (val->type) {
		case st_value_array: {
				ret = st_value_new_array_v1(st_value_list_get_length_v1(val));

				struct st_value_iterator * iter = st_value_list_get_iterator_v1(val);
				while (st_value_iterator_has_next_v1(iter)) {
					struct st_value * elt = st_value_iterator_get_value_v1(iter, !deep_copy);
					if (deep_copy)
						elt = st_value_copy_v1(elt, true);
					st_value_list_push_v1(ret, val, false);
				}
				st_value_iterator_free_v1(iter);
			}
			break;

		case st_value_boolean:
			ret = st_value_new_boolean_v1(st_value_boolean_get_v1(val));
			break;

		case st_value_custom:
			ret = st_value_share_v1(val);
			break;

		case st_value_float:
			ret = st_value_new_float_v1(st_value_float_get_v1(val));
			break;

		case st_value_hashtable: {
				struct st_value_hashtable * hashtable = (struct st_value_hashtable *) (val + 1);
				ret = st_value_new_hashtable_v1(hashtable->compute_hash);

				struct st_value_iterator * iter = st_value_hashtable_get_iterator_v1(val);
				while (st_value_iterator_has_next_v1(iter)) {
					struct st_value * key = st_value_iterator_get_key_v1(iter, false, false);
					struct st_value * value = st_value_iterator_get_value_v1(iter, false);

					if (deep_copy) {
						key = st_value_copy_v1(key, true);
						value = st_value_copy_v1(value, true);
					} else {
						key = st_value_share_v1(key);
						value = st_value_share_v1(value);
					}

					st_value_hashtable_put_v1(ret, key, true, value, true);
				}
				st_value_iterator_free_v1(iter);
			}
			break;

		case st_value_integer:
			ret = st_value_new_integer_v1(st_value_integer_get_v1(val));
			break;

		case st_value_null:
			break;

		case st_value_linked_list: {
				ret = st_value_new_linked_list_v1();

				struct st_value_iterator * iter = st_value_list_get_iterator_v1(val);
				while (st_value_iterator_has_next_v1(iter)) {
					struct st_value * elt = st_value_iterator_get_value_v1(iter, !deep_copy);
					if (deep_copy)
						elt = st_value_copy_v1(elt, true);
					st_value_list_push_v1(ret, val, false);
				}
				st_value_iterator_free_v1(iter);
			}
			break;

		case st_value_string:
			ret = st_value_new_string_v1(st_value_string_get_v1(val));
			break;
	}

	return ret;
}

static bool st_value_count_ref(struct st_value * base) {
	struct st_value_ref refs = { 0, 0 };

	switch (base->type) {
		case st_value_array:
		case st_value_linked_list: {
				refs.nb_ref++;
				refs.nb_total_ref += base->shared;

				struct st_value * found = st_value_new_hashtable3(st_value_compute_addr, true);
				st_value_hashtable_put_v1(found, base, true, &null_value, true);

				struct st_value_iterator * iter = st_value_list_get_iterator2(base, true);
				while (st_value_iterator_has_next_v1(iter)) {
					struct st_value * elt = st_value_iterator_get_value_v1(iter, false);
					st_value_count_ref2(elt, found, &refs);
				}
				st_value_iterator_free_v1(iter);

				st_value_free_v1(found);
				break;
			}

		case st_value_hashtable: {
				refs.nb_ref++;
				refs.nb_total_ref += base->shared;

				struct st_value * found = st_value_new_hashtable3(st_value_compute_addr, true);
				st_value_hashtable_put_v1(found, base, true, &null_value, true);

				struct st_value_iterator * iter = st_value_hashtable_get_iterator2(base, true);
				while (st_value_iterator_has_next_v1(iter)) {
					struct st_value * key = st_value_iterator_get_key_v1(iter, false, false);
					st_value_count_ref2(key, found, &refs);

					struct st_value * elt = st_value_iterator_get_value_v1(iter, false);
					st_value_count_ref2(elt, found, &refs);
				}
				st_value_iterator_free_v1(iter);

				st_value_free_v1(found);
				break;
			}

		default:
			break;
	}

	return refs.nb_ref == refs.nb_total_ref && refs.nb_ref > 0;
}

static void st_value_count_ref2(struct st_value * elt, struct st_value * found, struct st_value_ref * refs) {
	if (elt->shared == 0)
		return;

	refs->nb_ref++;

	if (st_value_hashtable_has_key_v1(found, elt))
		return;

	refs->nb_total_ref += elt->shared;
	st_value_hashtable_put_v1(found, elt, true, &null_value, true);

	switch (elt->type) {
		case st_value_array:
		case st_value_linked_list: {
				struct st_value_iterator * iter = st_value_list_get_iterator2(elt, true);
				while (st_value_iterator_has_next_v1(iter)) {
					struct st_value * lst_elt = st_value_iterator_get_value_v1(iter, false);
					st_value_count_ref2(lst_elt, found, refs);
				}
				st_value_iterator_free_v1(iter);
				break;
			}

		case st_value_hashtable: {
				struct st_value_iterator * iter = st_value_hashtable_get_iterator2(elt, true);
				while (st_value_iterator_has_next_v1(iter)) {
					struct st_value * key = st_value_iterator_get_key_v1(iter, false, false);
					st_value_count_ref2(key, found, refs);

					struct st_value * hsh_elt = st_value_iterator_get_value_v1(iter, false);
					st_value_count_ref2(hsh_elt, found, refs);
				}
				st_value_iterator_free_v1(iter);
				break;
			}

		default:
			break;
	}
}

__asm__(".symver st_value_equals_v1, st_value_equals@@LIBSTONE_1.2");
bool st_value_equals_v1(struct st_value * a, struct st_value * b) {
	if (a == NULL && b == NULL)
		return true;

	if (a == NULL || b == NULL)
		return false;

	switch (a->type) {
		case st_value_array:
		case st_value_linked_list:
			switch (b->type) {
				case st_value_array:
				case st_value_linked_list: {
						struct st_value_iterator * iter_a = st_value_list_get_iterator_v1(a);
						struct st_value_iterator * iter_b = st_value_list_get_iterator_v1(b);

						bool equals = true;
						while (equals && st_value_iterator_has_next_v1(iter_a) && st_value_iterator_has_next_v1(iter_b)) {
							struct st_value * v_a = st_value_iterator_get_value_v1(iter_a, false);
							struct st_value * v_b = st_value_iterator_get_value_v1(iter_b, false);

							equals = st_value_equals_v1(v_a, v_b);
						}

						if (equals)
							equals = !st_value_iterator_has_next_v1(iter_a) && !st_value_iterator_has_next_v1(iter_b);

						st_value_iterator_free_v1(iter_a);
						st_value_iterator_free_v1(iter_b);

						return equals;
					}

				default:
					return false;
			}

		case st_value_boolean:
			switch (b->type) {
				case st_value_boolean:
					return false;

				case st_value_float:
					return (st_value_boolean_get_v1(a) && st_value_float_get_v1(b) != 0) || (!st_value_boolean_get_v1(a) && st_value_float_get_v1(b) == 0);

				case st_value_integer:
					return (st_value_boolean_get_v1(a) && st_value_integer_get_v1(b) != 0) || (!st_value_boolean_get_v1(a) && st_value_integer_get_v1(b) == 0);

				default:
					return false;
			}

		case st_value_custom:
			if (b->type == st_value_custom) {
				struct st_value_custom * ca = st_value_get_v1(a);
				struct st_value_custom * cb = st_value_get_v1(b);
				return ca->data == cb->data;
			}
			return false;

		case st_value_float:
			switch (b->type) {
				case st_value_boolean:
					return (st_value_float_get_v1(a) != 0 && st_value_boolean_get_v1(b)) || (st_value_float_get_v1(a) == 0 && !st_value_boolean_get_v1(b));

				case st_value_float:
					return st_value_float_get_v1(a) == st_value_float_get_v1(b);

				case st_value_integer:
					return st_value_float_get_v1(a) == st_value_integer_get_v1(b);

				default:
					return false;
			}

		case st_value_hashtable:
			if (b->type == st_value_hashtable) {
				struct st_value_hashtable * ha = st_value_get_v1(a);
				struct st_value_hashtable * hb = st_value_get_v1(b);

				if (ha->nb_elements != hb->nb_elements)
					return false;

				struct st_value_iterator * iter = st_value_hashtable_get_iterator_v1(a);
				bool equals = true;
				while (equals && st_value_iterator_has_next_v1(iter)) {
					struct st_value * key = st_value_iterator_get_key_v1(iter, true, false);

					if (!st_value_hashtable_has_key_v1(b, key)) {
						equals = false;
						break;
					}

					struct st_value * val_a = st_value_hashtable_get_v1(a, key, false, false);
					struct st_value * val_b = st_value_hashtable_get_v1(b, key, false, false);
					equals = st_value_equals_v1(val_a, val_b);
				}
				st_value_iterator_free_v1(iter);

				return equals;
			}
			return false;

		case st_value_integer:
			switch (b->type) {
				case st_value_boolean:
					return (st_value_integer_get_v1(a) != 0 && st_value_boolean_get_v1(b)) || (st_value_integer_get_v1(a) == 0 && !st_value_boolean_get_v1(b));

				case st_value_float:
					return st_value_integer_get_v1(a) == st_value_float_get_v1(b);

				case st_value_integer:
					return st_value_integer_get_v1(a) == st_value_integer_get_v1(b);

				default:
					return false;
			}

		case st_value_null:
			return b->type == st_value_null;

		case st_value_string:
			switch (b->type) {
				case st_value_string: {
						const char * sa = st_value_string_get_v1(a);
						const char * sb = st_value_string_get_v1(b);
						return !strcmp(sa, sb);
					}

				default:
					return false;
			}
	}

	return false;
}

__asm__(".symver st_value_free_v1, st_value_free@@LIBSTONE_1.2");
void st_value_free_v1(struct st_value * value) {
	if (value == NULL || value == &null_value || value->shared == 0)
		return;

	if (value->shared > 1 && !st_value_count_ref(value)) {
		value->shared--;
		return;
	}

	value->shared = 0;

	switch (value->type) {
		case st_value_array: {
				st_value_list_clear_v1(value);
				struct st_value_array * array = st_value_get_v1(value);
				free(array->values);
			}
			break;

		case st_value_custom: {
				struct st_value_custom * custom = st_value_get_v1(value);
				if (custom->release != NULL)
					custom->release(custom->data);
			}
			break;

		case st_value_hashtable: {
				st_value_hashtable_clear_v1(value);
				struct st_value_hashtable * hashtable = st_value_get_v1(value);
				free(hashtable->nodes);
			}
			break;

		case st_value_linked_list:
			st_value_list_clear_v1(value);
			break;

		default:
			break;
	}
	free(value);
}

void * st_value_get_v1(const struct st_value * value) {
	return (void *) (value + 1);
}

static struct st_value * st_value_new(enum st_value_type type, size_t extra) {
	struct st_value * val = malloc(sizeof(struct st_value) + extra);
	bzero(val, sizeof(struct st_value) + extra);

	val->type = type;
	val->shared = 1;
	return val;
}

__asm__(".symver st_value_new_array_v1, st_value_new_array@@LIBSTONE_1.2");
struct st_value * st_value_new_array_v1(unsigned int size) {
	struct st_value * val = st_value_new(st_value_array, sizeof(struct st_value_array));
	if (size == 0)
		size = 16;
	struct st_value_array * array = st_value_get_v1(val);
	array->values = calloc(sizeof(struct st_value *), size);
	array->nb_vals = 0;
	array->nb_preallocated = size;
	return val;
}

__asm__(".symver st_value_new_boolean_v1, st_value_new_boolean@@LIBSTONE_1.2");
struct st_value * st_value_new_boolean_v1(bool value) {
	struct st_value * val = st_value_new(st_value_boolean, sizeof(bool));
	bool * boolean = st_value_get_v1(val);
	*boolean = value;
	return val;
}

__asm__(".symver st_value_new_custom_v1, st_value_new_custom@@LIBSTONE_1.2");
struct st_value * st_value_new_custom_v1(void * value, st_value_free_f release) {
	struct st_value * val = st_value_new(st_value_custom, sizeof(struct st_value_custom));
	struct st_value_custom * custom = st_value_get_v1(val);
	custom->data = value;
	custom->release = release;
	return val;
}

__asm__(".symver st_value_new_float_v1, st_value_new_float@@LIBSTONE_1.2");
struct st_value * st_value_new_float_v1(double value) {
	struct st_value * val = st_value_new(st_value_float, sizeof(double));
	double * floating = st_value_get_v1(val);
	*floating = value;
	return val;
}

__asm__(".symver st_value_new_hashtable_v1, st_value_new_hashtable@@LIBSTONE_1.2");
struct st_value * st_value_new_hashtable_v1(st_value_hashtable_compupte_hash_f compute_hash) {
	return st_value_new_hashtable3(compute_hash, false);
}

__asm__(".symver st_value_new_hashtable2_v1, st_value_new_hashtable2@@LIBSTONE_1.2");
struct st_value * st_value_new_hashtable2_v1() {
	return st_value_new_hashtable_v1(st_string_compute_hash_v1);
}

static struct st_value * st_value_new_hashtable3(st_value_hashtable_compupte_hash_f compute_hash, bool weak_ref) {
	struct st_value * val = st_value_new(st_value_hashtable, sizeof(struct st_value_hashtable));
	struct st_value_hashtable * hashtable = st_value_get_v1(val);
	hashtable->nodes = calloc(16, sizeof(struct st_value_hashtable_node));
	hashtable->nb_elements = 0;
	hashtable->size_node = 16;
	hashtable->allow_rehash = true;
	hashtable->weak_ref = weak_ref;
	hashtable->compute_hash = compute_hash;
	return val;
}

__asm__(".symver st_value_new_integer_v1, st_value_new_integer@@LIBSTONE_1.2");
struct st_value * st_value_new_integer_v1(long long int value) {
	struct st_value * val = st_value_new(st_value_integer, sizeof(long long int));
	long long int * integer = st_value_get_v1(val);
	*integer = value;
	return val;
}

__asm__(".symver st_value_new_linked_list_v1, st_value_new_linked_list@@LIBSTONE_1.2");
struct st_value * st_value_new_linked_list_v1() {
	struct st_value * val = st_value_new(st_value_linked_list, sizeof(struct st_value_linked_list));
	struct st_value_linked_list * list = st_value_get_v1(val);
	list->first = list->last = NULL;
	list->nb_vals = 0;
	return val;
}

__asm__(".symver st_value_new_null_v1, st_value_new_null@@LIBSTONE_1.2");
struct st_value * st_value_new_null_v1() {
	return &null_value;
}

__asm__(".symver st_value_new_string_v1, st_value_new_string@@LIBSTONE_1.2");
struct st_value * st_value_new_string_v1(const char * value) {
	struct st_value * val = st_value_new(st_value_string, strlen(value) + 1);
	char * string = st_value_get_v1(val);
	strcpy(string, value);
	return val;
}

static struct st_value * st_value_pack_inner(const char ** format, va_list params) {
	switch (**format) {
		case 'b':
			return st_value_new_boolean_v1(va_arg(params, int));

		case 'f':
			return st_value_new_float_v1(va_arg(params, double));

		case 'i':
			return st_value_new_integer_v1(va_arg(params, long long int));

		case 'n':
			return &null_value;

		case 'o': {
				struct st_value * value = va_arg(params, struct st_value *);
				if (value != NULL)
					return value;
				return &null_value;
			}

		case 'O': {
				struct st_value * value = va_arg(params, struct st_value *);
				if (value != NULL)
					return st_value_share_v1(value);
				return &null_value;
			}

		case 's': {
				const char * str_val = va_arg(params, const char *);
				if (str_val != NULL)
					return st_value_new_string_v1(str_val);
				return &null_value;
			}

		case '[': {
				struct st_value * array = st_value_new_linked_list_v1();
				(*format)++;

				while (**format != ']') {
					struct st_value * elt = st_value_pack_inner(format, params);
					if (elt == NULL) {
						st_value_free_v1(array);
						return NULL;
					}
					st_value_list_push_v1(array, elt, true);
					(*format)++;
				}

				return array;
			}

		case '{': {
				struct st_value * object = st_value_new_hashtable2_v1();
				(*format)++;

				while (**format != '}') {
					if (**format != 's') {
						st_value_free_v1(object);
						return NULL;
					}

					struct st_value * key = st_value_pack_inner(format, params);
					(*format)++;

					struct st_value * value = st_value_pack_inner(format, params);
					if (value == NULL) {
						st_value_free_v1(object);
						st_value_free_v1(key);
						return NULL;
					}

					st_value_hashtable_put_v1(object, key, true, value, true);
					(*format)++;
				}

				return object;
			}
	}
	return NULL;
}

__asm__(".symver st_value_pack_v1, st_value_pack@@LIBSTONE_1.2");
struct st_value * st_value_pack_v1(const char * format, ...) {
	if (format == NULL)
		return NULL;

	va_list va;
	va_start(va, format);

	struct st_value * new_value = st_value_pack_inner(&format, va);

	va_end(va);

	return new_value;
}

__asm__(".symver st_value_share_v1, st_value_share@@LIBSTONE_1.2");
struct st_value * st_value_share_v1(struct st_value * value) {
	if (value == NULL)
		return NULL;

	if (value == &null_value)
		return value;

	value->shared++;
	return value;
}

static int st_value_unpack_inner(struct st_value * value, const char ** format, va_list params) {
	if (value == NULL)
		return 0;

	switch (**format) {
		case 'b':
			if (value->type == st_value_boolean) {
				bool * val = va_arg(params, bool *);
				if (val != NULL) {
					*val = st_value_boolean_get_v1(value);
					return 1;
				}
			}
			break;

		case 'f':
			if (value->type == st_value_float) {
				double * val = va_arg(params, double *);
				if (val != NULL) {
					*val = st_value_float_get_v1(value);
					return 1;
				}
			}
			break;

		case 'i':
			if (value->type == st_value_integer) {
				long long int * val = va_arg(params, long long int *);
				if (val != NULL) {
					*val = st_value_integer_get_v1(value);
					return 1;
				}
			}
			break;

		case 'n':
			return value->type == st_value_null;

		case 'o': {
				struct st_value ** val = va_arg(params, struct st_value **);
				if (val != NULL) {
					*val = value;
					return 1;
				}
			}
			break;

		case 'O': {
				struct st_value ** val = va_arg(params, struct st_value **);
				if (val != NULL) {
					*val = st_value_share_v1(value);
					return 1;
				}
			}
			break;

		case 's':
			if (value->type == st_value_string) {
				char ** val = va_arg(params, char **);
				if (val != NULL) {
					*val = strdup(st_value_string_get_v1(value));
					return 1;
				}
			}
			break;

		case '[': {
				(*format)++;

				int ret = 0;
				struct st_value_iterator * iter = st_value_list_get_iterator_v1(value);
				while (**format != ']') {
					if (!st_value_iterator_has_next_v1(iter))
						break;

					struct st_value * elt = st_value_iterator_get_value_v1(iter, false);
					ret += st_value_unpack_inner(elt, format, params);
					(*format)++;
				}
				st_value_iterator_free_v1(iter);

				(*format)++;
				return ret;
			}

		case '{': {
				(*format)++;

				int ret = 0;
				while (**format != '}') {
					struct st_value * key = st_value_pack_inner(format, params);
					if (key == NULL || !st_value_hashtable_has_key_v1(value, key)) {
						st_value_free_v1(key);
						return ret;
					}

					(*format)++;

					struct st_value * child = st_value_hashtable_get_v1(value, key, false, false);
					ret += st_value_unpack_inner(child, format, params);

					st_value_free_v1(key);

					(*format)++;
				}

				(*format)++;

				return ret;
			}
	}

	return 0;
}

__asm__(".symver st_value_unpack_v1, st_value_unpack@@LIBSTONE_1.2");
int st_value_unpack_v1(struct st_value * root, const char * format, ...) {
	if (root == NULL || format == NULL)
		return false;

	va_list va;
	va_start(va, format);

	int ret = st_value_unpack_inner(root, &format, va);

	va_end(va);

	return ret;
}

static bool st_value_valid_inner(struct st_value * value, const char ** format, va_list params) {
	if (value == NULL)
		return false;

	switch (**format) {
		case 'b':
			return value->type == st_value_boolean;

		case 'f':
			return value->type == st_value_float;

		case 'i':
			return value->type == st_value_integer;

		case 'n':
			return value->type == st_value_null;

		case 'o':
		case 'O':
			return true;

		case 's':
			return value->type == st_value_string;

		case '[': {
				(*format)++;

				bool ok = true;
				struct st_value_iterator * iter = st_value_list_get_iterator_v1(value);
				while (ok && **format != ']') {
					ok = st_value_iterator_has_next_v1(iter);
					if (!ok)
						break;

					struct st_value * elt = st_value_iterator_get_value_v1(iter, false);
					ok = st_value_valid_inner(elt, format, params);
					(*format)++;
				}
				st_value_iterator_free_v1(iter);

				(*format)++;
				return ok;
			}

		case '{': {
				(*format)++;

				bool ok = true;
				while (ok && **format != '}') {
					struct st_value * key = st_value_pack_inner(format, params);
					if (key == NULL || !st_value_hashtable_has_key_v1(value, key)) {
						st_value_free_v1(key);
						return false;
					}

					(*format)++;

					struct st_value * child = st_value_hashtable_get_v1(value, key, false, false);
					ok = st_value_valid_inner(child, format, params);

					st_value_free_v1(key);

					(*format)++;
				}

				(*format)++;

				return ok;
			}
	}
	return false;
}

__asm__(".symver st_value_valid_v1, st_value_valid@@LIBSTONE_1.2");
bool st_value_valid_v1(struct st_value * value, const char * format, ...) {
	if (format == NULL)
		return NULL;

	va_list va;
	va_start(va, format);

	bool ok = st_value_valid_inner(value, &format, va);

	va_end(va);

	return ok;
}

__asm__(".symver st_value_vpack_v1, st_value_vpack@@LIBSTONE_1.2");
struct st_value * st_value_vpack_v1(const char * format, va_list params) {
	if (format == NULL)
		return NULL;

	return st_value_pack_inner(&format, params);
}

__asm__(".symver st_value_vunpack_v1, st_value_vunpack@@LIBSTONE_1.2");
int st_value_vunpack_v1(struct st_value * root, const char * format, va_list params) {
	if (root == NULL || format == NULL)
		return false;

	return st_value_unpack_inner(root, &format, params);
}

__asm__(".symver st_value_vvalid_v1, st_value_vvalid@@LIBSTONE_1.2");
bool st_value_vvalid_v1(struct st_value * value, const char * format, va_list params) {
	if (format == NULL)
		return NULL;

	return st_value_valid_inner(value, &format, params);
}


__asm__(".symver st_value_boolean_get_v1, st_value_boolean_get@@LIBSTONE_1.2");
bool st_value_boolean_get_v1(const struct st_value * value) {
	bool * boolean = st_value_get_v1(value);
	return *boolean;
}


__asm__(".symver st_value_custom_get_v1, st_value_custom_get@@LIBSTONE_1.2");
void * st_value_custom_get_v1(const struct st_value * value) {
	struct st_value_custom * custom = st_value_get_v1(value);
	return custom->data;
}


__asm__(".symver st_value_float_get_v1, st_value_float_get@@LIBSTONE_1.2");
double st_value_float_get_v1(const struct st_value * value) {
	double * floating = st_value_get_v1(value);
	return *floating;
}


__asm__(".symver st_value_hashtable_clear_v1, st_value_hashtable_clear@@LIBSTONE_1.2");
void st_value_hashtable_clear_v1(struct st_value * hash) {
	if (hash == NULL || hash->type != st_value_hashtable)
		return;

	unsigned int i;
	struct st_value_hashtable * hashtable = st_value_get_v1(hash);
	for (i = 0; i < hashtable->size_node; i++) {
		struct st_value_hashtable_node * node = hashtable->nodes[i];
		hashtable->nodes[i] = NULL;

		while (node != NULL) {
			struct st_value_hashtable_node * tmp = node;
			node = node->next;

			st_value_hashtable_release_node(tmp, hashtable->weak_ref);
		}
	}

	hashtable->nb_elements = 0;
}

__asm__(".symver st_value_hashtable_get_v1, st_value_hashtable_get@@LIBSTONE_1.2");
struct st_value * st_value_hashtable_get_v1(struct st_value * hash, struct st_value * key, bool shared, bool detach) {
	if (hash == NULL || hash->type != st_value_hashtable || key == NULL)
		return &null_value;

	struct st_value_hashtable * hashtable = st_value_get_v1(hash);
	unsigned long long h = hashtable->compute_hash(key);
	unsigned int index = h % hashtable->size_node;

	struct st_value_hashtable_node * node = hashtable->nodes[index];
	while (node != NULL && node->hash != h)
		node = node->next;

	struct st_value * ret = &null_value;
	if (node != NULL && node->hash == h) {
		if (shared)
			ret = st_value_share_v1(node->value);
		else
			ret = node->value;

		if (detach)
			st_value_hashtable_remove_v1(hash, key);
	}

	return ret;
}

__asm__(".symver st_value_hashtable_get2_v1, st_value_hashtable_get2@@LIBSTONE_1.2");
struct st_value * st_value_hashtable_get2_v1(struct st_value * hash, const char * key, bool shared, bool detach) {
	struct st_value * k = st_value_new_string_v1(key);
	struct st_value * returned = st_value_hashtable_get_v1(hash, k, shared, detach);
	st_value_free_v1(k);
	return returned;
}

__asm__(".symver st_value_hashtable_get_iterator_v1, st_value_hashtable_get_iterator@@LIBSTONE_1.2");
struct st_value_iterator * st_value_hashtable_get_iterator_v1(struct st_value * hash) {
	return st_value_hashtable_get_iterator2(hash, false);
}

static struct st_value_iterator * st_value_hashtable_get_iterator2(struct st_value * hash, bool weak_ref) {
	if (hash == NULL || hash->type != st_value_hashtable)
		return NULL;

	if (!weak_ref)
		st_value_share_v1(hash);

	struct st_value_hashtable * value_hash = st_value_get_v1(hash);
	struct st_value_iterator * iter = malloc(sizeof(struct st_value_iterator) + sizeof(struct st_value_iterator_hashtable));
	struct st_value_iterator_hashtable * iter_hash = (struct st_value_iterator_hashtable *) (iter + 1);

	iter->value = hash;
	iter->weak_ref = weak_ref;
	iter_hash->node = NULL;
	iter_hash->i_elements = 0;

	while (iter_hash->i_elements <= value_hash->size_node && iter_hash->node == NULL)
		iter_hash->node = value_hash->nodes[iter_hash->i_elements++];

	return iter;
}

__asm__(".symver st_value_hashtable_has_key_v1, st_value_hashtable_has_key@@LIBSTONE_1.2");
bool st_value_hashtable_has_key_v1(struct st_value * hash, struct st_value * key) {
	if (hash == NULL || hash->type != st_value_hashtable || key == NULL)
		return false;

	struct st_value_hashtable * hashtable = st_value_get_v1(hash);
	unsigned long long h = hashtable->compute_hash(key);
	unsigned int index = h % hashtable->size_node;

	struct st_value_hashtable_node * node = hashtable->nodes[index];
	while (node != NULL && node->hash != h)
		node = node->next;

	return node != NULL && node->hash == h;
}

__asm__(".symver st_value_hashtable_has_key2_v1, st_value_hashtable_has_key2@@LIBSTONE_1.2");
bool st_value_hashtable_has_key2_v1(struct st_value * hash, const char * key) {
	struct st_value * k = st_value_new_string_v1(key);
	bool returned = st_value_hashtable_has_key_v1(hash, k);
	st_value_free_v1(k);
	return returned;
}

__asm__(".symver st_value_hashtable_keys_v1, st_value_hashtable_keys@@LIBSTONE_1.2");
struct st_value * st_value_hashtable_keys_v1(struct st_value * hash) {
	if (hash == NULL || hash->type != st_value_hashtable)
		return &null_value;

	struct st_value_hashtable * hashtable = st_value_get_v1(hash);
	struct st_value * list = st_value_new_array_v1(hashtable->nb_elements);
	unsigned int i;
	for (i = 0; i < hashtable->size_node; i++) {
		struct st_value_hashtable_node * node = hashtable->nodes[i];
		while (node != NULL) {
			st_value_list_push_v1(list, node->key, false);
			node = node->next;
		}
	}

	return list;
}

__asm__(".symver st_value_hashtable_put_v1, st_value_hashtable_put@@LIBSTONE_1.2");
void st_value_hashtable_put_v1(struct st_value * hash, struct st_value * key, bool new_key, struct st_value * value, bool new_value) {
	if (hash == NULL || hash->type != st_value_hashtable || key == NULL || key->type == st_value_null || value == NULL)
		return;

	struct st_value_hashtable * hashtable = st_value_get_v1(hash);
	unsigned long long h = hashtable->compute_hash(key);
	unsigned int index = h % hashtable->size_node;

	if (!hashtable->weak_ref && !new_key)
		key = st_value_share_v1(key);
	if (!hashtable->weak_ref && !new_value)
		value = st_value_share_v1(value);

	struct st_value_hashtable_node * node = malloc(sizeof(struct st_value_hashtable_node));
	node->hash = h;
	node->key = key;
	node->value = value;
	node->next = NULL;

	st_value_hashtable_put_inner(hashtable, index, node);
	hashtable->nb_elements++;
}

__asm__(".symver st_value_hashtable_put2_v1, st_value_hashtable_put2@@LIBSTONE_1.2");
void st_value_hashtable_put2_v1(struct st_value * hash, const char * key, struct st_value * value, bool new_value) {
	struct st_value * k = st_value_new_string_v1(key);
	st_value_hashtable_put_v1(hash, k, true, value, new_value);
}

static void st_value_hashtable_put_inner(struct st_value_hashtable * hashtable, unsigned int index, struct st_value_hashtable_node * new_node) {
	struct st_value_hashtable_node * node = hashtable->nodes[index];

	if (node != NULL) {
		if (node->hash == new_node->hash) {
			hashtable->nodes[index] = new_node;
			new_node->next = node->next;

			st_value_hashtable_release_node(node, hashtable->weak_ref);
		} else {
			short nb_elt = 1;
			while (node->next != NULL) {
				if (node->next->hash == new_node->hash) {
					struct st_value_hashtable_node * old = node->next;
					node->next = old->next;

					st_value_hashtable_release_node(old, hashtable->weak_ref);
				}

				node = node->next;
				nb_elt++;
			}

			node->next = new_node;
			if (nb_elt > 4)
				st_value_hashtable_rehash(hashtable);
		}
	} else
		hashtable->nodes[index] = new_node;
}

static void st_value_hashtable_rehash(struct st_value_hashtable * hashtable) {
	if (!hashtable->allow_rehash)
		return;

	hashtable->allow_rehash = false;

	struct st_value_hashtable_node ** old_nodes = hashtable->nodes;
	unsigned int old_size_nodes = hashtable->size_node;

	hashtable->size_node <<= 1;
	hashtable->nodes = calloc(hashtable->size_node, sizeof(struct st_value_hashtable_node *));

	unsigned int i;
	for (i = 0; i < old_size_nodes; i++) {
		struct st_value_hashtable_node * node = old_nodes[i];
		while (node != NULL) {
			struct st_value_hashtable_node * current_node = node;
			node = node->next;
			current_node->next = NULL;

			unsigned long long h = current_node->hash % hashtable->size_node;
			st_value_hashtable_put_inner(hashtable, h, current_node);
		}
	}
}

static void st_value_hashtable_release_node(struct st_value_hashtable_node * node, bool weak_ref) {
	if (!weak_ref) {
		st_value_free_v1(node->key);
		st_value_free_v1(node->value);
	}
	free(node);
}

__asm__(".symver st_value_hashtable_remove_v1, st_value_hashtable_remove@@LIBSTONE_1.2");
void st_value_hashtable_remove_v1(struct st_value * hash, struct st_value * key) {
	if (hash == NULL || hash->type != st_value_hashtable || key == NULL)
		return;

	struct st_value_hashtable * hashtable = st_value_get_v1(hash);
	unsigned long long h = hashtable->compute_hash(key);
	unsigned int index = h % hashtable->size_node;

	struct st_value_hashtable_node * node = hashtable->nodes[index];
	if (node != NULL && node->hash == h) {
		hashtable->nodes[index] = node->next;

		st_value_hashtable_release_node(node, hashtable->weak_ref);
		hashtable->nb_elements--;

		return;
	}

	while (node->next != NULL && node->next->hash != h)
		node = node->next;

	if (node->next->hash == h) {
		struct st_value_hashtable_node * tmp = node->next;
		node->next = tmp->next;

		st_value_hashtable_release_node(tmp, hashtable->weak_ref);
		hashtable->nb_elements--;
	}
}

__asm__(".symver st_value_hashtable_remove2_v1, st_value_hashtable_remove2@@LIBSTONE_1.2");
void st_value_hashtable_remove2_v1(struct st_value * hash, const char * key) {
	struct st_value * k = st_value_new_string_v1(key);
	st_value_hashtable_remove_v1(hash, k);
	st_value_free_v1(k);
}

__asm__(".symver st_value_hashtable_values_v1, st_value_hashtable_values@@LIBSTONE_1.2");
struct st_value * st_value_hashtable_values_v1(struct st_value * hash) {
	if (hash == NULL || hash->type != st_value_hashtable)
		return &null_value;

	struct st_value_hashtable * hashtable = st_value_get_v1(hash);
	struct st_value * list = st_value_new_array_v1(hashtable->nb_elements);
	unsigned int i;
	for (i = 0; i < hashtable->size_node; i++) {
		struct st_value_hashtable_node * node = hashtable->nodes[i];
		while (node != NULL) {
			st_value_list_push_v1(list, node->value, false);
			node = node->next;
		}
	}

	return list;
}


__asm__(".symver st_value_integer_get_v1, st_value_integer_get@@LIBSTONE_1.2");
long long int st_value_integer_get_v1(const struct st_value * value) {
	long long int * integer = st_value_get_v1(value);
	return *integer;
}


__asm__(".symver st_value_list_clear_v1, st_value_list_clear@@LIBSTONE_1.2");
void st_value_list_clear_v1(struct st_value * list) {
	if (list == NULL || (list->type != st_value_array && list->type != st_value_linked_list))
		return;

	if (list->type == st_value_array) {
		struct st_value_array * array = st_value_get_v1(list);
		unsigned int i;
		for (i = 0; i < array->nb_vals; i++) {
			st_value_free_v1(array->values[i]);
			array->values[i] = NULL;
		}
		array->nb_vals = 0;
	} else {
		struct st_value_linked_list * linked = st_value_get_v1(list);

		if (linked->nb_vals == 0)
			return;

		struct st_value_linked_list_node * ptr;
		for (ptr = linked->first->next; ptr != NULL; ptr = ptr->next) {
			st_value_free_v1(ptr->previous->value);
			free(ptr->previous);
		}
		if (linked->last != NULL) {
			st_value_free_v1(linked->last->value);
			free(linked->last);
		}
		linked->first = linked->last = NULL;
		linked->nb_vals = 0;
	}
}

__asm__(".symver st_value_list_get_v1, st_value_list_get@@LIBSTONE_1.2");
struct st_value * st_value_list_get_v1(struct st_value * list, unsigned int index, bool shared) {
	struct st_value * ret = &null_value;

	if (list == NULL || (list->type != st_value_array && list->type != st_value_linked_list))
		return ret;

	if (list->type == st_value_array) {
		struct st_value_array * array = st_value_get_v1(list);
		if (index < array->nb_vals)
			ret = array->values[index];
	} else {
		struct st_value_linked_list * linked_list = st_value_get_v1(list);
		if (linked_list->nb_vals >= index) {
			if (index == 0)
				ret = linked_list->first->value;
			else if (linked_list->nb_vals == index)
				ret = linked_list->last->value;
			else {
				struct st_value_linked_list_node * node = linked_list->first;
				while (index > 0) {
					node = node->next;
					index--;
				}
				ret = node->value;
			}
		}
	}

	if (shared)
		st_value_share_v1(ret);

	return ret;
}

__asm__(".symver st_value_list_get_iterator_v1, st_value_list_get_iterator@@LIBSTONE_1.2");
struct st_value_iterator * st_value_list_get_iterator_v1(struct st_value * list) {
	return st_value_list_get_iterator2(list, false);
}

static struct st_value_iterator * st_value_list_get_iterator2(struct st_value * list, bool weak_ref) {
	if (list == NULL || (list->type != st_value_array && list->type != st_value_linked_list))
		return NULL;

	if (!weak_ref)
		st_value_share_v1(list);

	struct st_value_iterator * iter = NULL;
	if (list->type == st_value_array) {
		iter = malloc(sizeof(struct st_value_iterator) + sizeof(struct st_value_iterator_array));
		struct st_value_iterator_array * iter_array = (struct st_value_iterator_array *) (iter + 1);

		iter_array->index = 0;
	} else {
		struct st_value_linked_list * value_linked_list = st_value_get_v1(list);

		iter = malloc(sizeof(struct st_value_iterator) + sizeof(struct st_value_iterator_linked_list));
		struct st_value_iterator_linked_list * iter_linked_list = (struct st_value_iterator_linked_list *) (iter + 1);

		iter_linked_list->current = NULL;
		iter_linked_list->next = value_linked_list->first;
	}

	iter->value = list;
	iter->weak_ref = weak_ref;

	return iter;
}

__asm__(".symver st_value_list_get_length_v1, st_value_list_get_length@@LIBSTONE_1.2");
unsigned int st_value_list_get_length_v1(struct st_value * list) {
	if (list == NULL || (list->type != st_value_array && list->type != st_value_linked_list))
		return 0;

	if (list->type == st_value_array) {
		struct st_value_array * array = st_value_get_v1(list);
		return array->nb_vals;
	} else {
		struct st_value_linked_list * linked = st_value_get_v1(list);
		return linked->nb_vals;
	}
}

__asm__(".symver st_value_list_index_of_v1, st_value_list_index_of@@LIBSTONE_1.2");
int st_value_list_index_of_v1(struct st_value * list, struct st_value * elt) {
	if (list == NULL || (list->type != st_value_array && list->type != st_value_linked_list))
		return -1;

	bool found = false;
	int index = -1;
	struct st_value_iterator * iter = st_value_list_get_iterator_v1(list);
	while (!found && st_value_iterator_has_next_v1(iter)) {
		struct st_value * i_elt = st_value_iterator_get_value_v1(iter, false);
		found = st_value_equals_v1(elt, i_elt);
		index++;
	}
	st_value_iterator_free_v1(iter);

	return found ? index : -1;
}

__asm__(".symver st_value_list_pop_v1, st_value_list_pop@@LIBSTONE_1.2");
struct st_value * st_value_list_pop_v1(struct st_value * list) {
	if (list == NULL || (list->type != st_value_array && list->type != st_value_linked_list))
		return NULL;

	struct st_value * ret = NULL;

	if (list->type == st_value_array) {
		struct st_value_array * array = st_value_get_v1(list);
		if (array->nb_vals > 0) {
			array->nb_vals--;
			ret = array->values[array->nb_vals];
			array->values[array->nb_vals] = NULL;
		}
	} else {
		struct st_value_linked_list * linked = st_value_get_v1(list);
		struct st_value_linked_list_node * node = linked->last;
		if (node != NULL) {
			linked->last = node->previous;
			if (linked->last == NULL)
				linked->first = NULL;

			ret = node->value;
			linked->nb_vals--;

			free(node);
		}
	}

	return ret;
}

__asm__(".symver st_value_list_push_v1, st_value_list_push@@LIBSTONE_1.2");
bool st_value_list_push_v1(struct st_value * list, struct st_value * val, bool new_val) {
	if (list == NULL || val == NULL || (list->type != st_value_array && list->type != st_value_linked_list))
		return false;

	if (list->type == st_value_array) {
		struct st_value_array * array = st_value_get_v1(list);
		if (array->nb_vals == array->nb_preallocated) {
			void * new_addr = realloc(array->values, (array->nb_vals + 16) * sizeof(struct st_value *));
			if (new_addr == NULL)
				return false;

			array->values = new_addr;
			array->nb_preallocated += 16;
		}

		if (!new_val)
			val = st_value_share_v1(val);

		array->values[array->nb_vals] = val;
		array->nb_vals++;

		return true;
	} else {
		struct st_value_linked_list * linked = st_value_get_v1(list);
		struct st_value_linked_list_node * node = malloc(sizeof(struct st_value_linked_list_node));
		if (node == NULL)
			return false;

		if (!new_val)
			val = st_value_share_v1(val);

		node->value = val;
		node->next = NULL;
		node->previous = linked->last;

		if (linked->first == NULL)
			linked->first = linked->last = node;
		else
			linked->last = linked->last->next = node;

		linked->nb_vals++;

		return true;
	}
}

__asm__(".symver st_value_list_shift_v1, st_value_list_shift@@LIBSTONE_1.2");
bool st_value_list_shift_v1(struct st_value * list, struct st_value * val, bool new_val) {
	if (list == NULL || val == NULL || (list->type != st_value_array && list->type != st_value_linked_list))
		return false;

	if (list->type == st_value_array) {
		struct st_value_array * array = st_value_get_v1(list);
		if (array->nb_vals == array->nb_preallocated) {
			void * new_addr = realloc(array->values, (array->nb_vals + 16) * sizeof(struct st_value *));
			if (new_addr == NULL)
				return false;

			array->values = new_addr;
			array->nb_preallocated += 16;
		}

		if (!new_val)
			val = st_value_share_v1(val);

		memmove(array->values + 1, array->values, array->nb_vals * sizeof(struct st_value *));
		array->values[0] = val;
		array->nb_vals++;

		return true;
	} else {
		struct st_value_linked_list_node * node = malloc(sizeof(struct st_value_linked_list_node));
		if (node == NULL)
			return false;

		if (!new_val)
			val = st_value_share_v1(val);

		struct st_value_linked_list * linked = st_value_get_v1(list);
		node->value = val;
		node->next = linked->first;
		node->previous = NULL;

		if (linked->first == NULL)
			linked->first = linked->last = node;
		else
			linked->first = linked->first->previous = node;

		linked->nb_vals++;

		return true;
	}
}

__asm__(".symver st_value_list_slice_v1, st_value_list_slice@@LIBSTONE_1.2");
struct st_value * st_value_list_slice_v1(struct st_value * list, int index) {
	if (list == NULL || (list->type != st_value_array && list->type != st_value_linked_list))
		return st_value_new_linked_list_v1();

	int length = st_value_list_get_length_v1(list);
	struct st_value * ret = st_value_new_array_v1(length);

	if (index < 0) {
		index += length;
		if (index < 0)
			index = 0;
	} else if (index >= length)
		return ret;

	struct st_value_iterator * iter = st_value_list_get_iterator_v1(list);
	while (index > 0 && st_value_iterator_has_next_v1(iter)) {
		st_value_iterator_get_value_v1(iter, false);
		index--;
	}

	if (index == 0)
		while (st_value_iterator_has_next(iter))
			st_value_list_push_v1(ret, st_value_iterator_get_value_v1(iter, true), true);

	st_value_iterator_free_v1(iter);

	return ret;
}

__asm__(".symver st_value_list_slice2_v1, st_value_list_slice2@@LIBSTONE_1.2");
struct st_value * st_value_list_slice2_v1(struct st_value * list, int index, int end) {
	if (list == NULL || (list->type != st_value_array && list->type != st_value_linked_list))
		return st_value_new_linked_list_v1();

	int length = st_value_list_get_length_v1(list);
	struct st_value * ret = st_value_new_array_v1(length);

	if (index < 0) {
		index += length;
		if (index < 0)
			index = 0;
	} else if (index >= length)
		return ret;

	if (end < 0) {
		end += length;
		if (end < 0)
			end = 0;
	} else if (end >= length)
		return ret;

	if (index >= end)
		return ret;

	struct st_value_iterator * iter = st_value_list_get_iterator_v1(list);
	while (index > 0 && st_value_iterator_has_next_v1(iter)) {
		st_value_iterator_get_value_v1(iter, false);
		index--;
		end--;
	}

	if (index == 0)
		while (end > 0 && st_value_iterator_has_next(iter)) {
			st_value_list_push_v1(ret, st_value_iterator_get_value_v1(iter, true), true);
			end--;
		}

	st_value_iterator_free_v1(iter);

	return ret;
}

__asm__(".symver st_value_list_splice_v1, st_value_list_splice@@LIBSTONE_1.2");
struct st_value * st_value_list_splice_v1(struct st_value * list, int index, int how_many, ...) {
	if (list == NULL || (list->type != st_value_array && list->type != st_value_linked_list))
		return st_value_new_linked_list_v1();

	int length = st_value_list_get_length_v1(list);
	struct st_value * ret = st_value_new_array_v1(length);

	if (index < 0) {
		index += length;
		if (index < 0)
			index = 0;
	} else if (index >= length)
		index = length;

	if (list->type == st_value_array) {
		struct st_value_array * array = st_value_get_v1(list);
		struct st_value * tmp_elts = st_value_new_linked_list_v1();

		if (index < length && how_many > 0) {
			int i;
			for (i = index; i < index + how_many && i < length; i++) {
				st_value_list_push_v1(ret, array->values[i], false);
				array->values[i] = NULL;
				array->nb_vals--;
			}

			for (; i < length; i++) {
				st_value_list_push_v1(tmp_elts, array->values[i], false);
				array->values[i] = NULL;
				array->nb_vals--;
			}
		}

		va_list args;
		va_start(args, how_many);

		struct st_value * new_val = va_arg(args, struct st_value *);
		while (new_val != NULL) {
			st_value_list_push_v1(list, new_val, false);
			new_val = va_arg(args, struct st_value *);
		}

		va_end(args);

		struct st_value_iterator * iter = st_value_list_get_iterator_v1(tmp_elts);
		while (st_value_iterator_has_next_v1(iter)) {
			struct st_value * elt = st_value_iterator_get_value_v1(iter, false);
			st_value_list_push_v1(list, elt, false);
		}
		st_value_iterator_free_v1(iter);
		st_value_free_v1(tmp_elts);
	} else {
		struct st_value_linked_list * llist = st_value_get_v1(list);
		struct st_value_linked_list_node * node = llist->first;

		while (node != NULL && index > 0) {
			node = node->next;
			index--;
		}

		while (node != NULL && how_many > 0) {
			st_value_list_push_v1(ret, node->value, true);
			node->value = NULL;

			if (node == llist->first) {
				llist->first = node->next;
				if (llist->first != NULL)
					llist->first->previous = NULL;
			}
			if (node == llist->last) {
				llist->last = node->previous;
				if (llist->last != NULL)
					llist->last->next = NULL;
			}
			llist->nb_vals--;

			free(node);
			how_many--;
		}

		va_list args;
		va_start(args, how_many);

		struct st_value * new_val = va_arg(args, struct st_value *);
		while (new_val != NULL) {
			if (node == NULL || node->next == NULL)
				st_value_list_push(list, new_val, false);
			else {
				struct st_value_linked_list_node * new_node = malloc(sizeof(struct st_value_linked_list_node));
				new_node->value = st_value_share_v1(new_val);
				new_node->next = node;
				new_node->previous = node->previous;

				if (new_node->previous != NULL)
					new_node->previous->next = new_node;
				else
					llist->first = new_node;
				node->previous = new_node;

				llist->nb_vals++;
			}

			new_val = va_arg(args, struct st_value *);
		}

		va_end(args);
	}

	return ret;
}

__asm__(".symver st_value_list_unshift_v1, st_value_list_unshift@@LIBSTONE_1.2");
struct st_value * st_value_list_unshift_v1(struct st_value * list) {
	if (list == NULL || (list->type != st_value_array && list->type != st_value_linked_list))
		return NULL;

	struct st_value * ret = NULL;

	if (list->type == st_value_array) {
		struct st_value_array * array = st_value_get_v1(list);
		if (array->nb_vals > 0) {
			ret = array->values[0];
			array->nb_vals--;
			memmove(array->values, array->values + 1, array->nb_vals * sizeof(struct st_value *));
			array->values[array->nb_vals] = NULL;
		}
	} else {
		struct st_value_linked_list * linked = st_value_get_v1(list);
		struct st_value_linked_list_node * node = linked->last;
		if (node != NULL) {
			linked->last = node->previous;
			if (linked->last == NULL)
				linked->first = NULL;

			ret = node->value;
			linked->nb_vals--;

			free(node);
		}
	}

	return ret;
}


__asm__(".symver st_value_string_get_v1, st_value_string_get@@LIBSTONE_1.2");
const char * st_value_string_get_v1(const struct st_value * value) {
	return st_value_get_v1(value);
}


__asm__(".symver st_value_iterator_free_v1, st_value_iterator_free@@LIBSTONE_1.2");
void st_value_iterator_free_v1(struct st_value_iterator * iter) {
	if (iter == NULL)
		return;

	if (iter->value != NULL && !iter->weak_ref)
		st_value_free_v1(iter->value);
	iter->value = NULL;

	free(iter);
}

__asm__(".symver st_value_iterator_get_key_v1, st_value_iterator_get_key@@LIBSTONE_1.2");
struct st_value * st_value_iterator_get_key_v1(struct st_value_iterator * iter, bool move_to_next, bool shared) {
	if (iter == NULL || iter->value == NULL || iter->value->type != st_value_hashtable)
		return &null_value;

	struct st_value_iterator_hashtable * iter_hash = (struct st_value_iterator_hashtable *) (iter + 1);
	struct st_value_hashtable_node * node = iter_hash->node;
	struct st_value * key = node->key;

	if (shared)
		key = st_value_share_v1(key);

	if (move_to_next) {
		struct st_value_hashtable * hash = st_value_get_v1(iter->value);
		iter_hash->node = node->next;

		if (iter_hash->node == NULL)
			while (iter_hash->i_elements <= hash->size_node && iter_hash->node == NULL)
				iter_hash->node = hash->nodes[iter_hash->i_elements++];
	}

	return key;
}

__asm__(".symver st_value_iterator_get_value_v1, st_value_iterator_get_value@@LIBSTONE_1.2");
struct st_value * st_value_iterator_get_value_v1(struct st_value_iterator * iter, bool shared) {
	if (iter == NULL || iter->value == NULL)
		return &null_value;

	struct st_value * ret = &null_value;
	switch (iter->value->type) {
		case st_value_array: {
				struct st_value_array * array = st_value_get_v1(iter->value);
				struct st_value_iterator_array * iter_array = (struct st_value_iterator_array *) (iter + 1);

				if (iter_array->index >= array->nb_vals)
					break;

				ret = array->values[iter_array->index];
				iter_array->index++;
				break;
			}

		case st_value_hashtable: {
				struct st_value_iterator_hashtable * iter_hash = (struct st_value_iterator_hashtable *) (iter + 1);

				if (iter_hash->node == NULL)
					break;

				struct st_value_hashtable * hash = st_value_get_v1(iter->value);
				struct st_value_hashtable_node * node = iter_hash->node;
				ret = node->value;

				iter_hash->node = node->next;
				if (iter_hash->node != NULL)
					break;

				while (iter_hash->i_elements <= hash->size_node && iter_hash->node == NULL)
					iter_hash->node = hash->nodes[iter_hash->i_elements++];
			}
			break;

		case st_value_linked_list: {
				struct st_value_iterator_linked_list * iter_list = (struct st_value_iterator_linked_list *) (iter + 1);
				iter_list->current = iter_list->next;
				if (iter_list->current != NULL) {
					ret = iter_list->current->value;
					iter_list->next = iter_list->next->next;
				}
			}
			break;

		default:
			return &null_value;
	}

	if (ret != &null_value && shared)
		ret = st_value_share_v1(ret);

	return ret;
}

__asm__(".symver st_value_iterator_has_next_v1, st_value_iterator_has_next@@LIBSTONE_1.2");
bool st_value_iterator_has_next_v1(struct st_value_iterator * iter) {
	if (iter == NULL)
		return false;

	switch (iter->value->type) {
		case st_value_array: {
				struct st_value_array * array = st_value_get_v1(iter->value);
				struct st_value_iterator_array * iter_array = (struct st_value_iterator_array *) (iter + 1);

				return iter_array->index < array->nb_vals;
			}

		case st_value_hashtable: {
				struct st_value_iterator_hashtable * iter_hash = (struct st_value_iterator_hashtable *) (iter + 1);
				return iter_hash->node != NULL;
			}

		case st_value_linked_list: {
				struct st_value_iterator_linked_list * iter_list = (struct st_value_iterator_linked_list *) (iter + 1);
				if (iter_list->next != NULL)
					return true;
				if (iter_list->current == NULL)
					return false;

				/**
				 * \note Special case, curent->next != NULL && next = NULL
				 * This case occure when we fetch the last element, then we push new
				 * value to the list and check if iterator has next element
				 */
				return iter_list->current->next != NULL;
			}

		default:
			return false;
	}
}


static unsigned long long st_value_compute_addr(const struct st_value * key) {
	return (unsigned long long int) key;
}

