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

// free, realloc
#include <stdlib.h>
// memmove
#include <string.h>
// close
#include <unistd.h>
// time
#include <time.h>

#include <libstone/time.h>

#include "poll.h"

static struct pollfd * st_polls = NULL;
static struct st_poll_info {
	st_poll_callback_f_v1 callback;
	void * data;

	int timeout;
	struct timespec limit;
	st_poll_timeout_f_v1 timeout_callback;

	st_poll_free_f_v1 release;
} * st_poll_infos = NULL;
static unsigned int st_nb_polls = 0;
static bool st_poll_restart = false;

static void st_poll_exit(void) __attribute__((destructor));


static void st_poll_exit() {
	free(st_polls);
}

__asm__(".symver st_poll_v1, st_poll@@LIBSTONE_1.2");
int st_poll_v1(int timeout) {
	if (st_nb_polls < 1)
		return 0;

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	unsigned int i;
	for (i = 0; i < st_nb_polls; i++) {
		st_polls[i].revents = 0;

		if (st_poll_infos[i].timeout > 0) {
			if (st_time_cmp(&now, &st_poll_infos[i].limit) >= 0)
				st_poll_infos[i].timeout_callback(st_polls[i].fd, st_poll_infos[i].data);
			else if (timeout < 0)
				timeout = st_time_diff(&st_poll_infos[i].limit, &now) / 1000000;
			else {
				struct timespec future = {
					.tv_sec = now.tv_sec + timeout / 1000,
					.tv_nsec = now.tv_nsec + 1000000 * (timeout % 1000)
				};
				st_time_fix(&future);

				if (st_time_cmp(&future, &st_poll_infos[i].limit) > 0)
					timeout = st_time_diff(&st_poll_infos[i].limit, &now) / 1000000;
			}
		}
	}

	int nb_event = poll(st_polls, st_nb_polls, timeout);

	clock_gettime(CLOCK_MONOTONIC, &now);

	for (i = 0; i < st_nb_polls; i++) {
		int fd = st_polls[i].fd;
		short event = st_polls[i].revents;
		st_polls[i].revents = 0;

		if (event != 0) {
			st_poll_infos[i].callback(fd, event, st_poll_infos[i].data);

			if (st_poll_restart) {
				i = -1;
				st_poll_restart = false;
				continue;
			} else if (st_poll_infos[i].timeout > 0) {
				st_poll_infos[i].limit = now;
				st_poll_infos[i].limit.tv_sec += st_poll_infos[i].timeout / 1000;
				st_poll_infos[i].limit.tv_nsec += 1000000 * (st_poll_infos[i].timeout % 1000);
				st_time_fix(&st_poll_infos[i].limit);
			}
		} else if (st_poll_infos[i].timeout > 0 && st_time_cmp(&now, &st_poll_infos[i].limit) >= 0)
			st_poll_infos[i].timeout_callback(st_polls[i].fd, st_poll_infos[i].data);

		if (st_poll_restart) {
			i = -1;
			st_poll_restart = false;
		} else if ((fd == st_polls[i].fd) && (event & POLLHUP)) {
			st_poll_unregister_v1(fd);
			close(fd);

			st_poll_restart = false;
			i--;
		}
	}

	return nb_event;
}

__asm__(".symver st_poll_nb_handlers_v1, st_poll_nb_handlers@@LIBSTONE_1.2");
unsigned int st_poll_nb_handlers_v1() {
	return st_nb_polls;
}

__asm__(".symver st_poll_register_v1, st_poll_register@@LIBSTONE_1.2");
bool st_poll_register_v1(int fd, short event, st_poll_callback_f_v1 callback, void * data, st_poll_free_f_v1 release) {
	if (fd < 0 || callback == NULL)
		return false;

	void * addr = realloc(st_polls, (st_nb_polls + 1) * sizeof(struct pollfd));
	if (addr == NULL)
		return false;
	st_polls = addr;

	addr = realloc(st_poll_infos, (st_nb_polls + 1) * sizeof(struct st_poll_info));
	if (addr == NULL)
		return false;
	st_poll_infos = addr;

	struct pollfd * new_fd = st_polls + st_nb_polls;
	new_fd->fd = fd;
	new_fd->events = event;
	new_fd->revents = 0;

	struct st_poll_info * new_info = st_poll_infos + st_nb_polls;
	new_info->callback = callback;
	new_info->data = data;
	new_info->timeout = -1;
	new_info->limit.tv_sec = 0;
	new_info->limit.tv_nsec = 0;
	new_info->timeout_callback = NULL;
	new_info->release = release;

	st_nb_polls++;

	return true;
}

__asm__(".symver st_poll_set_timeout_v1, st_poll_set_timeout@@LIBSTONE_1.2");
bool st_poll_set_timeout_v1(int fd, int timeout, st_poll_timeout_f_v1 callback) {
	if (fd < 0 || timeout < 1 || callback == NULL)
		return false;

	unsigned int i;
	for (i = 0; i < st_nb_polls; i++) {
		if (st_polls[i].fd == fd) {
			struct st_poll_info * info = st_poll_infos + i;

			info->timeout = timeout;
			clock_gettime(CLOCK_MONOTONIC, &info->limit);
			info->timeout_callback = callback;

			info->limit.tv_sec += timeout / 1000;
			info->limit.tv_nsec += 1000000 * (timeout % 1000);
			st_time_fix(&info->limit);

			return true;
		}
	}

	return false;
}

__asm__(".symver st_poll_unregister_v1, st_poll_unregister@@LIBSTONE_1.2");
void st_poll_unregister_v1(int fd) {
	if (fd < 0)
		return;

	unsigned int i;
	for (i = 0; i < st_nb_polls; i++)
		if (st_polls[i].fd == fd)
			break;

	if (i == st_nb_polls)
		return;

	struct st_poll_info * new_info = st_poll_infos + i;
	if (new_info->data != NULL && new_info->release != NULL)
		new_info->release(new_info->data);

	if (i + 1 < st_nb_polls) {
		size_t nb_move = st_nb_polls - i - 1;
		memmove(st_polls + i, st_polls + (i + 1), nb_move * sizeof(struct pollfd));
		memmove(st_poll_infos + i, st_poll_infos + (i + 1), nb_move * sizeof(struct st_poll_info));
	}

	st_nb_polls--;

	void * addr = realloc(st_polls, st_nb_polls * sizeof(struct pollfd));
	if (addr != NULL)
		st_polls = addr;

	addr = realloc(st_poll_infos, st_nb_polls * sizeof(struct st_poll_info));
	if (addr != NULL)
		st_poll_infos = addr;

	st_poll_restart = true;
}

