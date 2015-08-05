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
*  Copyright (C) 2013-2015, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#define _GNU_SOURCE
// backtrace, backtrace_symbols
#include <execinfo.h>
// dgettext, dngettext
#include <libintl.h>
// pthread_getattr_np, pthread_attr_destroy, pthread_attr_getstack,
// pthread_self
#include <pthread.h>
// calloc, free
#include <stdlib.h>

#include <libstoriqone/debug.h>
#include <libstoriqone/log.h>

void so_debug_log_stack(unsigned int nb_stacks) {
	if (nb_stacks < 1)
		return;

	void * array = calloc(nb_stacks, sizeof(void *));
	ssize_t size = backtrace(array, nb_stacks);
	char ** strings = backtrace_symbols(array, size);

	pthread_attr_t attr;
	pthread_t self = pthread_self();
	pthread_getattr_np(self, &attr);

	void * stack_addr;
	size_t stack_size;
	pthread_attr_getstack(&attr, &stack_addr, &stack_size);
	pthread_attr_destroy(&attr);

	so_log_write(so_log_level_debug,
		dngettext("libstoriqone", "Dump %zd stack, stack addr: %p, stack size: %zd", "Dump %zd stacks, stack addr: %p, stack size: %zd", size),
		size, stack_addr, stack_size);

	ssize_t i;
	for (i = 0; i < size; i++)
		so_log_write(so_log_level_debug,
			dgettext("libstoriqone", "#%zd %s"),
			i, strings[i]);

	free(strings);
	free(array);
}

