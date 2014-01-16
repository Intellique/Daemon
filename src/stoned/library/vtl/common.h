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
*  Last modified: Tue, 20 Aug 2013 15:22:23 +0200                            *
\****************************************************************************/

#ifndef __STONED_LIBRARY_VTL_H__
#define __STONED_LIBRARY_VTL_H__

// bool
#include <stdbool.h>
// timeval
#include <sys/time.h>

struct st_changer;
struct st_drive;
struct st_media_format;
struct st_ressource;
struct st_slot;
struct st_vtl_config;

struct st_vtl_changer {
	char * path;

	struct st_media ** medias;
	unsigned int nb_medias;

	struct st_ressource * lock;
};

struct st_vtl_drive {
	char * path;
	char * media_path;
	unsigned int file_position;
	struct st_media_format * format;
	struct timeval last_start;
};

struct st_vtl_media {
	char * path;
	struct st_slot * slot;
	char * prefix;
	bool used;
};

struct st_vtl_slot {
	char * path;
};

struct st_changer * st_vtl_changer_init(struct st_vtl_config * cfg);
void st_vtl_changer_sync(struct st_changer * ch, struct st_vtl_config * cfg);
void st_vtl_drive_init(struct st_drive * drive, struct st_slot * slot, char * base_dir, struct st_media_format * format);
struct st_media * st_vtl_media_init(char * base_dir, char * prefix, unsigned int index, struct st_media_format * format);
struct st_media * st_vtl_slot_get_media(struct st_changer * changer, const char * base_dir);
void st_vtl_slot_init(struct st_slot * sl);

#endif

