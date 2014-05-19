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
*  Last modified: Tue, 18 Feb 2014 15:56:52 +0100                            *
\****************************************************************************/

#ifndef __STONE_BACKUP_H__
#define __STONE_BACKUP_H__

// time_t
#include <sys/time.h>

struct st_media;

struct st_backup {
	time_t timestamp;
	long nb_medias;
	long nb_archives;

	struct st_backup_volume {
		struct st_media * media;
		unsigned int position;
		struct st_job * job;
	} * volumes;
	unsigned int nb_volumes;
};

void st_backup_add_volume(struct st_backup * backup, struct st_media * media, unsigned int position, struct st_job * job);
void st_backup_free(struct st_backup * backup);
struct st_backup * st_backup_new(void);

#endif
