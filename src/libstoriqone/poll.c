/****************************************************************************\
*                    ______           _      ____                            *
*                   / __/ /____  ____(_)__ _/ __ \___  ___                   *
*                  _\ \/ __/ _ \/ __/ / _ `/ /_/ / _ \/ -_)                  *
*                 /___/\__/\___/_/ /_/\_, /\____/_//_/\__/                   *
*                                      /_/                                   *
*  ------------------------------------------------------------------------  *
*  This file is a part of Storiq One                                         *
*                                                                            *
*  Storiq One is free software; you can redistribute it and/or modify        *
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
*  Copyright (C) 2013-2016, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// free, realloc
#include <stdlib.h>
// memmove
#include <string.h>
// close
#include <unistd.h>
// time
#include <time.h>

#include <libstoriqone/poll.h>
#include <libstoriqone/time.h>

static struct pollfd * so_polls = NULL;
static struct so_poll_info {
	so_poll_callback_f callback;
	void * data;

	int timeout;
	struct timespec limit;
	so_poll_timeout_f timeout_callback;

	so_poll_free_f release;
} * so_poll_infos = NULL;
static unsigned int so_nb_used_polls = 0, so_nb_polls = 0;
static bool so_poll_restart = false;

static void so_poll_exit(void) __attribute__((destructor));


static void so_poll_exit() {
	free(so_polls);

	unsigned int i;
	for (i = 0; i < so_nb_used_polls; i++) {
		struct so_poll_info * info = so_poll_infos + i;

		if (info->release != NULL && info->data != NULL)
			info->release(info->data);
	}
	free(so_poll_infos);
}

int so_poll(int timeout) {
	if (so_nb_used_polls < 1)
		return 0;

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	unsigned int i;
	for (i = 0; i < so_nb_used_polls; i++) {
		so_polls[i].revents = 0;

		if (so_poll_infos[i].timeout > 0) {
			if (so_time_cmp(&now, &so_poll_infos[i].limit) >= 0) {
				so_poll_infos[i].timeout_callback(so_polls[i].fd, so_poll_infos[i].data);
				clock_gettime(CLOCK_MONOTONIC, &now);
			} else if (timeout < 0)
				timeout = so_time_diff(&so_poll_infos[i].limit, &now) / 1000000;
			else {
				struct timespec future = {
					.tv_sec = now.tv_sec + timeout / 1000,
					.tv_nsec = now.tv_nsec + 1000000 * (timeout % 1000)
				};
				so_time_fix(&future);

				if (so_time_cmp(&future, &so_poll_infos[i].limit) > 0)
					timeout = so_time_diff(&so_poll_infos[i].limit, &now) / 1000000;
			}
		}
	}

	int nb_event = poll(so_polls, so_nb_used_polls, timeout);

	clock_gettime(CLOCK_MONOTONIC, &now);

	for (i = 0; i < so_nb_used_polls; i++) {
		int fd = so_polls[i].fd;
		short event = so_polls[i].revents;
		so_polls[i].revents = 0;

		if (event == POLLNVAL) {
			so_poll_unregister(fd, so_polls[i].events);
		} else if (event != 0) {
			so_poll_infos[i].callback(fd, event, so_poll_infos[i].data);

			if (so_poll_restart) {
				i = -1;
				so_poll_restart = false;
				continue;
			} else if (so_poll_infos[i].timeout > 0) {
				so_poll_infos[i].limit = now;
				so_poll_infos[i].limit.tv_sec += so_poll_infos[i].timeout / 1000;
				so_poll_infos[i].limit.tv_nsec += 1000000 * (so_poll_infos[i].timeout % 1000);
				so_time_fix(&so_poll_infos[i].limit);
			}
		} else if (so_poll_infos[i].timeout > 0 && so_time_cmp(&now, &so_poll_infos[i].limit) >= 0)
			so_poll_infos[i].timeout_callback(so_polls[i].fd, so_poll_infos[i].data);

		if (so_poll_restart) {
			i = -1;
			so_poll_restart = false;
		} else if ((fd == so_polls[i].fd) && (event & POLLHUP)) {
			so_poll_unregister(fd, so_polls[i].events);
			close(fd);

			so_poll_restart = false;
			i--;
		}
	}

	return nb_event;
}

unsigned int so_poll_nb_handlers() {
	return so_nb_used_polls;
}

bool so_poll_register(int fd, short event, so_poll_callback_f callback, void * data, so_poll_free_f release) {
	if (fd < 0 || callback == NULL)
		return false;

	if (so_nb_used_polls == so_nb_polls) {
		void * addr = realloc(so_polls, (so_nb_polls + 4) * sizeof(struct pollfd));
		if (addr == NULL)
			return false;
		so_polls = addr;

		addr = realloc(so_poll_infos, (so_nb_polls + 4) * sizeof(struct so_poll_info));
		if (addr == NULL)
			return false;
		so_poll_infos = addr;

		so_nb_polls += 4;
	}

	struct pollfd * new_fd = so_polls + so_nb_used_polls;
	new_fd->fd = fd;
	new_fd->events = event;
	new_fd->revents = 0;

	struct so_poll_info * new_info = so_poll_infos + so_nb_used_polls;
	new_info->callback = callback;
	new_info->data = data;
	new_info->timeout = -1;
	new_info->limit.tv_sec = 0;
	new_info->limit.tv_nsec = 0;
	new_info->timeout_callback = NULL;
	new_info->release = release;

	so_nb_used_polls++;

	return true;
}

bool so_poll_set_timeout(int fd, int timeout, so_poll_timeout_f callback) {
	if (fd < 0 || timeout < 1 || callback == NULL)
		return false;

	unsigned int i;
	for (i = 0; i < so_nb_used_polls; i++) {
		if (so_polls[i].fd == fd) {
			struct so_poll_info * info = so_poll_infos + i;

			info->timeout = timeout;
			clock_gettime(CLOCK_MONOTONIC, &info->limit);
			info->timeout_callback = callback;

			info->limit.tv_sec += timeout / 1000;
			info->limit.tv_nsec += 1000000 * (timeout % 1000);
			so_time_fix(&info->limit);

			return true;
		}
	}

	return false;
}

void so_poll_unregister(int fd, short event) {
	if (fd < 0)
		return;

	unsigned int i;
	for (i = 0; i < so_nb_used_polls; i++)
		if (so_polls[i].fd == fd && event == so_polls[i].events)
			break;

	if (i == so_nb_used_polls)
		return; // not found

	struct so_poll_info * new_info = so_poll_infos + i;
	if (new_info->data != NULL && new_info->release != NULL)
		new_info->release(new_info->data);

	if (i + 1 < so_nb_used_polls) {
		size_t nb_move = so_nb_used_polls - i - 1;
		memmove(so_polls + i, so_polls + (i + 1), nb_move * sizeof(struct pollfd));
		memmove(so_poll_infos + i, so_poll_infos + (i + 1), nb_move * sizeof(struct so_poll_info));
	}

	so_nb_used_polls--;
	so_poll_restart = true;
}

bool so_poll_unset_timeout(int fd) {
	if (fd < 0)
		return false;

	unsigned int i;
	for (i = 0; i < so_nb_used_polls; i++) {
		if (so_polls[i].fd == fd) {
			struct so_poll_info * info = so_poll_infos + i;

			info->timeout = -1;
			info->timeout_callback = NULL;

			return true;
		}
	}

	return false;
}

