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
*  Copyright (C) 2010, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Tue, 28 Sep 2010 18:06:25 +0200                       *
\***********************************************************************/

#ifndef __STORIQARCHIVER_HASHTABLE_H__
#define __STORIQARCHIVER_HASHTABLE_H__

struct hashtableNode {
	unsigned long long hash;
	void * key;
	void * value;
	struct hashtableNode * next;
};

struct hashtable {
	struct hashtableNode ** nodes;
	unsigned int nbElements;
	unsigned int sizeNode;

	unsigned char allowRehash;

	unsigned long long (*computeHash)(const void *);
	void (*releaseKeyValue)(void *, void *);
};

void hashtable_clear(struct hashtable * hashtable);
void hashtable_free(struct hashtable * hashtable);
short hashtable_hasKey(struct hashtable * hashtable, const void * key);
const void ** hashtable_keys(struct hashtable * hashtable);
struct hashtable * hashtable_new(unsigned long long (*computeHash)(const void * key));
struct hashtable * hashtable_new2(unsigned long long (*computeHash)(const void * key), void (*releaseKeyValue)(void * key, void * value));
void hashtable_put(struct hashtable * hashtable, void * key, void * value);
void * hashtable_remove(struct hashtable * hashtable, const void * key);
void * hashtable_value(struct hashtable * hashtable, const void * key);
void ** hashtable_values(struct hashtable * hashtable);

#endif

