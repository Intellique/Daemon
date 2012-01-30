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
*  Last modified: Mon, 30 Jan 2012 11:50:33 +0100                         *
\*************************************************************************/

#include <jansson.h>
// getopt_long
#include <getopt.h>
// printf
#include <stdio.h>
#include <string.h>

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

	struct st_stream_reader * sr = socket->ops->get_output_stream(socket);
	struct st_stream_writer * sw = socket->ops->get_input_stream(socket);

	for (;;) {
		char * line = stad_rl_get_line("Query: ");
		if (!line)
			return 0;

		json_t * root = json_object();
		json_object_set_new(root, "query", json_string(line));

		char * message = json_dumps(root, 0);

		ssize_t nb_write = sw->ops->write(sw, message, strlen(message));

		free(message);
		json_decref(root);
		free(line);

		if (nb_write <= 0) {
			printf("Ops, failed to send message, exiting\n");
			break;
		}

		char buffer[4096];
		ssize_t nb_read = sr->ops->read(sr, buffer, 4096);
		if (nb_read <= 0) {
			printf("Ops, no data received from daemon, exiting\n");
			break;
		}

		json_error_t error;
		root = json_loadb(buffer, nb_read, 0, &error);
		if (!root) {
			printf("Ops, daemon sent data but it is not json formatted, exiting\n");
			break;
		}

		json_t * result = json_object_get(root, "response");
		json_t * status = json_object_get(root, "status");
		if (!result || !status) {
			printf("Ops, there is no status nor result in daemon's response, exiting\n");
			json_decref(root);
			continue;
		}

		size_t nb_line = json_array_size(result);
		size_t i_line;
		for (i_line = 0; i_line < nb_line; i_line++) {
			json_t * line = json_array_get(result, i_line);
			printf(">  %s\n", json_string_value(line));
		}

		printf(">> Status code: %" JSON_INTEGER_FORMAT "\n", json_integer_value(status));

		json_decref(root);
	}

	sr->ops->close(sr);
	sw->ops->close(sw);

	sr->ops->free(sr);
	sw->ops->free(sw);

	socket->ops->free(socket);

	return 0;
}

void st_show_help() {
	printf("STone-admin, version: %s, build: %s %s\n", STONE_VERSION, __DATE__, __TIME__);
	printf("    --help,     -h : Show this and exit\n");
	printf("    --host,     -H : connect to this host instead of \"%s\"\n", ADMIN_DEFAULT_HOST);
	printf("    --port,     -p : connect to this port instead of \"%hu\"\n", ADMIN_DEFAULT_PORT);
	printf("    --version,  -V : Show the version of STone then exit\n");
}

