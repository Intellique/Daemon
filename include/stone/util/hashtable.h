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
*  Last modified: Wed, 04 Jul 2012 10:30:54 +0200                         *
\*************************************************************************/

#ifndef __STONE_UTIL_HASHTABLE_H__
#define __STONE_UTIL_HASHTABLE_H__

/**
 * \struct st_hashtable
 * \brief This is a hashtable
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
	unsigned int size_node;

	unsigned char allow_rehash;

	unsigned long long (*compute_hash)(const void *);
	void (*release_key_value)(void *, void *);
};

/**
 * \brief Remove (and release) all elements in hashtable
 */
void st_hashtable_clear(struct st_hashtable * hashtable);
/**
 * \brief Release a hashtable
 */
void st_hashtable_free(struct st_hashtable * hashtable);
/**
 * \brief Check if hashtable contains this key
 *
 * \param[in] hashtable : a hashtable
 * \param[in] key : a key
 * \returns 1 if found otherwise 0
 */
short st_hashtable_has_key(struct st_hashtable * hashtable, const void * key);
const void ** st_hashtable_keys(struct st_hashtable * hashtable);
struct st_hashtable * st_hashtable_new(unsigned long long (*compute_hash)(const void * key));
struct st_hashtable * st_hashtable_new2(unsigned long long (*compute_hash)(const void * key), void (*release_key_value)(void * key, void * value));
void st_hashtable_put(struct st_hashtable * hashtable, void * key, void * value);
void * st_hashtable_remove(struct st_hashtable * hashtable, const void * key);
void * st_hashtable_value(struct st_hashtable * hashtable, const void * key);
void ** st_hashtable_values(struct st_hashtable * hashtable);

#endif

