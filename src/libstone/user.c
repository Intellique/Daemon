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
*  Last modified: Thu, 16 Aug 2012 22:36:19 +0200                         *
\*************************************************************************/

// pthread_mutex_lock, pthread_mutex_unlock
#include <pthread.h>
// free, malloc, realloc
#include <stdlib.h>
// strcmp
#include <string.h>
// bzero
#include <strings.h>

#include <libstone/database.h>
#include <libstone/log.h>
#include <libstone/user.h>

static struct st_user ** st_users = NULL;
static unsigned int st_user_nb_users = 0;

struct st_user * st_user_get(const char * login) {
	if (login == NULL)
		return NULL;

	static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_lock(&lock);

	struct st_user * user = NULL;
	unsigned int i;
	for (i = 0; !user && i < st_user_nb_users; i++)
		if (login && !strcmp(login, st_users[i]->login))
			user = st_users[i];

	if (user == NULL) {
		struct st_database * db = st_database_get_default_driver();
		struct st_database_config * config = NULL;
		struct st_database_connection * connect = NULL;

		if (db != NULL)
			config = db->ops->get_default_config();

		if (config != NULL)
			connect = config->ops->connect(config);

		if (connect != NULL) {
			struct st_user user2;
			bzero(&user2, sizeof(struct st_user));

			if (!connect->ops->get_user(connect, &user2, login)) {
				void * new_addr = realloc(st_users, (st_user_nb_users + 1) * sizeof(struct st_user *));
				if (new_addr != NULL) {
					st_users = new_addr;
					user = st_users[st_user_nb_users] = malloc(sizeof(struct st_user));
					memcpy(user, &user2, sizeof(struct st_user));
				} else {
					free(user2.login);
					free(user2.password);
					free(user2.salt);
					free(user2.fullname);
					free(user2.email);
					free(user2.db_data);

					st_log_write_all(st_log_level_info, st_log_type_daemon, "Not enough memory to get user '%s'", login);
				}
			}

			connect->ops->close(connect);
			connect->ops->free(connect);
		}
	}

	pthread_mutex_unlock(&lock);

	return user;
}

int st_user_sync(struct st_user * user) {
	if (user == NULL)
		return 0;

	struct st_database * db = st_database_get_default_driver();
	struct st_database_config * config = NULL;
	struct st_database_connection * connect = NULL;

	if (db != NULL)
		config = db->ops->get_default_config();

	if (config != NULL)
		connect = config->ops->connect(config);

	int ok = 0;
	if (connect != NULL) {
		ok = connect->ops->sync_user(connect, user);

		connect->ops->close(connect);
		connect->ops->free(connect);
	}

	return ok;
}

