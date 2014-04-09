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

#ifndef __LIBSTONE_VALUE_H_P__
#define __LIBSTONE_VALUE_H_P__

#include <libstone/value.h>

#define st_value_v1 st_value
#define st_value_free_f_v1 st_value_free_f
#define st_value_hashtable_compupte_hash_f_v1 st_value_hashtable_compupte_hash_f
#define st_value_iterator_v1 st_value_iterator

bool st_value_can_convert_v1(struct st_value_v1 * val, enum st_value_type type) __attribute__((nonnull));
struct st_value_v1 * st_value_convert_v1(struct st_value_v1 * val, enum st_value_type type) __attribute__((nonnull,warn_unused_result));
struct st_value_v1 * st_value_copy_v1(struct st_value_v1 * val, bool deep_copy) __attribute__((nonnull,warn_unused_result));
bool st_value_equals_v1(struct st_value_v1 * a, struct st_value_v1 * b) __attribute__((nonnull));
void st_value_free_v1(struct st_value_v1 * value) __attribute__((nonnull));
struct st_value_v1 * st_value_new_array_v1(unsigned int size) __attribute__((warn_unused_result));
struct st_value_v1 * st_value_new_boolean_v1(bool value) __attribute__((warn_unused_result));
struct st_value_v1 * st_value_new_custom_v1(void * value, st_value_free_f_v1 release) __attribute__((warn_unused_result));
struct st_value_v1 * st_value_new_float_v1(double value) __attribute__((warn_unused_result));
struct st_value_v1 * st_value_new_hashtable_v1(st_value_hashtable_compupte_hash_f compute_hash) __attribute__((nonnull,warn_unused_result));
struct st_value_v1 * st_value_new_hashtable2_v1(void) __attribute__((warn_unused_result));
struct st_value_v1 * st_value_new_integer_v1(long long int value) __attribute__((warn_unused_result));
struct st_value_v1 * st_value_new_linked_list_v1(void) __attribute__((warn_unused_result));
struct st_value_v1 * st_value_new_null_v1(void) __attribute__((warn_unused_result));
struct st_value_v1 * st_value_new_string_v1(const char * value) __attribute__((nonnull,warn_unused_result));
struct st_value_v1 * st_value_pack_v1(const char * format, ...) __attribute__((nonnull(1),warn_unused_result));
struct st_value_v1 * st_value_share_v1(struct st_value_v1 * value) __attribute__((nonnull,warn_unused_result));
int st_value_unpack_v1(struct st_value_v1 * root, const char * format, ...) __attribute__((nonnull));
bool st_value_valid_v1(struct st_value_v1 * value, const char * format, ...) __attribute__((nonnull));
struct st_value * st_value_vpack_v1(const char * format, va_list params) __attribute__((nonnull(1),warn_unused_result));

void st_value_hashtable_clear_v1(struct st_value_v1 * hash) __attribute__((nonnull));
struct st_value_v1 * st_value_hashtable_get_v1(struct st_value_v1 * hash, struct st_value_v1 * key, bool shared, bool detach) __attribute__((nonnull,warn_unused_result));
struct st_value_v1 * st_value_hashtable_get2_v1(struct st_value_v1 * hash, const char * key, bool shared) __attribute__((nonnull,warn_unused_result));
struct st_value_iterator_v1 * st_value_hashtable_get_iterator_v1(struct st_value_v1 * hash) __attribute__((nonnull,warn_unused_result));
bool st_value_hashtable_has_key_v1(struct st_value_v1 * hash, struct st_value_v1 * key) __attribute__((nonnull));
bool st_value_hashtable_has_key2_v1(struct st_value_v1 * hash, const char * key) __attribute__((nonnull));
struct st_value_v1 * st_value_hashtable_keys_v1(struct st_value_v1 * hash) __attribute__((nonnull));
void st_value_hashtable_put_v1(struct st_value_v1 * hash, struct st_value_v1 * key, bool new_key, struct st_value_v1 * value, bool new_value) __attribute__((nonnull));
void st_value_hashtable_put2_v1(struct st_value_v1 * hash, const char * key, struct st_value_v1 * value, bool new_value) __attribute__((nonnull));
void st_value_hashtable_remove_v1(struct st_value_v1 * hash, struct st_value_v1 * key) __attribute__((nonnull));
struct st_value_v1 * st_value_hashtable_values_v1(struct st_value_v1 * hash) __attribute__((nonnull));

void st_value_list_clear_v1(struct st_value_v1 * list) __attribute__((nonnull));
struct st_value_iterator_v1 * st_value_list_get_iterator_v1(struct st_value_v1 * list) __attribute__((nonnull,warn_unused_result));
unsigned int st_value_list_get_length_v1(struct st_value_v1 * list) __attribute__((nonnull));
struct st_value_v1 * st_value_list_pop_v1(struct st_value_v1 * list) __attribute__((nonnull,warn_unused_result));
bool st_value_list_push_v1(struct st_value_v1 * list, struct st_value_v1 * val, bool new_val) __attribute__((nonnull));
bool st_value_list_shift_v1(struct st_value_v1 * list, struct st_value_v1 * val, bool new_val) __attribute__((nonnull));
struct st_value_v1 * st_value_list_unshift_v1(struct st_value_v1 * list) __attribute__((nonnull,warn_unused_result));

void st_value_iterator_free_v1(struct st_value_iterator_v1 * iter) __attribute__((nonnull));
struct st_value_v1 * st_value_iterator_get_key_v1(struct st_value_iterator_v1 * iter, bool move_to_next, bool shared) __attribute__((nonnull,warn_unused_result));
struct st_value_v1 * st_value_iterator_get_value_v1(struct st_value_iterator_v1 * iter, bool shared) __attribute__((nonnull,warn_unused_result));
bool st_value_iterator_has_next_v1(struct st_value_iterator_v1 * iter) __attribute__((nonnull));

#endif

