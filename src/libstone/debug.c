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

#define _GNU_SOURCE
// backtrace, backtrace_symbols
#include <execinfo.h>
// pthread_self
#include <pthread.h>
// calloc, free
#include <stdlib.h>

#include <libstone/log.h>

#include "debug.h"

__asm__(".symver st_debug_log_stack_v1, st_debug_log_stack@@LIBSTONE_1.2");
void st_debug_log_stack_v1(unsigned int nb_stacks) {
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

	st_log_write(st_log_level_debug, "Dump %zd stack%c, stack addr: %p, stack size: %zd", size, size != 1 ? 's' : '\0', stack_addr, stack_size);

	ssize_t i;
	for (i = 0; i < size; i++)
		st_log_write(st_log_level_debug, "#%zd %s", i, strings[i]);

	free(strings);
	free(array);
}

