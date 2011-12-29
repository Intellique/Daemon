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
*  Last modified: Thu, 29 Dec 2011 19:44:04 +0100                         *
\*************************************************************************/

// pthread_mutex_lock, pthread_mutex_unlock
#include <pthread.h>
// free, malloc, realloc
#include <stdlib.h>
// strcmp
#include <string.h>

#include <stone/database.h>
#include <stone/user.h>

static struct st_user ** st_user_users = 0;
static unsigned int st_user_nb_users = 0;

struct st_user * st_user_get(long id, const char * login) {
	if (id < 0 && !login)
		return 0;

	int old_state;
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &old_state);
	static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_lock(&lock);

	struct st_user * user = 0;
	unsigned int i;
	for (i = 0; !user && i < st_user_nb_users; i++)
		if (id == st_user_users[i]->id || (login && !strcmp(login, st_user_users[i]->login)))
			user = st_user_users[i];

	if (!user) {
		struct st_database * db = st_db_get_default_db();
		struct st_database_connection * con = db->ops->connect(db, 0);

		if (con) {
			st_user_users = realloc(st_user_users, (st_user_nb_users + 1) * sizeof(struct st_user *));
			st_user_users[st_user_nb_users] = malloc(sizeof(struct st_user));

			if (!con->ops->get_user(con, st_user_users[st_user_nb_users], id, login)) {
				user = st_user_users[st_user_nb_users];
				st_user_nb_users++;
			} else {
				free(st_user_users[st_user_nb_users]);
				st_user_users = realloc(st_user_users, st_user_nb_users * sizeof(struct st_user *));
			}
		}
	}

	pthread_mutex_unlock(&lock);
	pthread_setcancelstate(old_state, 0);

	return user;
}

int st_user_sync(struct st_user * user) {
	if (!user)
		return 0;

	struct st_database * db = st_db_get_default_db();
	struct st_database_connection * con = db->ops->connect(db, 0);

	int ok = con->ops->sync_user(con, user);

	con->ops->close(con);
	con->ops->free(con);
	free(con);

	return ok;
}

