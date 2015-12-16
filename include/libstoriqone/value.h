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

#ifndef __LIBSTORIQONE_VALUE_H__
#define __LIBSTORIQONE_VALUE_H__

// va_list
#include <stdarg.h>
// bool
#include <stdbool.h>
// ssize_t
#include <sys/types.h>

/**
 * \brief Wrapper of value
 *
 * This structure can contain differents kind of value. Each value can be shared.
 */
struct so_value {
		/**
		 * \enum so_value_type
		 * \brief Type of value
		 */
		enum so_value_type {
				/**
				 * \brief Used to store an array list
				 */
				so_value_array = 0x1,
				/**
				 * \brief Used to store a boolean value
				 */
				so_value_boolean = 0x2,
				/**
				 * \brief Used to store a custom value
				 */
				so_value_custom = 0x3,
				/**
				 * \brief Used to store a floating value
				 */
				so_value_float = 0x4,
				/**
				 * \brief Used to store an hashtable
				 */
				so_value_hashtable = 0x5,
				/**
				 * \brief Used to store an integer value
				 */
				so_value_integer = 0x6,
				/**
				 * \brief Used to store a linked list
				 */
				so_value_linked_list = 0x7,
				/**
				 * \brief Used to store a null value
				 */
				so_value_null = 0x8,
				/**
				 * \brief Used to store a string value
				 */
				so_value_string = 0x9,
		} type;
		/**
		 * \brief Number of many times this value is shared
		 *
		 * \attention Should not be modify
		 */
		unsigned int shared;
		void * data;
} __attribute__((packed));

/**
 * \brief Used to free custom value
 *
 * \param[in] value value to release
 */
typedef void (*so_value_free_f)(void * value);

/**
 * \brief Function used to compute hash of keys
 *
 * \param[in] key : a \a key
 * \returns hash
 *
 * \note Mathematical consideration : \f$ \forall (x, y), x \ne y \Rightarrow f(x) \ne f(y) \f$ \n
 * If \f$ x \ne y, f(x) = f(y) \f$, the hash function admits a collision. \n
 * Hash function returns a 64 bits integer so we can have \f$ 2^{64}-1 \f$ (= 18446744073709551615) differents values
 */
typedef unsigned long long (*so_value_hashtable_compupte_hash_f)(const struct so_value * key);

struct so_value_iterator {
	struct so_value * value;
	bool weak_ref;
};


/**
 * \brief test if \a val can be convert into type \a type
 *
 * \param[in] val test this value
 * \param[in] type type of conversion
 * \returns \b true if \a val can be converted into \a type
 */
bool so_value_can_convert(struct so_value * val, enum so_value_type type);
/**
 * \brief Convert \a val into \a type
 *
 * Before call this function, call so_value_can_convert to check if convertion is posible.
 *
 * \param[in] val value to convert
 * \param[in] type type of conversion
 * \returns new value
 * \note if conversion failed, it returns a so_value which represent a null value
 * \see so_value_convert
 */
struct so_value * so_value_convert(struct so_value * val, enum so_value_type type) __attribute__((warn_unused_result));
/**
 * \brief Copy \a val into new value
 *
 * \param[in] val val to copy
 * \param[in] deep_copy copy recursively \a val
 * \returns a copy of \a val
 */
struct so_value * so_value_copy(struct so_value * val, bool deep_copy) __attribute__((warn_unused_result));
/**
 * \brief Test if \a a and \a b are equals
 *
 * \param[in] a a value
 * \param[in] b a value
 * \returns \b true if \a a and \a b are equals
 */
bool so_value_equals(struct so_value * a, struct so_value * b);
/**
 * \brief Release value
 *
 * Release value if \a value is not shared.
 *
 * \param[in] value release this \a value
 */
void so_value_free(struct so_value * value);
void * so_value_get(const struct so_value * value);
/**
 * \brief Create new array list
 *
 * \param[in] size : pre allocate \a size elements into array
 * \returns new array value
 * \note if \a size is equals to \b 0, pre allocation will be <b>16 elements</b>
 */
struct so_value * so_value_new_array(unsigned int size) __attribute__((warn_unused_result));
/**
 * \brief Create new boolean value
 *
 * \param[in] value value of boolean type
 * \returns new boolean value
 */
struct so_value * so_value_new_boolean(bool value) __attribute__((warn_unused_result));
/**
 * \brief Create new custom value
 *
 * Custom value is used to store value which can not be stored otherwise.
 *
 * \param[in] value address of custom value.
 * \param[in] release function used to release custom value. \a release can be NULL
 * \returns new custom value
 */
struct so_value * so_value_new_custom(void * value, so_value_free_f release) __attribute__((warn_unused_result));
/**
 * \brief Create new floating value
 *
 * \param[in] value a floating type
 * \returns new floating type value
 */
struct so_value * so_value_new_float(double value) __attribute__((warn_unused_result));
/**
 * \brief Create new hashtable
 *
 * \param[in] compute_hash function used to compute hash from key
 * \returns new hashtable value
 */
struct so_value * so_value_new_hashtable(so_value_hashtable_compupte_hash_f compute_hash) __attribute__((warn_unused_result));
struct so_value * so_value_new_hashtable2(void) __attribute__((warn_unused_result));
/**
 * \brief Create new integer value
 *
 * \param[in] value value of integer type
 * \returns new integer type value
 */
struct so_value * so_value_new_integer(long long int value) __attribute__((warn_unused_result));
/**
 * \brief Create new linked list
 *
 * \returns new linked list
 */
struct so_value * so_value_new_linked_list(void) __attribute__((warn_unused_result));
/**
 * \brief Get null value
 *
 * \returns a null value represented by a so_value
 */
struct so_value * so_value_new_null(void) __attribute__((warn_unused_result));
/**
 * \brief Create new string value type
 *
 * \param[in] value a string value which will be duplicated
 * \returns new string value
 */
struct so_value * so_value_new_string(const char * value) __attribute__((warn_unused_result));
struct so_value * so_value_new_string2(const char * value, ssize_t length) __attribute__((warn_unused_result));
struct so_value * so_value_pack(const char * format, ...) __attribute__((warn_unused_result));
/**
 * \brief Share this value
 *
 * \param[in] value value be be sharred
 * \returns sharred value
 */
struct so_value * so_value_share(struct so_value * value) __attribute__((warn_unused_result));
int so_value_unpack(struct so_value * root, const char * format, ...);
bool so_value_valid(struct so_value * value, const char * format, ...);
struct so_value * so_value_vpack(const char * format, va_list params) __attribute__((warn_unused_result));
int so_value_vunpack(struct so_value * root, const char * format, va_list params);
bool so_value_vvalid(struct so_value * value, const char * format, va_list params);

bool so_value_boolean_get(const struct so_value * value);

unsigned long long so_value_custom_compute_hash(const struct so_value * value);
void * so_value_custom_get(const struct so_value * value);

double so_value_float_get(const struct so_value * value);

/**
 * \brief remove all elements from hashtable
 *
 * \param[in] hash an hashtable value
 */
void so_value_hashtable_clear(struct so_value * hash);
/**
 * \brief Get an element from hashtable with a key
 *
 * \param[in] hash an hashtable value
 * \param[in] key key associated to one value stored into hashtable
 * \param[in] detach if \b true then remove also value from hashtable
 * \returns \b null or the value associated to \a key
 */
struct so_value * so_value_hashtable_get(struct so_value * hash, struct so_value * key, bool shared, bool detach) __attribute__((warn_unused_result));
struct so_value * so_value_hashtable_get2(struct so_value * hash, const char * key, bool shared, bool detach) __attribute__((warn_unused_result));
/**
 * \brief Get an object to iter through an hashtable
 *
 * \param[in] hash iter from this hashtable
 * \return an iterator
 */
struct so_value_iterator * so_value_hashtable_get_iterator(struct so_value * hash) __attribute__((warn_unused_result));
unsigned int so_value_hashtable_get_length(struct so_value * hash);
/**
 * \brief Check if \a key is present into hashtable
 *
 * \param[in] hash an hashtable value
 * \param[in] key check this \a key
 * \return \b true if \a key is found
 */
bool so_value_hashtable_has_key(struct so_value * hash, struct so_value * key);
bool so_value_hashtable_has_key2(struct so_value * hash, const char * key);
/**
 * \brief Get a list of keys presents into hashtable
 *
 * \param[in] hash an hashtable value
 * \return an array list of keys of \a hash
 */
struct so_value * so_value_hashtable_keys(struct so_value * hash);
/**
 * \brief Add new value to hashtable
 *
 * \param[in] hash an hashtable value
 * \param[in] key key associated to new value
 * \param[in] new_key steal \a key
 * \param[in] value new value
 * \param[in] new_value steal \a value
 * \note if \a new_key is equal to \b false then function will get a shared value of \a key. item for \a new_value
 */
void so_value_hashtable_put(struct so_value * hash, struct so_value * key, bool new_key, struct so_value * value, bool new_value);
void so_value_hashtable_put2(struct so_value * hash, const char * key, struct so_value * value, bool new_value);
/**
 * \brief Remove value from hashtable
 *
 * \param[in] hash an hashtable value
 * \param[in] key remove a value associated to this \a key
 */
void so_value_hashtable_remove(struct so_value * hash, struct so_value * key);
void so_value_hashtable_remove2(struct so_value * hash, const char * key);
/**
 * \brief Get a list of all values
 *
 * \param[in] hash an hashtable value
 * \return an array list of values of \a hash
 */
struct so_value * so_value_hashtable_values(struct so_value * hash);

long long int so_value_integer_get(const struct so_value * value);

void so_value_list_clear(struct so_value * list);
struct so_value * so_value_list_get(struct so_value * list, unsigned int index, bool shared) __attribute__((warn_unused_result));
struct so_value_iterator * so_value_list_get_iterator(struct so_value * list) __attribute__((warn_unused_result));
unsigned int so_value_list_get_length(struct so_value * list);
int so_value_list_index_of(struct so_value * list, struct so_value * elt);
struct so_value * so_value_list_pop(struct so_value * list) __attribute__((warn_unused_result));
bool so_value_list_push(struct so_value * list, struct so_value * val, bool new_val);
bool so_value_list_remove(struct so_value * list, unsigned int index);
struct so_value * so_value_list_shift(struct so_value * list) __attribute__((warn_unused_result));
struct so_value * so_value_list_slice(struct so_value * list, int index) __attribute__((warn_unused_result));
struct so_value * so_value_list_slice2(struct so_value * list, int index, int end) __attribute__((warn_unused_result));
struct so_value * so_value_list_splice(struct so_value * list, int index, int how_many, ...) __attribute__((warn_unused_result));
bool so_value_list_unshift(struct so_value * list, struct so_value * val, bool new_val);

const char * so_value_string_get(const struct so_value * value);

bool so_value_iterator_detach_previous(struct so_value_iterator * iter);
void so_value_iterator_free(struct so_value_iterator * iter);
struct so_value * so_value_iterator_get_key(struct so_value_iterator * iter, bool move_to_next, bool shared) __attribute__((warn_unused_result));
struct so_value * so_value_iterator_get_value(struct so_value_iterator * iter, bool shared);
bool so_value_iterator_has_next(struct so_value_iterator * iter);

#endif

