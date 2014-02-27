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

// free
#include <stdlib.h>
// memmove
#include <string.h>

#include "poll_v1.h"

static struct pollfd * st_polls = NULL;
static struct st_poll_info {
	st_pool_callback_f callback;
	void * data;
} * st_poll_infos = NULL;
static unsigned int st_nb_polls = 0;

static void st_poll_exit(void) __attribute__((destructor));


static void st_poll_exit() {
	free(st_polls);
}

__asm__(".symver st_poll_v1, st_poll@@LIBSTONE_1.0");
int st_poll_v1(int timeout) {
	if (st_nb_polls < 1)
		return 0;

	unsigned int i;
	for (i = 0; i < st_nb_polls; i++)
		st_polls[i].revents = 0;

	int nb_event = poll(st_polls, st_nb_polls, timeout);
	if (nb_event == 0)
		return 0;

	for (i = 0; i < st_nb_polls; i++) {
		struct pollfd * evt = st_polls + i;
		if (evt->revents != 0)
			st_poll_infos[i].callback(evt->fd, evt->revents, st_poll_infos[i].data);
	}

	return nb_event;
}

__asm__(".symver st_poll_register_v1, st_poll_register@@LIBSTONE_1.0");
bool st_poll_register_v1(int fd, short event, st_pool_callback_f callback, void * data) {
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

	st_nb_polls++;

	return true;
}

__asm__(".symver st_poll_unregister_v1, st_poll_unregister@@LIBSTONE_1.0");
void st_poll_unregister_v1(int fd) {
	unsigned int i;
	for (i = 0; i < st_nb_polls; i++)
		if (st_polls[i].fd == fd)
			break;

	if (i == st_nb_polls)
		return;

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
}

