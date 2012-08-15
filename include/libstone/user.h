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
*  Last modified: Wed, 15 Aug 2012 14:13:57 +0200                         *
\*************************************************************************/

#ifndef __STONE_USER_H__
#define __STONE_USER_H__

struct st_database_connection;
struct st_pool;

struct st_user {
	char * login;
	char * password;
	char * salt;
	char * fullname;
	char * email;

	unsigned char is_admin;
	unsigned char can_archive;
	unsigned char can_restore;
	unsigned char disabled;

	struct st_pool * pool;

	void * db_data;
};

struct st_user * st_user_get(const char * login);
int st_user_sync(struct st_user * user);

#endif

