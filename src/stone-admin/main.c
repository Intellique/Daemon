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
*  Last modified: Thu, 26 Jan 2012 19:07:09 +0100                         *
\*************************************************************************/

// getopt_long
#include <getopt.h>
// printf
#include <stdio.h>

#include <stone/io/socket.h>

#include "auth.h"
#include "config.h"
#include "read_line.h"

static void st_show_help(void);

int main(int argc, char ** argv) {
	enum {
		OPT_HELP     = 'h',
		OPT_HOST     = 'H',
		OPT_PORT     = 'p',
		OPT_VERSION  = 'V',
	};

	static int option_index = 0;
	static struct option long_options[] = {
		{ "help",     0, 0, OPT_HELP },
		{ "host",     1, 0, OPT_HOST },
		{ "port",     1, 0, OPT_PORT },
		{ "version",  0, 0, OPT_VERSION },

		{0, 0, 0, 0},
	};

	char * host = ADMIN_DEFAULT_HOST;
	unsigned short port = ADMIN_DEFAULT_PORT;

	int opt;
	do {
		opt = getopt_long(argc, argv, "hH:V", long_options, &option_index);

		switch (opt) {
			case OPT_HELP:
				st_show_help();
				return 0;

			case OPT_HOST:
				host = optarg;
				break;

			case OPT_PORT:
				if (sscanf(optarg, "%hu", &port) < 0) {
					printf("Error while parsing option '-p', '%s' is not an integer\n", optarg);
					return 1;
				}
				break;

			case OPT_VERSION:
				printf("STone-admin, version: %s, build: %s %s\n", STONE_VERSION, __DATE__, __TIME__);
				return 0;
		}
	} while (opt > -1);

	struct st_io_socket * socket = st_io_socket_new();
	int failed = socket->ops->connect(socket, host, port);
	if (failed) {
		printf("Failed to connect to host: %s\n", host);
		return 2;
	}

	if (stad_auth_do(socket)) {
		printf("Failed to authentificate\n");
		return 3;
	}

	return 0;
}

void st_show_help() {
	printf("STone-admin, version: %s, build: %s %s\n", STONE_VERSION, __DATE__, __TIME__);
	printf("    --help,     -h : Show this and exit\n");
	printf("    --host,     -H : connect to this host instead of \"%s\"\n", ADMIN_DEFAULT_HOST);
	printf("    --port,     -p : connect to this port instead of \"%hu\"\n", ADMIN_DEFAULT_PORT);
	printf("    --version,  -V : Show the version of STone then exit\n");
}

