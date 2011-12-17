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
*  Last modified: Sat, 17 Dec 2011 17:36:46 +0100                         *
\*************************************************************************/

// free
#include <malloc.h>
// strlen
#include <string.h>

#include "util.h"

void sa_util_free_key_value(void * key, void * value) {
	if (key)
		free(key);
	if (value)
		free(value);
}

/**
 * sdbm function
 */
unsigned long long sa_util_hash_string(const void * key) {
	const char * cstr = key;
	unsigned long long int hash = 0;
	int length = strlen(cstr), i;
	for (i = 0; i < length; i++)
		hash = cstr[i] + (hash << 6) + (hash << 16) - hash;
	return hash;
}

