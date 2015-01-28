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
*  Copyright (C) 2013-2015, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Mon, 11 Feb 2013 13:49:53 +0100                            *
\****************************************************************************/

#ifndef __STONE_LIBRARY_RESSOURCE_H__
#define __STONE_LIBRARY_RESSOURCE_H__

// bool
#include <stdbool.h>

struct st_ressource {
	struct st_ressource_ops {
		int (*free)(struct st_ressource * res);
		int (*lock)(struct st_ressource * res);
		int (*timed_lock)(struct st_ressource *res, unsigned int timeout);
		int (*try_lock)(struct st_ressource * res);
		void (*unlock)(struct st_ressource * res);
	} * ops;
	void * data;
	int locked;
};

int st_ressource_lock(int nb_res, struct st_ressource * res1, struct st_ressource * res2, ...);
struct st_ressource * st_ressource_new(bool recursive_lock);

#endif

