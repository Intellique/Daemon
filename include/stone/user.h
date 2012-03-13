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
*  Last modified: Tue, 13 Mar 2012 18:40:17 +0100                         *
\*************************************************************************/

#ifndef __STONE_USER_H__
#define __STONE_USER_H__

#include <sys/types.h>

struct st_user {
	long id;
	char * login;
	char * password;
	char * salt;
	char * fullname;
	char * email;
	char is_admin;
	char can_archive;
	char can_restore;
	int nb_connection;
	time_t last_connection;
	char disabled;
	struct st_pool * pool;
};

struct st_user * st_user_get(long id, const char * login);
int st_user_sync(struct st_user * user);

#endif

