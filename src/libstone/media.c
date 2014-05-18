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

// free
#include <stdlib.h>
// strcasecmp
#include <string.h>

#include "media.h"

static const struct st_media_format_mode2 {
	char * name;
	enum st_media_format_mode mode;
} st_media_format_modes[] = {
	{ "disk",    st_media_format_mode_disk },
	{ "linear",  st_media_format_mode_linear },
	{ "optical", st_media_format_mode_optical },

	{ "unknown", st_media_format_mode_unknown },
};


__asm__(".symver st_media_format_mode_to_string_v1, st_media_format_mode_to_string@@LIBSTONE_1.2");
const char * st_media_format_mode_to_string_v1(enum st_media_format_mode mode) {
	unsigned int i;
	for (i = 0; st_media_format_modes[i].mode != st_media_format_mode_unknown; i++)
		if (st_media_format_modes[i].mode == mode)
			return st_media_format_modes[i].name;

	return st_media_format_modes[i].name;
}

__asm__(".symver st_media_string_to_format_mode_v1, st_media_string_to_format_mode@@LIBSTONE_1.2");
enum st_media_format_mode st_media_string_to_format_mode_v1(const char * mode) {
	if (mode == NULL)
		return st_media_format_mode_unknown;

	unsigned int i;
	for (i = 0; st_media_format_modes[i].mode != st_media_format_mode_unknown; i++)
		if (!strcasecmp(mode, st_media_format_modes[i].name))
			return st_media_format_modes[i].mode;

	return st_media_format_modes[i].mode;
}

