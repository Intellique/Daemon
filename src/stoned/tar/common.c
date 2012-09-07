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
*  Copyright (C) 2012, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Thu, 06 Sep 2012 17:54:51 +0200                         *
\*************************************************************************/

// free
#include <stdlib.h>
// bzero
#include <string.h>

#include "common.h"

void st_tar_free_header(struct st_tar_header * h) {
	if (!h)
		return;

	if (h->path)
		free(h->path);
	h->path = h->filename = 0;

	if (h->link)
		free(h->link);
	h->link = 0;
}

void st_tar_init_header(struct st_tar_header * h) {
	if (!h)
		return;

	bzero(h, sizeof(struct st_tar_header));
}

