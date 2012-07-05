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
*  Last modified: Thu, 05 Jul 2012 15:21:52 +0200                         *
\*************************************************************************/

#ifndef __STONE_UTIL_HASHTABLE_H__
#define __STONE_UTIL_HASHTABLE_H__

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
typedef unsigned long long (*st_compute_hash_f)(const void * key);

/**
 * \brief Function used by a hashtable to release a pair of key value
 *
 * \param[in] key : a \a key
 * \param[in] value : a \a value
 */
typedef void (*st_release_key_value_f)(void * key, void * value);

/**
 * \struct st_hashtable
 * \brief This is a hashtable
 *
 * A hashtable is a collection of pairs of keys values.
 */
struct st_hashtable {
	/**
	 * \struct st_hashtable_node
	 * \brief A node of one hashtable
	 */
	struct st_hashtable_node {
		/**
		 * \brief Hash value of key
		 */
		unsigned long long hash;
		void * key;
		void * value;
		struct st_hashtable_node * next;
	} ** nodes;
	/**
	 * \brief Numbers of elements in hashtable
	 */
	unsigned int nb_elements;
	/**
	 * \brief Only for internal use of hashtable
	 */
	unsigned int size_node;
	/**
	 * \brief Only for internal use of hashtable
	 */
	unsigned char allow_rehash;

	/**
	 * \brief Function used by hashtable for computing hash of one key
	 *
	 * \see st_compute_hash_f
	 */
	st_compute_hash_f compute_hash;
	/**
	 * \brief Function used by hashtable for releasing elements in hashtable
	 *
	 * \see st_release_key_value_f
	 */
	st_release_key_value_f release_key_value;
};

/**
 * \brief Remove (and release) all elements in hashtable
 *
 * \param[in] hashtable : a hashtable
 *
 * \see st_release_key_value_f
 */
void st_hashtable_clear(struct st_hashtable * hashtable);

/**
 * \brief Release memory associated to a hashtable
 *
 * \param[in] hashtable : a hashtable
 *
 * \see st_hashtable_clear
 */
void st_hashtable_free(struct st_hashtable * hashtable);

/**
 * \brief Check if this \a hashtable contains this key
 *
 * \param[in] hashtable : a hashtable
 * \param[in] key : a key
 * \returns 1 if found otherwise 0
 */
short st_hashtable_has_key(struct st_hashtable * hashtable, const void * key);

/**
 * \brief Gets keys from hashtable
 *
 * \param[in] hashtable : a hashtable
 * \returns a calloc allocated and null terminated array which contains keys
 *
 * \attention You should release the returned array with \a free but not keys
 * because keys are shared with hashtable
 */
const void ** st_hashtable_keys(struct st_hashtable * hashtable);

/**
 * \brief Create a new hashtable
 *
 * \param[in] ch : function used by hashtable to compute hash of keys
 * \returns a hashtable
 *
 * \attention You sould use a function that not contains known collisions.
 *
 * \see st_compute_hash_f
 */
struct st_hashtable * st_hashtable_new(st_compute_hash_f ch);

/**
 * \brief Create a new hashtable
 *
 * \param[in] ch : function used by hashtable to compute hash of keys
 * \param[in] release : function used by hashtable to release a pair of key and value
 * \returns a hashtable
 *
 * \see st_hashtable_new
 * \see st_release_key_value_f
 */
struct st_hashtable * st_hashtable_new2(st_compute_hash_f ch, st_release_key_value_f release);

/**
 * \brief Put a new pair of key value into this \a hashtable
 *
 * Compute hash of this key by using function compute_hash provide at creation of hashtable.\n
 * If a hash is already present (in this case, there is a collision), we release old pair of
 * key and value.\n In all cases, it adds new pair of key and value.
 *
 * \note Parameters \a hashtable and \a key are not supposed to be \b NULL so
 * st_hashtable_put does nothing in this case
 *
 * \note Release old value is done by using release_key_value function passed at creation of
 * hashtable
 *
 * \param[in] hashtable : a hashtable
 * \param[in] key : a key
 * \param[in] value : a value
 *
 * \see st_hashtable_new2
 */
void st_hashtable_put(struct st_hashtable * hashtable, void * key, void * value);

/**
 * \brief Remove a pair of key and value
 *
 * Compute hash of \a key to find a pair of key value. If the pair is no found,
 * this function does nothing.\n Otherwise it removes the founded pair and release
 * key and value if release_key_value was provided while creating this
 * \a hashtable by using st_hashtable_new2
 *
 * \param[in] hashtable : a hashtable
 * \param[in] key : a key
 *
 * \note Parameters \a hashtable and \a key are not supposed to be \b NULL so
 * st_hashtable_remove does nothing in this case
 *
 * \see st_hashtable_new2
 * \see st_hashtable_clear
 */
void st_hashtable_remove(struct st_hashtable * hashtable, const void * key);

/**
 * \brief Get a value associated with this \a key
 *
 * Compute hash of \a key to find a pair of key value. If the pair is no found,
 * this function returns \b NULL.\n Otherwise returns value of found pair.
 *
 * \param[in] hashtable : a hashtable
 * \param[in] key : a key
 * \returns \b 0 if there is no value associated with this \a key
 *
 * \note Parameters \a hashtable and \a key are not supposed to be \b NULL so
 * st_hashtable_value returns \b 0 if \a hashtable is \b NULL or \a key is \b NULL
 *
 * \attention You <b> SHOULD NOT RELEASE </b> the returned value because this value is
 * shared with this \a hashtable
 */
void * st_hashtable_value(struct st_hashtable * hashtable, const void * key);

/**
 * \brief Get all values of this \a hashtable
 *
 * \param[in] hashtable : a hashtable
 * \returns a calloc allocated and null terminated array which contains all values
 *
 * \note Parameter \a hashtable is not supposed to be \b NULL so st_hashtable_values
 * returns \b 0 if \a hashtable is \b NULL
 *
 * \attention You should release the returned array with \a free but not values
 * because values are shared with this \a hashtable
 *
 * \see st_hashtable_keys
 */
void ** st_hashtable_values(struct st_hashtable * hashtable);

#endif

