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
*  Copyright (C) 2013, Clercin guillaume <gclercin@intellique.com>           *
*  Last modified: Fri, 16 Aug 2013 13:55:42 +0200                            *
\****************************************************************************/

#ifndef __STONE_LIBRARY_VTL_H__
#define __STONE_LIBRARY_VTL_H__

// bool
#include <stdbool.h>

struct st_changer;

struct st_vtl_config {
	char * path;
	char * prefix;
	unsigned int nb_slots;
	unsigned int nb_drives;
	struct st_media_format * format;
	bool deleted;
};

struct st_vtl_list {
	struct st_changer * changer;
	struct st_vtl_list * next;
};

void st_vtl_config_free(struct st_vtl_config * config, unsigned int nb_config);

#endif

