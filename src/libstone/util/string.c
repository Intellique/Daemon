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

// NULL
#include <stddef.h>
// strlen
#include <string.h>

#include "string.h"
#include "value.h"

__asm__(".symver st_util_string_compute_hash_v1, st_util_string_compute_hash@@LIBSTONE_1.0");
unsigned long long st_util_string_compute_hash_v1(const struct st_value * value) {
	if (value == NULL || value->type != st_value_string)
		return 0;

	unsigned long long hash = 0;
	int length = strlen(value->value.string), i;
	for (i = 0; i < length; i++)
		hash = value->value.string[i] + (hash << 6) + (hash << 16) - hash;
	return hash;
}

