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
*  Copyright (C) 2013-2015, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// va_end, va_start
#include <stdarg.h>
// calloc, free, malloc
#include <stdlib.h>
// memmove, strcpy, strlen, strdup
#include <string.h>
// bzero
#include <strings.h>

#include <libstoriqone/string.h>
#include <libstoriqone/value.h>

struct so_value_array {
	struct so_value ** values;
	unsigned int nb_vals, nb_preallocated;
};

struct so_value_custom {
	void * data;
	/**
	 * \brief Function used to release custom value
	 */
	so_value_free_f release;
};

struct so_value_hashtable {
	struct so_value_hashtable_node {
		unsigned long long hash;
		struct so_value * key;
		struct so_value * value;
		struct so_value_hashtable_node * next;
	} ** nodes;
	unsigned int nb_elements;
	unsigned int size_node;

	bool allow_rehash;

	so_value_hashtable_compupte_hash_f compute_hash;
};

struct so_value_linked_list {
	struct so_value_linked_list_node {
		struct so_value * value;
		struct so_value_linked_list_node * next;
		struct so_value_linked_list_node * previous;
	} * first, * last;
	unsigned int nb_vals;
};

struct so_value_iterator_array {
	unsigned int index;
};

struct so_value_iterator_hashtable {
	struct {
		struct so_value_hashtable_node * node;
		unsigned int i_elements;
	} next, previous;
};

struct so_value_iterator_linked_list {
	struct so_value_linked_list * linked_list;
	bool previous_detached;
	struct so_value_linked_list_node * previous;
	struct so_value_linked_list_node * next;
};

struct so_value_count {
	unsigned int nb_refs;

	struct so_value_count_list {
		struct so_value * elt;
		struct so_value_count_list * next;
	} * first;
};

static bool so_value_count_ref(struct so_value * ref);
static void so_value_count_ref2(struct so_value * ref, struct so_value * elt);
static struct so_value * so_value_new(enum so_value_type type, size_t extra);
static struct so_value * so_value_pack_inner(const char ** format, va_list params);
static int so_value_unpack_inner(struct so_value * root, const char ** format, va_list params);
static bool so_value_valid_inner(struct so_value * value, const char ** format, va_list params);

static struct so_value_iterator * so_value_hashtable_get_iterator2(struct so_value * hash, bool weak_ref) __attribute__((warn_unused_result));
static void so_value_hashtable_put_inner(struct so_value_hashtable * hash, unsigned int index, struct so_value_hashtable_node * new_node);
static void so_value_hashtable_rehash(struct so_value_hashtable * hash);
static void so_value_hashtable_release_node(struct so_value_hashtable_node * node);

static struct so_value_iterator * so_value_list_get_iterator2(struct so_value * list, bool weak_ref) __attribute__((warn_unused_result));

static struct so_value null_value = {
	.type = so_value_null,
	.shared = 0,
};


bool so_value_can_convert(struct so_value * val, enum so_value_type type) {
	if (val == NULL)
		return type == so_value_null;

	switch (val->type) {
		case so_value_array:
			switch (type) {
				case so_value_array:
				case so_value_linked_list:
					return true;

				default:
					return false;
			}

		case so_value_boolean:
			switch (type) {
				case so_value_boolean:
				case so_value_integer:
					return true;

				default:
					return false;
			}

		case so_value_custom:
			switch (type) {
				case so_value_custom:
					return true;

				default:
					return false;
			}

		case so_value_float:
			switch (type) {
				case so_value_integer:
				case so_value_float:
					return true;

				default:
					return false;
			}

		case so_value_hashtable:
			switch (type) {
				case so_value_hashtable:
					return true;

				default:
					return false;
			}

		case so_value_integer:
			switch (type) {
				case so_value_boolean:
				case so_value_integer:
				case so_value_float:
					return true;

				default:
					return false;
			}

		case so_value_linked_list:
			switch (type) {
				case so_value_array:
				case so_value_linked_list:
					return true;

				default:
					return false;
			}

		case so_value_null:
			switch (type) {
				case so_value_null:
					return true;

				default:
					return false;
			}

		case so_value_string:
			switch (type) {
				case so_value_null:
					return false;

				default:
					return true;
			}
	}

	return false;
}

struct so_value * so_value_convert(struct so_value * val, enum so_value_type type) {
	if (val == NULL)
		return &null_value;

	switch (val->type) {
		case so_value_array:
			switch (type) {
				case so_value_array:
					return so_value_share(val);

				case so_value_linked_list: {
						struct so_value * ret = so_value_new_linked_list();
						struct so_value_iterator * iter = so_value_list_get_iterator(val);
						while (so_value_iterator_has_next(iter)) {
							struct so_value * elt = so_value_iterator_get_value(iter, true);
							so_value_list_push(ret, elt, true);
						}
						so_value_iterator_free(iter);
						return ret;
					}

				default:
					return &null_value;
			}

		case so_value_boolean:
			switch (type) {
				case so_value_boolean:
					return so_value_share(val);

				case so_value_integer:
					return so_value_new_integer(so_value_boolean_get(val));

				default:
					return &null_value;
			}

		case so_value_custom:
			switch (type) {
				case so_value_custom:
					return so_value_share(val);

				default:
					return &null_value;
			}

		case so_value_float:
			switch (type) {
				case so_value_integer:
					return so_value_new_integer(so_value_float_get(val));

				case so_value_float:
					return so_value_share(val);

				default:
					return &null_value;
			}

		case so_value_hashtable:
			switch (type) {
				case so_value_hashtable:
					return so_value_share(val);

				default:
					return &null_value;
			}

		case so_value_integer:
			switch (type) {
				case so_value_boolean:
					return so_value_new_boolean(so_value_integer_get(val) != 0);

				case so_value_integer:
					return so_value_share(val);

				case so_value_float:
					return so_value_new_float(so_value_integer_get(val));

				default:
					return &null_value;
			}

		case so_value_linked_list:
			switch (type) {
				case so_value_array: {
					struct so_value * ret = so_value_new_array(so_value_list_get_length(val));
					struct so_value_iterator * iter = so_value_list_get_iterator(val);
					while (so_value_iterator_has_next(iter)) {
						struct so_value * elt = so_value_iterator_get_value(iter, true);
						so_value_list_push(ret, elt, true);
					}
					so_value_iterator_free(iter);
					return ret;
				}

				case so_value_linked_list:
					return so_value_share(val);

				default:
					return &null_value;
			}

		case so_value_null:
			return &null_value;

		case so_value_string:
			switch (type) {
				case so_value_string:
					return so_value_share(val);

				default:
					return &null_value;
			}
	}

	return &null_value;
}

struct so_value * so_value_copy(struct so_value * val, bool deep_copy) {
	struct so_value * ret = &null_value;

	if (val == NULL)
		return ret;

	switch (val->type) {
		case so_value_array: {
				ret = so_value_new_array(so_value_list_get_length(val));

				struct so_value_iterator * iter = so_value_list_get_iterator(val);
				while (so_value_iterator_has_next(iter)) {
					struct so_value * elt = so_value_iterator_get_value(iter, !deep_copy);
					if (deep_copy)
						elt = so_value_copy(elt, true);
					so_value_list_push(ret, val, false);
				}
				so_value_iterator_free(iter);
			}
			break;

		case so_value_boolean:
			ret = so_value_new_boolean(so_value_boolean_get(val));
			break;

		case so_value_custom:
			ret = so_value_share(val);
			break;

		case so_value_float:
			ret = so_value_new_float(so_value_float_get(val));
			break;

		case so_value_hashtable: {
				struct so_value_hashtable * hashtable = (struct so_value_hashtable *) (val + 1);
				ret = so_value_new_hashtable(hashtable->compute_hash);

				struct so_value_iterator * iter = so_value_hashtable_get_iterator(val);
				while (so_value_iterator_has_next(iter)) {
					struct so_value * key = so_value_iterator_get_key(iter, false, false);
					struct so_value * value = so_value_iterator_get_value(iter, false);

					if (deep_copy) {
						key = so_value_copy(key, true);
						value = so_value_copy(value, true);
					} else {
						key = so_value_share(key);
						value = so_value_share(value);
					}

					so_value_hashtable_put(ret, key, true, value, true);
				}
				so_value_iterator_free(iter);
			}
			break;

		case so_value_integer:
			ret = so_value_new_integer(so_value_integer_get(val));
			break;

		case so_value_null:
			break;

		case so_value_linked_list: {
				ret = so_value_new_linked_list();

				struct so_value_iterator * iter = so_value_list_get_iterator(val);
				while (so_value_iterator_has_next(iter)) {
					struct so_value * elt = so_value_iterator_get_value(iter, !deep_copy);
					if (deep_copy)
						elt = so_value_copy(elt, true);
					so_value_list_push(ret, val, false);
				}
				so_value_iterator_free(iter);
			}
			break;

		case so_value_string:
			ret = so_value_new_string(so_value_string_get(val));
			break;
	}

	return ret;
}

static bool so_value_count_ref(struct so_value * ref) {
	struct so_value_count count_ref = { 1, NULL };
	ref->data = &count_ref;

	switch (ref->type) {
		case so_value_array:
		case so_value_linked_list: {
				struct so_value_iterator * iter = so_value_list_get_iterator2(ref, true);
				while (so_value_iterator_has_next(iter)) {
					struct so_value * elt = so_value_iterator_get_value(iter, false);
					so_value_count_ref2(ref, elt);
				}
				so_value_iterator_free(iter);

				break;
			}

		case so_value_hashtable: {
				struct so_value_iterator * iter = so_value_hashtable_get_iterator2(ref, true);
				while (so_value_iterator_has_next(iter)) {
					struct so_value * key = so_value_iterator_get_key(iter, false, false);
					so_value_count_ref2(ref, key);

					struct so_value * elt = so_value_iterator_get_value(iter, false);
					so_value_count_ref2(ref, elt);
				}
				so_value_iterator_free(iter);

				break;
			}

		default:
			break;
	}

	while (count_ref.first != NULL) {
		struct so_value_count_list * ptr = count_ref.first;
		count_ref.first = ptr->next;

		free(ptr->elt->data);
		ptr->elt->data = NULL;

		free(ptr);
	}

	ref->data = NULL;
	return count_ref.nb_refs == ref->shared;
}

static void so_value_count_ref2(struct so_value * ref, struct so_value * elt) {
	if (elt->shared == 0)
		return;

	if (elt->data != NULL) {
		struct so_value_count * count_ref = elt->data;
		count_ref->nb_refs++;
		return;
	}

	struct so_value_count count_elt = { 1, NULL };
	if (elt->shared > 1) {
		struct so_value_count * count_elt = elt->data = malloc(sizeof(struct so_value_count));
		count_elt->nb_refs = 1;
		count_elt->first = NULL;

		struct so_value_count * count_ref = ref->data;
		struct so_value_count_list * list_ref = malloc(sizeof(struct so_value_count_list));
		list_ref->elt = elt;
		list_ref->next = count_ref->first;
		count_ref->first = list_ref;
	} else
		elt->data = &count_elt;

	switch (elt->type) {
		case so_value_array:
		case so_value_linked_list: {
				struct so_value_iterator * iter = so_value_list_get_iterator2(elt, true);
				while (so_value_iterator_has_next(iter)) {
					struct so_value * lst_elt = so_value_iterator_get_value(iter, false);
					so_value_count_ref2(ref, lst_elt);
				}
				so_value_iterator_free(iter);
				break;
			}

		case so_value_hashtable: {
				struct so_value_iterator * iter = so_value_hashtable_get_iterator2(elt, true);
				while (so_value_iterator_has_next(iter)) {
					struct so_value * key = so_value_iterator_get_key(iter, false, false);
					so_value_count_ref2(ref, key);

					struct so_value * hsh_elt = so_value_iterator_get_value(iter, false);
					so_value_count_ref2(ref, hsh_elt);
				}
				so_value_iterator_free(iter);
				break;
			}

		default:
			break;
	}

	if (elt->shared == 1)
		elt->data = NULL;
}

bool so_value_equals(struct so_value * a, struct so_value * b) {
	if (a == NULL && b == NULL)
		return true;

	if (a == NULL || b == NULL)
		return false;

	switch (a->type) {
		case so_value_array:
		case so_value_linked_list:
			switch (b->type) {
				case so_value_array:
				case so_value_linked_list: {
						struct so_value_iterator * iter_a = so_value_list_get_iterator(a);
						struct so_value_iterator * iter_b = so_value_list_get_iterator(b);

						bool equals = true;
						while (equals && so_value_iterator_has_next(iter_a) && so_value_iterator_has_next(iter_b)) {
							struct so_value * v_a = so_value_iterator_get_value(iter_a, false);
							struct so_value * v_b = so_value_iterator_get_value(iter_b, false);

							equals = so_value_equals(v_a, v_b);
						}

						if (equals)
							equals = !so_value_iterator_has_next(iter_a) && !so_value_iterator_has_next(iter_b);

						so_value_iterator_free(iter_a);
						so_value_iterator_free(iter_b);

						return equals;
					}

				default:
					return false;
			}

		case so_value_boolean:
			switch (b->type) {
				case so_value_boolean:
					return false;

				case so_value_float:
					return (so_value_boolean_get(a) && so_value_float_get(b) != 0) || (!so_value_boolean_get(a) && so_value_float_get(b) == 0);

				case so_value_integer:
					return (so_value_boolean_get(a) && so_value_integer_get(b) != 0) || (!so_value_boolean_get(a) && so_value_integer_get(b) == 0);

				default:
					return false;
			}

		case so_value_custom:
			if (b->type == so_value_custom) {
				struct so_value_custom * ca = so_value_get(a);
				struct so_value_custom * cb = so_value_get(b);
				return ca->data == cb->data;
			}
			return false;

		case so_value_float:
			switch (b->type) {
				case so_value_boolean:
					return (so_value_float_get(a) != 0 && so_value_boolean_get(b)) || (so_value_float_get(a) == 0 && !so_value_boolean_get(b));

				case so_value_float:
					return so_value_float_get(a) == so_value_float_get(b);

				case so_value_integer:
					return so_value_float_get(a) == so_value_integer_get(b);

				default:
					return false;
			}

		case so_value_hashtable:
			if (b->type == so_value_hashtable) {
				struct so_value_hashtable * ha = so_value_get(a);
				struct so_value_hashtable * hb = so_value_get(b);

				if (ha->nb_elements != hb->nb_elements)
					return false;

				struct so_value_iterator * iter = so_value_hashtable_get_iterator(a);
				bool equals = true;
				while (equals && so_value_iterator_has_next(iter)) {
					struct so_value * key = so_value_iterator_get_key(iter, true, false);

					if (!so_value_hashtable_has_key(b, key)) {
						equals = false;
						break;
					}

					struct so_value * val_a = so_value_hashtable_get(a, key, false, false);
					struct so_value * val_b = so_value_hashtable_get(b, key, false, false);
					equals = so_value_equals(val_a, val_b);
				}
				so_value_iterator_free(iter);

				return equals;
			}
			return false;

		case so_value_integer:
			switch (b->type) {
				case so_value_boolean:
					return (so_value_integer_get(a) != 0 && so_value_boolean_get(b)) || (so_value_integer_get(a) == 0 && !so_value_boolean_get(b));

				case so_value_float:
					return so_value_integer_get(a) == so_value_float_get(b);

				case so_value_integer:
					return so_value_integer_get(a) == so_value_integer_get(b);

				default:
					return false;
			}

		case so_value_null:
			return b->type == so_value_null;

		case so_value_string:
			switch (b->type) {
				case so_value_string: {
						const char * sa = so_value_string_get(a);
						const char * sb = so_value_string_get(b);
						return !strcmp(sa, sb);
					}

				default:
					return false;
			}
	}

	return false;
}

void so_value_free(struct so_value * value) {
	if (value == NULL || value == &null_value || value->shared == 0)
		return;

	if (value->shared > 1 && !so_value_count_ref(value)) {
		value->shared--;
		return;
	}

	value->shared = 0;

	switch (value->type) {
		case so_value_array: {
				so_value_list_clear(value);
				struct so_value_array * array = so_value_get(value);
				free(array->values);
			}
			break;

		case so_value_custom: {
				struct so_value_custom * custom = so_value_get(value);
				if (custom->release != NULL)
					custom->release(custom->data);
			}
			break;

		case so_value_hashtable: {
				so_value_hashtable_clear(value);
				struct so_value_hashtable * hashtable = so_value_get(value);
				free(hashtable->nodes);
			}
			break;

		case so_value_linked_list:
			so_value_list_clear(value);
			break;

		default:
			break;
	}
	free(value);
}

void * so_value_get(const struct so_value * value) {
	return (void *) (value + 1);
}

static struct so_value * so_value_new(enum so_value_type type, size_t extra) {
	struct so_value * val = malloc(sizeof(struct so_value) + extra);
	bzero(val, sizeof(struct so_value) + extra);

	val->type = type;
	val->shared = 1;
	return val;
}

struct so_value * so_value_new_array(unsigned int size) {
	struct so_value * val = so_value_new(so_value_array, sizeof(struct so_value_array));
	if (size == 0)
		size = 16;
	struct so_value_array * array = so_value_get(val);
	array->values = calloc(sizeof(struct so_value *), size);
	array->nb_vals = 0;
	array->nb_preallocated = size;
	return val;
}

struct so_value * so_value_new_boolean(bool value) {
	struct so_value * val = so_value_new(so_value_boolean, sizeof(bool));
	bool * boolean = so_value_get(val);
	*boolean = value;
	return val;
}

struct so_value * so_value_new_custom(void * value, so_value_free_f release) {
	struct so_value * val = so_value_new(so_value_custom, sizeof(struct so_value_custom));
	struct so_value_custom * custom = so_value_get(val);
	custom->data = value;
	custom->release = release;
	return val;
}

struct so_value * so_value_new_float(double value) {
	struct so_value * val = so_value_new(so_value_float, sizeof(double));
	double * floating = so_value_get(val);
	*floating = value;
	return val;
}

struct so_value * so_value_new_hashtable(so_value_hashtable_compupte_hash_f compute_hash) {
	struct so_value * val = so_value_new(so_value_hashtable, sizeof(struct so_value_hashtable));
	struct so_value_hashtable * hashtable = so_value_get(val);
	hashtable->nodes = calloc(16, sizeof(struct so_value_hashtable_node *));
	hashtable->nb_elements = 0;
	hashtable->size_node = 16;
	hashtable->allow_rehash = true;
	hashtable->compute_hash = compute_hash;
	return val;
}

struct so_value * so_value_new_hashtable2() {
	return so_value_new_hashtable(so_string_compute_hash);
}

struct so_value * so_value_new_integer(long long int value) {
	struct so_value * val = so_value_new(so_value_integer, sizeof(long long int));
	long long int * integer = so_value_get(val);
	*integer = value;
	return val;
}

struct so_value * so_value_new_linked_list() {
	struct so_value * val = so_value_new(so_value_linked_list, sizeof(struct so_value_linked_list));
	struct so_value_linked_list * list = so_value_get(val);
	list->first = list->last = NULL;
	list->nb_vals = 0;
	return val;
}

struct so_value * so_value_new_null() {
	return &null_value;
}

struct so_value * so_value_new_string(const char * value) {
	struct so_value * val = so_value_new(so_value_string, strlen(value) + 1);
	char * string = so_value_get(val);
	strcpy(string, value);
	return val;
}

struct so_value * so_value_new_string2(const char * value, ssize_t length) {
	struct so_value * val = so_value_new(so_value_string, length + 1);
	char * string = so_value_get(val);
	strncpy(string, value, length);
	string[length] = '\0';
	return val;
}

struct so_value * so_value_pack(const char * format, ...) {
	if (format == NULL)
		return NULL;

	va_list va;
	va_start(va, format);

	struct so_value * new_value = so_value_pack_inner(&format, va);

	va_end(va);

	return new_value;
}

static struct so_value * so_value_pack_inner(const char ** format, va_list params) {
	switch (**format) {
		case 'b':
			return so_value_new_boolean(va_arg(params, int));

		case 'f':
			return so_value_new_float(va_arg(params, double));

		case 'i':
			return so_value_new_integer(va_arg(params, int));

		case 'I':
			return so_value_new_integer(va_arg(params, long long int));

		case 'n':
			return &null_value;

		case 'o': {
				struct so_value * value = va_arg(params, struct so_value *);
				if (value != NULL)
					return value;
				return &null_value;
			}

		case 'O': {
				struct so_value * value = va_arg(params, struct so_value *);
				if (value != NULL)
					return so_value_share(value);
				return &null_value;
			}

		case 's': {
				const char * str_val = va_arg(params, const char *);
				if (str_val != NULL)
					return so_value_new_string(str_val);
				return &null_value;
			}

		case 'u':
			return so_value_new_integer(va_arg(params, unsigned int));

		case 'U':
			return so_value_new_integer(va_arg(params, unsigned long long int));

		case 'z':
			return so_value_new_integer(va_arg(params, size_t));

		case '[': {
				struct so_value * array = so_value_new_linked_list();
				(*format)++;

				while (**format != ']') {
					struct so_value * elt = so_value_pack_inner(format, params);
					if (elt == NULL) {
						so_value_free(array);
						return NULL;
					}
					so_value_list_push(array, elt, true);
					(*format)++;
				}

				return array;
			}

		case '{': {
				struct so_value * object = so_value_new_hashtable2();
				(*format)++;

				while (**format != '}') {
					if (**format != 's') {
						so_value_free(object);
						return NULL;
					}

					struct so_value * key = so_value_pack_inner(format, params);
					(*format)++;

					struct so_value * value = so_value_pack_inner(format, params);
					if (value == NULL) {
						so_value_free(object);
						so_value_free(key);
						return NULL;
					}

					so_value_hashtable_put(object, key, true, value, true);
					(*format)++;
				}

				return object;
			}
	}

	return NULL;
}

struct so_value * so_value_share(struct so_value * value) {
	if (value == NULL)
		return NULL;

	if (value == &null_value)
		return value;

	value->shared++;
	return value;
}

int so_value_unpack(struct so_value * root, const char * format, ...) {
	if (root == NULL || format == NULL)
		return false;

	va_list va;
	va_start(va, format);

	int ret = so_value_unpack_inner(root, &format, va);

	va_end(va);

	return ret;
}

static int so_value_unpack_inner(struct so_value * value, const char ** format, va_list params) {
	if (value == NULL)
		return 0;

	char f = **format;
	(*format)++;

	switch (f) {
		case 'b':
			if (value->type == so_value_boolean) {
				bool * val = va_arg(params, bool *);
				if (val != NULL) {
					*val = so_value_boolean_get(value);
					return 1;
				}
			}
			break;

		case 'f':
			if (value->type == so_value_float) {
				double * val = va_arg(params, double *);
				if (val != NULL) {
					*val = so_value_float_get(value);
					return 1;
				}
			} else if (value->type == so_value_integer) {
				double * val = va_arg(params, double *);
				if (val != NULL) {
					*val = so_value_integer_get(value);
					return 1;
				}
			}
			break;

		case 'i':
			if (value->type == so_value_integer) {
				int * val = va_arg(params, int *);
				if (val != NULL) {
					*val = so_value_integer_get(value);
					return 1;
				}
			}
			break;

		case 'I':
			if (value->type == so_value_integer) {
				long long int * val = va_arg(params, long long int *);
				if (val != NULL) {
					*val = so_value_integer_get(value);
					return 1;
				}
			}
			break;

		case 'n':
			return value->type == so_value_null;

		case 'o': {
				struct so_value ** val = va_arg(params, struct so_value **);
				if (val != NULL) {
					*val = value;
					return 1;
				}
			}
			break;

		case 'O': {
				struct so_value ** val = va_arg(params, struct so_value **);
				if (val != NULL) {
					*val = so_value_share(value);
					return 1;
				}
			}
			break;

		case 's':
			if (value->type == so_value_string) {
				char ** val = va_arg(params, char **);
				if (val != NULL) {
					*val = strdup(so_value_string_get(value));
					return 1;
				}
			} else if (value->type == so_value_null) {
				char ** val = va_arg(params, char **);
				if (val != NULL) {
					*val = NULL;
					return 1;
				}
			}
			break;

		case 'u':
			if (value->type == so_value_integer) {
				unsigned int * val = va_arg(params, unsigned int *);
				if (val != NULL) {
					*val = so_value_integer_get(value);
					return 1;
				}
			}
			break;

		case 'U':
			if (value->type == so_value_integer) {
				unsigned long long int * val = va_arg(params, unsigned long long int *);
				if (val != NULL) {
					*val = so_value_integer_get(value);
					return 1;
				}
			}
			break;

		case 'z':
			if (value->type == so_value_integer) {
				size_t * val = va_arg(params, size_t *);
				if (val != NULL) {
					*val = so_value_integer_get(value);
					return 1;
				}
			}
			break;

		case '[': {
				int ret = 0;
				struct so_value_iterator * iter = so_value_list_get_iterator(value);
				while (**format != ']') {
					if (!so_value_iterator_has_next(iter))
						break;

					struct so_value * elt = so_value_iterator_get_value(iter, false);
					ret += so_value_unpack_inner(elt, format, params);
				}
				so_value_iterator_free(iter);

				(*format)++;
				return ret;
			}

		case '{': {
				int ret = 0;
				while (**format != '}') {
					struct so_value * key = so_value_pack_inner(format, params);
					if (key == NULL || !so_value_hashtable_has_key(value, key)) {
						so_value_free(key);
						return ret;
					}

					(*format)++;

					struct so_value * child = so_value_hashtable_get(value, key, false, false);
					ret += so_value_unpack_inner(child, format, params);

					so_value_free(key);
				}

				(*format)++;

				return ret;
			}
	}

	return 0;
}

bool so_value_valid(struct so_value * value, const char * format, ...) {
	if (format == NULL)
		return NULL;

	va_list va;
	va_start(va, format);

	bool ok = so_value_valid_inner(value, &format, va);

	va_end(va);

	return ok;
}

static bool so_value_valid_inner(struct so_value * value, const char ** format, va_list params) {
	if (value == NULL)
		return false;

	switch (**format) {
		case 'b':
			return value->type == so_value_boolean;

		case 'f':
			return value->type == so_value_float;

		case 'i':
		case 'I':
		case 'u':
		case 'U':
		case 'z':
			return value->type == so_value_integer;

		case 'n':
			return value->type == so_value_null;

		case 'o':
		case 'O':
			return true;

		case 's':
			return value->type == so_value_string;

		case '[': {
				bool ok = true;
				struct so_value_iterator * iter = so_value_list_get_iterator(value);
				(*format)++;

				while (ok && **format != ']') {
					ok = so_value_iterator_has_next(iter);
					if (!ok)
						break;

					struct so_value * elt = so_value_iterator_get_value(iter, false);
					ok = so_value_valid_inner(elt, format, params);

					(*format)++;
				}
				so_value_iterator_free(iter);

				return ok;
			}

		case '{': {
				(*format)++;

				bool ok = true;
				while (ok && **format != '}') {
					struct so_value * key = so_value_pack_inner(format, params);
					if (key == NULL || !so_value_hashtable_has_key(value, key)) {
						so_value_free(key);
						return false;
					}

					(*format)++;

					struct so_value * child = so_value_hashtable_get(value, key, false, false);
					ok = so_value_valid_inner(child, format, params);

					so_value_free(key);

					(*format)++;
				}

				(*format)++;

				return ok;
			}
	}
	return false;
}

struct so_value * so_value_vpack(const char * format, va_list params) {
	if (format == NULL)
		return NULL;

	return so_value_pack_inner(&format, params);
}

int so_value_vunpack(struct so_value * root, const char * format, va_list params) {
	if (root == NULL || format == NULL)
		return false;

	return so_value_unpack_inner(root, &format, params);
}

bool so_value_vvalid(struct so_value * value, const char * format, va_list params) {
	if (format == NULL)
		return NULL;

	return so_value_valid_inner(value, &format, params);
}


bool so_value_boolean_get(const struct so_value * value) {
	bool * boolean = so_value_get(value);
	return *boolean;
}


unsigned long long so_value_custom_compute_hash(const struct so_value * value) {
	struct so_value_custom * custom = so_value_get(value);
	return (unsigned long) custom->data;
}

void * so_value_custom_get(const struct so_value * value) {
	struct so_value_custom * custom = so_value_get(value);
	return custom->data;
}


double so_value_float_get(const struct so_value * value) {
	double * floating = so_value_get(value);
	return *floating;
}


void so_value_hashtable_clear(struct so_value * hash) {
	if (hash == NULL || hash->type != so_value_hashtable)
		return;

	unsigned int i;
	struct so_value_hashtable * hashtable = so_value_get(hash);
	for (i = 0; i < hashtable->size_node; i++) {
		struct so_value_hashtable_node * node = hashtable->nodes[i];
		hashtable->nodes[i] = NULL;

		while (node != NULL) {
			struct so_value_hashtable_node * tmp = node;
			node = node->next;

			so_value_hashtable_release_node(tmp);
		}
	}

	hashtable->nb_elements = 0;
}

struct so_value * so_value_hashtable_get(struct so_value * hash, struct so_value * key, bool shared, bool detach) {
	if (hash == NULL || hash->type != so_value_hashtable || key == NULL)
		return &null_value;

	struct so_value_hashtable * hashtable = so_value_get(hash);
	unsigned long long h = hashtable->compute_hash(key);
	unsigned int index = h % hashtable->size_node;

	struct so_value_hashtable_node * node = hashtable->nodes[index];
	while (node != NULL && node->hash != h)
		node = node->next;

	struct so_value * ret = &null_value;
	if (node != NULL && node->hash == h) {
		if (shared || detach)
			ret = so_value_share(node->value);
		else
			ret = node->value;

		if (detach)
			so_value_hashtable_remove(hash, key);
	}

	return ret;
}

struct so_value * so_value_hashtable_get2(struct so_value * hash, const char * key, bool shared, bool detach) {
	struct so_value * k = so_value_new_string(key);
	struct so_value * returned = so_value_hashtable_get(hash, k, shared, detach);
	so_value_free(k);
	return returned;
}

struct so_value_iterator * so_value_hashtable_get_iterator(struct so_value * hash) {
	return so_value_hashtable_get_iterator2(hash, false);
}

static struct so_value_iterator * so_value_hashtable_get_iterator2(struct so_value * hash, bool weak_ref) {
	if (hash == NULL || hash->type != so_value_hashtable)
		return NULL;

	if (!weak_ref)
		hash = so_value_share(hash);

	struct so_value_hashtable * value_hash = so_value_get(hash);
	struct so_value_iterator * iter = malloc(sizeof(struct so_value_iterator) + sizeof(struct so_value_iterator_hashtable));
	struct so_value_iterator_hashtable * iter_hash = (struct so_value_iterator_hashtable *) (iter + 1);

	iter->value = hash;
	iter->weak_ref = weak_ref;

	iter_hash->next.node = iter_hash->previous.node = NULL;

	for (iter_hash->next.i_elements = 0; iter_hash->next.i_elements < value_hash->size_node && iter_hash->next.node == NULL; iter_hash->next.i_elements++)
		iter_hash->next.node = value_hash->nodes[iter_hash->next.i_elements];

	iter_hash->previous.i_elements = iter_hash->next.i_elements - 1;

	return iter;
}

unsigned int so_value_hashtable_get_length(struct so_value * hash) {
	if (hash == NULL || hash->type != so_value_hashtable)
		return 0;

	struct so_value_hashtable * hashtable = so_value_get(hash);
	return hashtable->nb_elements;
}

bool so_value_hashtable_has_key(struct so_value * hash, struct so_value * key) {
	if (hash == NULL || hash->type != so_value_hashtable || key == NULL)
		return false;

	struct so_value_hashtable * hashtable = so_value_get(hash);
	unsigned long long h = hashtable->compute_hash(key);
	unsigned int index = h % hashtable->size_node;

	struct so_value_hashtable_node * node = hashtable->nodes[index];
	while (node != NULL && node->hash != h)
		node = node->next;

	return node != NULL && node->hash == h;
}

bool so_value_hashtable_has_key2(struct so_value * hash, const char * key) {
	struct so_value * k = so_value_new_string(key);
	bool returned = so_value_hashtable_has_key(hash, k);
	so_value_free(k);
	return returned;
}

struct so_value * so_value_hashtable_keys(struct so_value * hash) {
	if (hash == NULL || hash->type != so_value_hashtable)
		return &null_value;

	struct so_value_hashtable * hashtable = so_value_get(hash);
	struct so_value * list = so_value_new_array(hashtable->nb_elements);
	unsigned int i;
	for (i = 0; i < hashtable->size_node; i++) {
		struct so_value_hashtable_node * node = hashtable->nodes[i];
		while (node != NULL) {
			so_value_list_push(list, node->key, false);
			node = node->next;
		}
	}

	return list;
}

void so_value_hashtable_put(struct so_value * hash, struct so_value * key, bool new_key, struct so_value * value, bool new_value) {
	if (hash == NULL || hash->type != so_value_hashtable || key == NULL || key->type == so_value_null || value == NULL)
		return;

	struct so_value_hashtable * hashtable = so_value_get(hash);
	unsigned long long h = hashtable->compute_hash(key);
	unsigned int index = h % hashtable->size_node;

	if (!new_key)
		key = so_value_share(key);
	if (!new_value)
		value = so_value_share(value);

	struct so_value_hashtable_node * node = malloc(sizeof(struct so_value_hashtable_node));
	node->hash = h;
	node->key = key;
	node->value = value;
	node->next = NULL;

	so_value_hashtable_put_inner(hashtable, index, node);
	hashtable->nb_elements++;
}

void so_value_hashtable_put2(struct so_value * hash, const char * key, struct so_value * value, bool new_value) {
	struct so_value * k = so_value_new_string(key);
	so_value_hashtable_put(hash, k, true, value, new_value);
}

static void so_value_hashtable_put_inner(struct so_value_hashtable * hashtable, unsigned int index, struct so_value_hashtable_node * new_node) {
	struct so_value_hashtable_node * node = hashtable->nodes[index];

	if (node != NULL) {
		if (node->hash == new_node->hash) {
			hashtable->nodes[index] = new_node;
			new_node->next = node->next;

			so_value_hashtable_release_node(node);
			hashtable->nb_elements--;
		} else {
			short nb_elt = 1;
			while (node->next != NULL) {
				if (node->next->hash == new_node->hash) {
					struct so_value_hashtable_node * old = node->next;
					node->next = new_node;
					new_node->next = old->next;

					so_value_hashtable_release_node(old);
					hashtable->nb_elements--;
					return;
				}

				node = node->next;
				nb_elt++;
			}

			node->next = new_node;
			if (nb_elt > 4)
				so_value_hashtable_rehash(hashtable);
		}
	} else
		hashtable->nodes[index] = new_node;
}

static void so_value_hashtable_rehash(struct so_value_hashtable * hashtable) {
	if (!hashtable->allow_rehash)
		return;

	hashtable->allow_rehash = false;

	struct so_value_hashtable_node ** old_nodes = hashtable->nodes;
	unsigned int old_size_nodes = hashtable->size_node;

	hashtable->size_node <<= 1;
	hashtable->nodes = calloc(hashtable->size_node, sizeof(struct so_value_hashtable_node *));

	unsigned int i;
	for (i = 0; i < old_size_nodes; i++) {
		struct so_value_hashtable_node * node = old_nodes[i];
		while (node != NULL) {
			struct so_value_hashtable_node * current_node = node;
			node = node->next;
			current_node->next = NULL;

			unsigned long long h = current_node->hash % hashtable->size_node;
			so_value_hashtable_put_inner(hashtable, h, current_node);
		}
	}

	free(old_nodes);
}

static void so_value_hashtable_release_node(struct so_value_hashtable_node * node) {
	so_value_free(node->key);
	so_value_free(node->value);
	free(node);
}

void so_value_hashtable_remove(struct so_value * hash, struct so_value * key) {
	if (hash == NULL || hash->type != so_value_hashtable || key == NULL)
		return;

	struct so_value_hashtable * hashtable = so_value_get(hash);
	unsigned long long h = hashtable->compute_hash(key);
	unsigned int index = h % hashtable->size_node;

	struct so_value_hashtable_node * node = hashtable->nodes[index];
	if (node == NULL)
		return;

	if (node->hash == h) {
		hashtable->nodes[index] = node->next;

		so_value_hashtable_release_node(node);
		hashtable->nb_elements--;

		return;
	}

	while (node->next != NULL && node->next->hash != h)
		node = node->next;

	if (node->next != NULL && node->next->hash == h) {
		struct so_value_hashtable_node * tmp = node->next;
		node->next = tmp->next;

		so_value_hashtable_release_node(tmp);
		hashtable->nb_elements--;
	}
}

void so_value_hashtable_remove2(struct so_value * hash, const char * key) {
	struct so_value * k = so_value_new_string(key);
	so_value_hashtable_remove(hash, k);
	so_value_free(k);
}

struct so_value * so_value_hashtable_values(struct so_value * hash) {
	if (hash == NULL || hash->type != so_value_hashtable)
		return &null_value;

	struct so_value_hashtable * hashtable = so_value_get(hash);
	struct so_value * list = so_value_new_array(hashtable->nb_elements);
	unsigned int i;
	for (i = 0; i < hashtable->size_node; i++) {
		struct so_value_hashtable_node * node = hashtable->nodes[i];
		while (node != NULL) {
			so_value_list_push(list, node->value, false);
			node = node->next;
		}
	}

	return list;
}


long long int so_value_integer_get(const struct so_value * value) {
	long long int * integer = so_value_get(value);
	return *integer;
}


void so_value_list_clear(struct so_value * list) {
	if (list == NULL || (list->type != so_value_array && list->type != so_value_linked_list))
		return;

	if (list->type == so_value_array) {
		struct so_value_array * array = so_value_get(list);
		unsigned int i;
		for (i = 0; i < array->nb_vals; i++) {
			so_value_free(array->values[i]);
			array->values[i] = NULL;
		}
		array->nb_vals = 0;
	} else {
		struct so_value_linked_list * linked = so_value_get(list);

		if (linked->nb_vals == 0)
			return;

		struct so_value_linked_list_node * ptr;
		for (ptr = linked->first->next; ptr != NULL; ptr = ptr->next) {
			so_value_free(ptr->previous->value);
			free(ptr->previous);
		}
		if (linked->last != NULL) {
			so_value_free(linked->last->value);
			free(linked->last);
		}
		linked->first = linked->last = NULL;
		linked->nb_vals = 0;
	}
}

struct so_value * so_value_list_get(struct so_value * list, unsigned int index, bool shared) {
	struct so_value * ret = &null_value;

	if (list == NULL || (list->type != so_value_array && list->type != so_value_linked_list))
		return ret;

	if (list->type == so_value_array) {
		struct so_value_array * array = so_value_get(list);
		if (index < array->nb_vals)
			ret = array->values[index];
	} else {
		struct so_value_linked_list * linked_list = so_value_get(list);
		if (linked_list->nb_vals >= index) {
			if (index == 0)
				ret = linked_list->first->value;
			else if (linked_list->nb_vals == index)
				ret = linked_list->last->value;
			else {
				struct so_value_linked_list_node * node = linked_list->first;
				while (index > 0) {
					node = node->next;
					index--;
				}
				ret = node->value;
			}
		}
	}

	if (shared)
		ret = so_value_share(ret);

	return ret;
}

struct so_value_iterator * so_value_list_get_iterator(struct so_value * list) {
	return so_value_list_get_iterator2(list, false);
}

static struct so_value_iterator * so_value_list_get_iterator2(struct so_value * list, bool weak_ref) {
	if (list == NULL || (list->type != so_value_array && list->type != so_value_linked_list))
		return NULL;

	if (!weak_ref)
		list = so_value_share(list);

	struct so_value_iterator * iter = NULL;
	if (list->type == so_value_array) {
		iter = malloc(sizeof(struct so_value_iterator) + sizeof(struct so_value_iterator_array));
		struct so_value_iterator_array * iter_array = (struct so_value_iterator_array *) (iter + 1);

		iter_array->index = 0;
	} else {
		struct so_value_linked_list * value_linked_list = so_value_get(list);

		iter = malloc(sizeof(struct so_value_iterator) + sizeof(struct so_value_iterator_linked_list));
		struct so_value_iterator_linked_list * iter_linked_list = (struct so_value_iterator_linked_list *) (iter + 1);

		iter_linked_list->linked_list = value_linked_list;
		iter_linked_list->previous_detached = false;
		iter_linked_list->previous = NULL;
		iter_linked_list->next = value_linked_list->first;
	}

	iter->value = list;
	iter->weak_ref = weak_ref;

	return iter;
}

unsigned int so_value_list_get_length(struct so_value * list) {
	if (list == NULL || (list->type != so_value_array && list->type != so_value_linked_list))
		return 0;

	if (list->type == so_value_array) {
		struct so_value_array * array = so_value_get(list);
		return array->nb_vals;
	} else {
		struct so_value_linked_list * linked = so_value_get(list);
		return linked->nb_vals;
	}
}

int so_value_list_index_of(struct so_value * list, struct so_value * elt) {
	if (list == NULL || (list->type != so_value_array && list->type != so_value_linked_list))
		return -1;

	bool found = false;
	int index = -1;
	struct so_value_iterator * iter = so_value_list_get_iterator(list);
	while (!found && so_value_iterator_has_next(iter)) {
		struct so_value * i_elt = so_value_iterator_get_value(iter, false);
		found = so_value_equals(elt, i_elt);
		index++;
	}
	so_value_iterator_free(iter);

	return found ? index : -1;
}

struct so_value * so_value_list_pop(struct so_value * list) {
	if (list == NULL || (list->type != so_value_array && list->type != so_value_linked_list))
		return NULL;

	struct so_value * ret = NULL;

	if (list->type == so_value_array) {
		struct so_value_array * array = so_value_get(list);
		if (array->nb_vals > 0) {
			array->nb_vals--;
			ret = array->values[array->nb_vals];
			array->values[array->nb_vals] = NULL;
		}
	} else {
		struct so_value_linked_list * linked = so_value_get(list);
		struct so_value_linked_list_node * node = linked->last;
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

bool so_value_list_push(struct so_value * list, struct so_value * val, bool new_val) {
	if (list == NULL || val == NULL || (list->type != so_value_array && list->type != so_value_linked_list))
		return false;

	if (list->type == so_value_array) {
		struct so_value_array * array = so_value_get(list);
		if (array->nb_vals == array->nb_preallocated) {
			void * new_addr = realloc(array->values, (array->nb_vals + 16) * sizeof(struct so_value *));
			if (new_addr == NULL)
				return false;

			array->values = new_addr;
			array->nb_preallocated += 16;
		}

		if (!new_val)
			val = so_value_share(val);

		array->values[array->nb_vals] = val;
		array->nb_vals++;

		return true;
	} else {
		struct so_value_linked_list * linked = so_value_get(list);
		struct so_value_linked_list_node * node = malloc(sizeof(struct so_value_linked_list_node));
		if (node == NULL)
			return false;

		if (!new_val)
			val = so_value_share(val);

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

bool so_value_list_remove(struct so_value * list, unsigned int index) {
	if (list == NULL || (list->type != so_value_array && list->type != so_value_linked_list))
		return false;

	if (list->type == so_value_array) {
		struct so_value_array * array = so_value_get(list);
		if (index < array->nb_vals) {
			so_value_free(array->values[index]);
			array->nb_vals--;
			memmove(array->values + index, array->values + index + 1, (array->nb_vals - index) * sizeof(struct so_value *));
			array->values[array->nb_vals] = NULL;
			return true;
		}
	} else {
		struct so_value_linked_list * linked = so_value_get(list);
		if (index >= linked->nb_vals)
			return false;

		struct so_value_linked_list_node * node = linked->first;
		unsigned int i;
		for (i = 0; i < index && node != NULL; i++, node = node->next);

		if (node != NULL) {
			if (node->previous != NULL)
				node->previous->next = node->next;
			else
				linked->first = node->next;

			if (node->next != NULL)
				node->next->previous = node->previous;
			else
				linked->last = node->previous;

			so_value_free(node->value);
			linked->nb_vals--;

			free(node);

			return true;
		}
	}

	return false;
}

struct so_value * so_value_list_shift(struct so_value * list) {
	if (list == NULL || (list->type != so_value_array && list->type != so_value_linked_list))
		return NULL;

	struct so_value * ret = NULL;

	if (list->type == so_value_array) {
		struct so_value_array * array = so_value_get(list);
		if (array->nb_vals > 0) {
			ret = array->values[0];
			array->nb_vals--;
			memmove(array->values, array->values + 1, array->nb_vals * sizeof(struct so_value *));
			array->values[array->nb_vals] = NULL;
		}
	} else {
		struct so_value_linked_list * linked = so_value_get(list);
		struct so_value_linked_list_node * node = linked->first;
		if (node != NULL) {
			linked->first = node->next;
			if (linked->first == NULL)
				linked->last = NULL;

			ret = node->value;
			linked->nb_vals--;

			free(node);
		}
	}

	return ret;
}

struct so_value * so_value_list_slice(struct so_value * list, int index) {
	if (list == NULL || (list->type != so_value_array && list->type != so_value_linked_list))
		return so_value_new_linked_list();

	int length = so_value_list_get_length(list);
	struct so_value * ret = so_value_new_array(length);

	if (index < 0) {
		index += length;
		if (index < 0)
			index = 0;
	} else if (index >= length)
		return ret;

	struct so_value_iterator * iter = so_value_list_get_iterator(list);
	while (index > 0 && so_value_iterator_has_next(iter)) {
		so_value_iterator_get_value(iter, false);
		index--;
	}

	if (index == 0)
		while (so_value_iterator_has_next(iter))
			so_value_list_push(ret, so_value_iterator_get_value(iter, true), true);

	so_value_iterator_free(iter);

	return ret;
}

struct so_value * so_value_list_slice2(struct so_value * list, int index, int end) {
	if (list == NULL || (list->type != so_value_array && list->type != so_value_linked_list))
		return so_value_new_linked_list();

	int length = so_value_list_get_length(list);
	struct so_value * ret = so_value_new_array(length);

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

	struct so_value_iterator * iter = so_value_list_get_iterator(list);
	while (index > 0 && so_value_iterator_has_next(iter)) {
		so_value_iterator_get_value(iter, false);
		index--;
		end--;
	}

	if (index == 0)
		while (end > 0 && so_value_iterator_has_next(iter)) {
			so_value_list_push(ret, so_value_iterator_get_value(iter, true), true);
			end--;
		}

	so_value_iterator_free(iter);

	return ret;
}

struct so_value * so_value_list_splice(struct so_value * list, int index, int how_many, ...) {
	if (list == NULL || (list->type != so_value_array && list->type != so_value_linked_list))
		return so_value_new_linked_list();

	int length = so_value_list_get_length(list);
	struct so_value * ret = so_value_new_array(length);

	if (index < 0) {
		index += length;
		if (index < 0)
			index = 0;
	} else if (index >= length)
		index = length;

	if (list->type == so_value_array) {
		struct so_value_array * array = so_value_get(list);
		struct so_value * tmp_elts = so_value_new_linked_list();

		if (index < length && how_many > 0) {
			int i;
			for (i = index; i < index + how_many && i < length; i++) {
				so_value_list_push(ret, array->values[i], false);
				array->values[i] = NULL;
				array->nb_vals--;
			}

			for (; i < length; i++) {
				so_value_list_push(tmp_elts, array->values[i], false);
				array->values[i] = NULL;
				array->nb_vals--;
			}
		}

		va_list args;
		va_start(args, how_many);

		struct so_value * new_val = va_arg(args, struct so_value *);
		while (new_val != NULL) {
			so_value_list_push(list, new_val, false);
			new_val = va_arg(args, struct so_value *);
		}

		va_end(args);

		struct so_value_iterator * iter = so_value_list_get_iterator(tmp_elts);
		while (so_value_iterator_has_next(iter)) {
			struct so_value * elt = so_value_iterator_get_value(iter, false);
			so_value_list_push(list, elt, false);
		}
		so_value_iterator_free(iter);
		so_value_free(tmp_elts);
	} else {
		struct so_value_linked_list * llist = so_value_get(list);
		struct so_value_linked_list_node * node = llist->first;

		while (node != NULL && index > 0) {
			node = node->next;
			index--;
		}

		while (node != NULL && how_many > 0) {
			so_value_list_push(ret, node->value, true);
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

		struct so_value * new_val = va_arg(args, struct so_value *);
		while (new_val != NULL) {
			if (node == NULL || node->next == NULL)
				so_value_list_push(list, new_val, false);
			else {
				struct so_value_linked_list_node * new_node = malloc(sizeof(struct so_value_linked_list_node));
				new_node->value = so_value_share(new_val);
				new_node->next = node;
				new_node->previous = node->previous;

				if (new_node->previous != NULL)
					new_node->previous->next = new_node;
				else
					llist->first = new_node;
				node->previous = new_node;

				llist->nb_vals++;
			}

			new_val = va_arg(args, struct so_value *);
		}

		va_end(args);
	}

	return ret;
}

bool so_value_list_unshift(struct so_value * list, struct so_value * val, bool new_val) {
	if (list == NULL || val == NULL || (list->type != so_value_array && list->type != so_value_linked_list))
		return false;

	if (list->type == so_value_array) {
		struct so_value_array * array = so_value_get(list);
		if (array->nb_vals == array->nb_preallocated) {
			void * new_addr = realloc(array->values, (array->nb_vals + 16) * sizeof(struct so_value *));
			if (new_addr == NULL)
				return false;

			array->values = new_addr;
			array->nb_preallocated += 16;
		}

		if (!new_val)
			val = so_value_share(val);

		memmove(array->values + 1, array->values, array->nb_vals * sizeof(struct so_value *));
		array->values[0] = val;
		array->nb_vals++;

		return true;
	} else {
		struct so_value_linked_list_node * node = malloc(sizeof(struct so_value_linked_list_node));
		if (node == NULL)
			return false;

		if (!new_val)
			val = so_value_share(val);

		struct so_value_linked_list * linked = so_value_get(list);
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


const char * so_value_string_get(const struct so_value * value) {
	return so_value_get(value);
}


bool so_value_iterator_detach_previous(struct so_value_iterator * iter) {
	if (iter == NULL)
		return false;

	switch (iter->value->type) {
		case so_value_array: {
				struct so_value_array * array = so_value_get(iter->value);
				struct so_value_iterator_array * iter_array = (struct so_value_iterator_array *) (iter + 1);

				if (iter_array->index < 1)
					return false;

				array->nb_vals--;
				iter_array->index--;
				memmove(array->values + iter_array->index, array->values + iter_array->index + 1, (array->nb_vals - iter_array->index) * sizeof(struct so_value *));
				array->values[array->nb_vals] = NULL;

				return true;
			}

		case so_value_hashtable: {
				struct so_value_iterator_hashtable * iter_hash = (struct so_value_iterator_hashtable *) (iter + 1);

				if (iter_hash->previous.node == NULL)
					return false;

				struct so_value_hashtable * hash = so_value_get(iter->value);
				struct so_value_hashtable_node * node = hash->nodes[iter_hash->previous.i_elements];
				if (node->hash == iter_hash->previous.node->hash) {
					hash->nodes[iter_hash->previous.i_elements] = node->next;

					so_value_free(node->key);
					free(node);
					hash->nb_elements--;

					return true;
				}

				while (node->next != NULL && node->next->hash != iter_hash->previous.node->hash)
					node = node->next;

				if (node->next->hash == iter_hash->previous.node->hash) {
					struct so_value_hashtable_node * tmp = node->next;
					node->next = tmp->next;

					so_value_free(tmp->key);
					free(tmp);
					hash->nb_elements--;

					return true;
				}

				return false;
			}
			break;

		case so_value_linked_list: {
				struct so_value_iterator_linked_list * iter_list = (struct so_value_iterator_linked_list *) (iter + 1);

				struct so_value_linked_list_node * old = iter_list->previous;
				if (old->previous != NULL)
					old->previous->next = old->next;
				else
					iter_list->linked_list->first = old->next;
				if (old->next != NULL)
					old->next->previous = old->previous;
				else
					iter_list->linked_list->last = old->previous;

				iter_list->previous_detached = true;
				iter_list->linked_list->nb_vals--;

				return true;
			}
			break;

		default:
			return false;
	}
}

void so_value_iterator_free(struct so_value_iterator * iter) {
	if (iter == NULL)
		return;

	if (iter->value->type == so_value_linked_list) {
		struct so_value_iterator_linked_list * iter_list = (struct so_value_iterator_linked_list *) (iter + 1);

		if (iter_list->previous_detached)
			free(iter_list->previous);
	}

	if (iter->value != NULL && !iter->weak_ref)
		so_value_free(iter->value);
	iter->value = NULL;

	free(iter);
}

struct so_value * so_value_iterator_get_key(struct so_value_iterator * iter, bool move_to_next, bool shared) {
	if (iter == NULL || iter->value == NULL || iter->value->type != so_value_hashtable)
		return &null_value;

	struct so_value_iterator_hashtable * iter_hash = (struct so_value_iterator_hashtable *) (iter + 1);
	struct so_value_hashtable_node * node = iter_hash->next.node;
	struct so_value * key = node->key;

	if (shared)
		key = so_value_share(key);

	if (move_to_next) {
		struct so_value_hashtable * hash = so_value_get(iter->value);

		iter_hash->previous.node = node;

		iter_hash->next.node = node->next;

		if (iter_hash->next.node == NULL) {
			while (iter_hash->next.i_elements < hash->size_node && iter_hash->next.node == NULL)
				iter_hash->next.node = hash->nodes[iter_hash->next.i_elements++];

			if (iter_hash->next.i_elements < hash->size_node)
				iter_hash->previous.i_elements = iter_hash->next.i_elements - 1;
		}
	}

	return key;
}

struct so_value * so_value_iterator_get_value(struct so_value_iterator * iter, bool shared) {
	if (iter == NULL || iter->value == NULL)
		return &null_value;

	struct so_value * ret = &null_value;
	switch (iter->value->type) {
		case so_value_array: {
				struct so_value_array * array = so_value_get(iter->value);
				struct so_value_iterator_array * iter_array = (struct so_value_iterator_array *) (iter + 1);

				if (iter_array->index >= array->nb_vals)
					break;

				ret = array->values[iter_array->index];
				iter_array->index++;
				break;
			}

		case so_value_hashtable: {
				struct so_value_iterator_hashtable * iter_hash = (struct so_value_iterator_hashtable *) (iter + 1);

				if (iter_hash->next.node == NULL)
					break;

				struct so_value_hashtable * hash = so_value_get(iter->value);
				struct so_value_hashtable_node * node = iter_hash->next.node;
				ret = node->value;

				iter_hash->previous.node = node;

				iter_hash->next.node = node->next;
				if (iter_hash->next.node != NULL)
					break;

				for (; iter_hash->next.i_elements < hash->size_node && iter_hash->next.node == NULL; iter_hash->next.i_elements++)
					iter_hash->next.node = hash->nodes[iter_hash->next.i_elements];

				if (iter_hash->next.i_elements < hash->size_node)
					iter_hash->previous.i_elements = iter_hash->next.i_elements - 1;
			}
			break;

		case so_value_linked_list: {
				struct so_value_iterator_linked_list * iter_list = (struct so_value_iterator_linked_list *) (iter + 1);

				if (iter_list->previous_detached) {
					free(iter_list->previous);
					iter_list->previous_detached = false;
				}

				iter_list->previous = iter_list->next;
				ret = iter_list->next->value;

				if (iter_list->next != NULL)
					iter_list->next = iter_list->next->next;
			}
			break;

		default:
			return &null_value;
	}

	if (ret != &null_value && shared)
		ret = so_value_share(ret);

	return ret;
}

bool so_value_iterator_has_next(struct so_value_iterator * iter) {
	if (iter == NULL)
		return false;

	switch (iter->value->type) {
		case so_value_array: {
				struct so_value_array * array = so_value_get(iter->value);
				struct so_value_iterator_array * iter_array = (struct so_value_iterator_array *) (iter + 1);

				return iter_array->index < array->nb_vals;
			}

		case so_value_hashtable: {
				struct so_value_iterator_hashtable * iter_hash = (struct so_value_iterator_hashtable *) (iter + 1);
				return iter_hash->next.node != NULL;
			}

		case so_value_linked_list: {
				struct so_value_iterator_linked_list * iter_list = (struct so_value_iterator_linked_list *) (iter + 1);
				return iter_list->next != NULL;
			}

		default:
			return false;
	}
}

