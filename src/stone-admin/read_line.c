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
*  Last modified: Thu, 26 Jan 2012 19:05:52 +0100                         *
\*************************************************************************/

#include <readline/readline.h>
#include <termios.h>
#include <unistd.h>

#include "read_line.h"

static void stad_rl_init(void) __attribute__((constructor));


char * stad_rl_get_line(char * prompt) {
	return readline(prompt);
}

char * stad_rl_get_password(char * prompt) {
	struct termios conf;
	int failed = tcgetattr(0, &conf);
	if (failed)
		return stad_rl_get_line(prompt);

	printf(prompt);
	fflush(stdout);

	tcflag_t old_value = conf.c_lflag;
	conf.c_lflag &= ~ECHO;

	failed = tcsetattr(0, TCSANOW, &conf);

	char * line = readline(0);

	conf.c_lflag = old_value;
	tcsetattr(0, TCSANOW, &conf);

	printf("\n");

	return line;
}

void stad_rl_init(void) {
}

