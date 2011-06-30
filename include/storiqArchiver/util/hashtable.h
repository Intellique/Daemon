/***********************************************************************\
*       ______           ____     ___           __   _                  *
*      / __/ /____  ____/  _/__ _/ _ | ________/ /  (_)  _____ ____     *
*     _\ \/ __/ _ \/ __// // _ `/ __ |/ __/ __/ _ \/ / |/ / -_) __/     *
*    /___/\__/\___/_/ /___/\_, /_/ |_/_/  \__/_//_/_/|___/\__/_/        *
*                           /_/                                         *
*  -------------------------------------------------------------------  *
*  This file is a part of StorIqArchiver                                *
*                                                                       *
*  StorIqArchiver is free software; you can redistribute it and/or      *
*  modify it under the terms of the GNU General Public License          *
*  as published by the Free Software Foundation; either version 3       *
*  of the License, or (at your option) any later version.               *
*                                                                       *
*  This program is distributed in the hope that it will be useful,      *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of       *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
*  GNU General Public License for more details.                         *
*                                                                       *
*  You should have received a copy of the GNU General Public License    *
*  along with this program; if not, write to the Free Software          *
*  Foundation, Inc., 51 Franklin Street, Fifth Floor,                   *
*  Boston, MA  02110-1301, USA.                                         *
*                                                                       *
*  -------------------------------------------------------------------  *
*  Copyright (C) 2011, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Thu, 30 Jun 2011 22:29:03 +0200                       *
\***********************************************************************/

#ifndef __STORIQARCHIVER_HASHTABLE_H__
#define __STORIQARCHIVER_HASHTABLE_H__

struct sa_hashtable_node {
	unsigned long long hash;
	void * key;
	void * value;
	struct sa_hashtable_node * next;
};

struct sa_hashtable {
	struct sa_hashtable_node ** nodes;
	unsigned int nbElements;
	unsigned int sizeNode;

	unsigned char allowRehash;

	unsigned long long (*computeHash)(const void *);
	void (*releaseKeyValue)(void *, void *);
};

void sa_hashtable_clear(struct sa_hashtable * hashtable);
void sa_hashtable_free(struct sa_hashtable * hashtable);
short sa_hashtable_hasKey(struct sa_hashtable * hashtable, const void * key);
const void ** sa_hashtable_keys(struct sa_hashtable * hashtable);
struct sa_hashtable * sa_hashtable_new(unsigned long long (*computeHash)(const void * key));
struct sa_hashtable * sa_hashtable_new2(unsigned long long (*computeHash)(const void * key), void (*releaseKeyValue)(void * key, void * value));
void sa_hashtable_put(struct sa_hashtable * hashtable, void * key, void * value);
void * sa_hashtable_remove(struct sa_hashtable * hashtable, const void * key);
void * sa_hashtable_value(struct sa_hashtable * hashtable, const void * key);
void ** sa_hashtable_values(struct sa_hashtable * hashtable);

#endif

