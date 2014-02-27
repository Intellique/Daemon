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

#ifndef __LIBSTONE_VALUE_H__
#define __LIBSTONE_VALUE_H__

// bool
#include <stdbool.h>

struct st_value;

/**
 * \brief Used to free custom value
 *
 * \param[in] value value to release
 */
typedef void (*st_value_free_f)(void * value);

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
typedef unsigned long long (*st_value_hashtable_compupte_hash_f)(const struct st_value * key);

/**
 * \brief Wrapper of value
 *
 * This structure can contain differents kind of value. Each value can be shared.
 */
struct st_value {
        /**
         * \enum st_value_type
         * \brief Type of value
         */
        enum st_value_type {
                /**
                 * \brief Used to store an array list
                 */
                st_value_array,
                /**
                 * \brief Used to store a boolean value
                 */
                st_value_boolean,
                /**
                 * \brief Used to store a custom value
                 */
                st_value_custom,
                /**
                 * \brief Used to store a floating value
                 */
                st_value_float,
                /**
                 * \brief Used to store an hashtable
                 */
                st_value_hashtable,
                /**
                 * \brief Used to store an integer value
                 */
                st_value_integer,
                /**
                 * \brief Used to store a linked list
                 */
                st_value_linked_list,
                /**
                 * \brief Used to store a null value
                 */
                st_value_null,
                /**
                 * \brief Used to store a string value
                 */
                st_value_string,
        } type;
        union {
                bool boolean;
                struct st_value_array {
                        struct st_value ** values;
                        unsigned int nb_vals, nb_preallocated;
                } array;
                long long int integer;
                double floating;
                struct st_value_hashtable {
                        struct st_value_hashtable_node {
                                unsigned long long hash;
                                struct st_value * key;
                                struct st_value * value;
                                struct st_value_hashtable_node * next;
                        } ** nodes;
                        unsigned int nb_elements;
                        unsigned int size_node;

                        bool allow_rehash;

                        st_value_hashtable_compupte_hash_f compute_hash;
                } hashtable;
                struct st_value_linked_list {
                        struct st_value_linked_list_node {
                                struct st_value * value;
                                struct st_value_linked_list_node * next;
                                struct st_value_linked_list_node * previous;
                        } * first, * last;
                        unsigned int nb_vals;
                } list;
                char * string;
                void * custom;
        } value;
        /**
         * \brief Number of many times this value is shared
         *
         * \attention Should not be modify
         */
        unsigned int shared;
        /**
         * \brief Function used to release custom value
         */
        st_value_free_f release;
} __attribute__((packed));

struct st_value_iterator {
        struct st_value * value;
        union {
                unsigned int array_index;
                struct st_value_iterator_hashtable {
                        struct st_value_hashtable_node * node;
                        unsigned int i_elements;
                } hashtable;
                struct st_value_linked_list_node * list_node;
        } data;
};

/**
 * \brief test if \a val can be convert into type \a type
 *
 * \param[in] val test this value
 * \param[in] type type of conversion
 * \returns \b true if \a val can be converted into \a type
 */
bool st_value_can_convert(struct st_value * val, enum st_value_type type) __attribute__((nonnull));
/**
 * \brief Convert \a val into \a type
 *
 * Before call this function, call st_value_can_convert to check if convertion is posible.
 *
 * \param[in] val value to convert
 * \param[in] type type of conversion
 * \returns new value
 * \note if conversion failed, it returns a st_value which represent a null value
 * \see st_value_convert
 */
struct st_value * st_value_convert(struct st_value * val, enum st_value_type type) __attribute__((nonnull,warn_unused_result));
/**
 * \brief Copy \a val into new value
 *
 * \param[in] val val to copy
 * \param[in] deep_copy copy recursively \a val
 * \returns a copy of \a val
 */
struct st_value * st_value_copy(struct st_value * val, bool deep_copy) __attribute__((nonnull,warn_unused_result));
/**
 * \brief Test if \a a and \a b are equals
 *
 * \param[in] a a value
 * \param[in] b a value
 * \returns \b true if \a a and \a b are equals
 */
bool st_value_equals(struct st_value * a, struct st_value * b) __attribute__((nonnull));
/**
 * \brief Release value
 *
 * Release value if \a value is not shared.
 *
 * \param[in] value release this \a value
 */
void st_value_free(struct st_value * value) __attribute__((nonnull));
/**
 * \brief Create new array list
 *
 * \param[in] size : pre allocate \a size elements into array
 * \returns new array value
 * \note if \a size is equals to \b 0, pre allocation will be <b>16 elements</b>
 */
struct st_value * st_value_new_array(unsigned int size) __attribute__((warn_unused_result));
/**
 * \brief Create new boolean value
 *
 * \param[in] value value of boolean type
 * \returns new boolean value
 */
struct st_value * st_value_new_boolean(bool value) __attribute__((warn_unused_result));
/**
 * \brief Create new custom value
 *
 * Custom value is used to store value which can not be stored otherwise.
 *
 * \param[in] value address of custom value.
 * \param[in] release function used to release custom value. \a release can be NULL
 * \returns new custom value
 */
struct st_value * st_value_new_custom(void * value, st_value_free_f release) __attribute__((warn_unused_result));
/**
 * \brief Create new floating value
 *
 * \param[in] value a floating type
 * \returns new floating type value
 */
struct st_value * st_value_new_float(double value) __attribute__((warn_unused_result));
/**
 * \brief Create new hashtable
 *
 * \param[in] compute_hash function used to compute hash from key
 * \returns new hashtable value
 */
struct st_value * st_value_new_hashtable(st_value_hashtable_compupte_hash_f compute_hash) __attribute__((nonnull,warn_unused_result));
/**
 * \brief Create new integer value
 *
 * \param[in] value value of integer type
 * \returns new integer type value
 */
struct st_value * st_value_new_integer(long long int value) __attribute__((warn_unused_result));
/**
 * \brief Create new linked list
 *
 * \returns new linked list
 */
struct st_value * st_value_new_linked_list(void) __attribute__((warn_unused_result));
/**
 * \brief Get null value
 *
 * \returns a null value represented by a st_value
 */
struct st_value * st_value_new_null(void) __attribute__((warn_unused_result));
/**
 * \brief Create new string value type
 *
 * \param[in] value a string value which will be duplicated
 * \returns new string value
 */
struct st_value * st_value_new_string(const char * value) __attribute__((nonnull,warn_unused_result));
/**
 * \brief Share this value
 *
 * \param[in] value value be be sharred
 * \returns sharred value
 */
struct st_value * st_value_share(struct st_value * value) __attribute__((nonnull,warn_unused_result));

/**
 * \brief remove all elements from hashtable
 *
 * \param[in] hash an hashtable value
 */
void st_value_hashtable_clear(struct st_value * hash) __attribute__((nonnull));
/**
 * \brief Get an element from hashtable with a key
 *
 * \param[in] hash an hashtable value
 * \param[in] key key associated to one value stored into hashtable
 * \param[in] detach if \b true then remove also value from hashtable
 * \returns \b null or the value associated to \a key
 */
struct st_value * st_value_hashtable_get(struct st_value * hash, struct st_value * key, bool detach) __attribute__((nonnull,warn_unused_result));
struct st_value * st_value_hashtable_get2(struct st_value * hash, const char * key) __attribute__((nonnull,warn_unused_result));
/**
 * \brief Get an object to iter through an hashtable
 *
 * \param[in] hash iter from this hashtable
 * \return an iterator
 */
struct st_value_iterator * st_value_hashtable_get_iterator(struct st_value * hash) __attribute__((nonnull,warn_unused_result));
/**
 * \brief Check if \a key is present into hashtable
 *
 * \param[in] hash an hashtable value
 * \param[in] key check this \a key
 * \return \b true if \a key is found
 */
bool st_value_hashtable_has_key(struct st_value * hash, struct st_value * key) __attribute__((nonnull));
/**
 * \brief Get a list of keys presents into hashtable
 *
 * \param[in] hash an hashtable value
 * \return an array list of keys of \a hash
 */
struct st_value * st_value_hashtable_keys(struct st_value * hash) __attribute__((nonnull));
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
void st_value_hashtable_put(struct st_value * hash, struct st_value * key, bool new_key, struct st_value * value, bool new_value) __attribute__((nonnull));
/**
 * \brief Remove value from hashtable
 *
 * \param[in] hash an hashtable value
 * \param[in] key remove a value associated to this \a key
 */
void st_value_hashtable_remove(struct st_value * hash, struct st_value * key) __attribute__((nonnull));
/**
 * \brief Get a list of all values
 *
 * \param[in] hash an hashtable value
 * \return an array list of values of \a hash
 */
struct st_value * st_value_hashtable_values(struct st_value * hash) __attribute__((nonnull));

void st_value_list_clear(struct st_value * list) __attribute__((nonnull));
struct st_value_iterator * st_value_list_get_iterator(struct st_value * list) __attribute__((nonnull,warn_unused_result));
struct st_value * st_value_list_pop(struct st_value * list) __attribute__((nonnull,warn_unused_result));
bool st_value_list_push(struct st_value * list, struct st_value * val, bool new_val) __attribute__((nonnull));
bool st_value_list_shift(struct st_value * list, struct st_value * val, bool new_val) __attribute__((nonnull));
struct st_value * st_value_list_unshift(struct st_value * list) __attribute__((nonnull,warn_unused_result));

void st_value_iterator_free(struct st_value_iterator * iter) __attribute__((nonnull));
struct st_value * st_value_iterator_get_key(struct st_value_iterator * iter, bool move_to_next, bool shared) __attribute__((nonnull,warn_unused_result));
struct st_value * st_value_iterator_get_value(struct st_value_iterator * iter, bool shared) __attribute__((nonnull,warn_unused_result));
bool st_value_iterator_has_next(struct st_value_iterator * iter) __attribute__((nonnull));

#endif

