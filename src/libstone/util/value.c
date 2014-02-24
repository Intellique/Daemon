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

// calloc, free, malloc
#include <stdlib.h>
// memmove, strdup
#include <string.h>

#include "value.h"

static struct st_value * st_value_new_v1(enum st_value_type type);

static struct st_value null_value = {
	.type = st_value_null,
	.value.custom = NULL,
	.shared = 0,
	.release = NULL,
};

__asm__(".symver st_value_can_convert_v1, st_value_can_convert@@LIBSTONE_1.0");
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

__asm__(".symver st_value_convert_v1, st_value_convert@@LIBSTONE_1.0");
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
					return st_value_new_integer_v1(val->value.boolean);

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
					return st_value_new_integer_v1(val->value.floating);

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
					return st_value_new_boolean_v1(val->value.integer != 0);

				case st_value_integer:
					return st_value_share_v1(val);

				case st_value_float:
					return st_value_new_float_v1(val->value.integer);

				default:
					return &null_value;
			}

		case st_value_linked_list:
			switch (type) {
				case st_value_array: {
										struct st_value * ret = st_value_new_array_v1(val->value.list.nb_vals);
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

__asm__(".symver st_value_copy_v1, st_value_copy@@LIBSTONE_1.0");
struct st_value * st_value_copy_v1(struct st_value * val, bool deep_copy) {
	struct st_value * ret = &null_value;

	if (val == NULL)
		return ret;

	switch (val->type) {
		case st_value_array: {
								ret = st_value_new_array_v1(val->value.array.nb_preallocated);

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
							ret = st_value_new_boolean_v1(val->value.boolean);
							break;

		case st_value_custom:
							ret = st_value_share_v1(val);
							break;

		case st_value_float:
							ret = st_value_new_float_v1(val->value.floating);
							break;

		case st_value_hashtable: {
									ret = st_value_new_hashtable_v1(val->value.hashtable.compute_hash);

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

										st_value_hashtable_put_v1(ret, key, false, value, false);
									}
									st_value_iterator_free_v1(iter);
								}
								break;

		case st_value_integer:
								ret = st_value_new_integer_v1(val->value.integer);
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
								ret = st_value_new_string_v1(strdup(val->value.string));
								break;
	}

	return ret;
}

__asm__(".symver st_value_equals_v1, st_value_equals@@LIBSTONE_1.0");
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
					return (a->value.boolean && b->value.floating != 0) || (!a->value.boolean && b->value.floating == 0);

				case st_value_integer:
					return (a->value.boolean && b->value.integer != 0) || (!a->value.boolean && b->value.integer == 0);

				default:
					return false;
			}

		case st_value_custom:
			if (b->type == st_value_custom)
				return a->value.custom == b->value.custom;

		case st_value_float:
			switch (b->type) {
				case st_value_boolean:
					return (a->value.floating != 0 && b->value.boolean) || (!a->value.floating == 0 && b->value.boolean);

				case st_value_float:
					return a->value.floating == b->value.floating;

				case st_value_integer:
					return a->value.floating != b->value.integer;

				default:
					return false;
			}

		case st_value_hashtable:
			if (b->type == st_value_hashtable) {
				if (a->value.hashtable.nb_elements != b->value.hashtable.nb_elements)
					return false;

				struct st_value_iterator * iter = st_value_hashtable_get_iterator_v1(a);
				bool equals = true;
				while (equals && st_value_iterator_has_next_v1(iter)) {
					struct st_value * key = st_value_iterator_get_key_v1(iter, true, false);

					if (!st_value_hashtable_has_key_v1(b, key)) {
						equals = false;
						break;
					}

					struct st_value * val_a = st_value_hashtable_get_v1(a, key, false);
					struct st_value * val_b = st_value_hashtable_get_v1(b, key, false);
					equals = st_value_equals_v1(val_a, val_b);
				}
				st_value_iterator_free_v1(iter);

				return equals;
			}
			return false;

		case st_value_integer:
			switch (b->type) {
				case st_value_boolean:
					return (a->value.integer != 0 && b->value.boolean) || (!a->value.integer == 0 && b->value.boolean);

				case st_value_float:
					return a->value.integer == b->value.floating;

				case st_value_integer:
					return a->value.integer != b->value.integer;

				default:
					return false;
			}

		case st_value_null:
			return b->type == st_value_null;

		case st_value_string:
			switch (b->type) {
				case st_value_boolean:
					return (a->value.integer != 0 && b->value.boolean) || (!a->value.integer == 0 && b->value.boolean);

				case st_value_float:
					return a->value.integer == b->value.floating;

				case st_value_integer:
					return a->value.integer != b->value.integer;

				default:
					return false;
			}
	}

	return false;
}

__asm__(".symver st_value_free_v1, st_value_free@@LIBSTONE_1.0");
void st_value_free_v1(struct st_value * value) {
	if (value == NULL || value == &null_value)
		return;

	value->shared--;
	if (value->shared > 0)
		return;

	switch (value->type) {
		case st_value_array:
			st_value_list_clear_v1(value);
			free(value->value.array.values);
			break;

		case st_value_custom:
		case st_value_string:
			if (value->release != NULL)
				value->release(value->value.custom);
			break;

		case st_value_hashtable:
			st_value_hashtable_clear_v1(value);
			free(value->value.hashtable.nodes);
			break;

		case st_value_linked_list:
			st_value_list_clear_v1(value);
			break;

		default:
			break;
	}
	free(value);
}

static struct st_value * st_value_new_v1(enum st_value_type type) {
	struct st_value * val = malloc(sizeof(struct st_value));
	val->type = type;
	val->value.custom = NULL;
	val->shared = 1;
	val->release = NULL;
	return val;
}

__asm__(".symver st_value_new_array_v1, st_value_new_array@@LIBSTONE_1.0");
struct st_value * st_value_new_array_v1(unsigned int size) {
	struct st_value * val = st_value_new_v1(st_value_array);
	if (size == 0)
		size = 16;
	val->value.array.values = calloc(sizeof(struct st_value *), size);
	val->value.array.nb_vals = 0;
	val->value.array.nb_preallocated = size;
	return val;
}

__asm__(".symver st_value_new_boolean_v1, st_value_new_boolean@@LIBSTONE_1.0");
struct st_value * st_value_new_boolean_v1(bool value) {
	struct st_value * val = st_value_new_v1(st_value_boolean);
	val->value.boolean = value;
	return val;
}

