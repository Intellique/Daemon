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
*  Last modified: Sat, 17 Dec 2011 19:35:41 +0100                         *
\*************************************************************************/

#ifndef __STONE_HASHTABLE_H__
#define __STONE_HASHTABLE_H__

struct st_hashtable_node {
	unsigned long long hash;
	void * key;
	void * value;
	struct st_hashtable_node * next;
};

struct st_hashtable {
	struct st_hashtable_node ** nodes;
	unsigned int nb_elements;
	unsigned int size_node;

	unsigned char allow_rehash;

	unsigned long long (*compute_hash)(const void *);
	void (*release_key_value)(void *, void *);
};

void st_hashtable_clear(struct st_hashtable * hashtable);
void st_hashtable_free(struct st_hashtable * hashtable);
short st_hashtable_has_key(struct st_hashtable * hashtable, const void * key);
const void ** st_hashtable_keys(struct st_hashtable * hashtable);
struct st_hashtable * st_hashtable_new(unsigned long long (*compute_hash)(const void * key));
struct st_hashtable * st_hashtable_new2(unsigned long long (*compute_hash)(const void * key), void (*release_key_value)(void * key, void * value));
void st_hashtable_put(struct st_hashtable * hashtable, void * key, void * value);
void * st_hashtable_remove(struct st_hashtable * hashtable, const void * key);
void * st_hashtable_value(struct st_hashtable * hashtable, const void * key);
void ** st_hashtable_values(struct st_hashtable * hashtable);

#endif

