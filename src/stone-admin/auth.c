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
*  Last modified: Tue, 01 May 2012 21:40:06 +0200                         *
\*************************************************************************/

#include <jansson.h>
#include <openssl/sha.h>
// printf
#include <stdio.h>
#include <string.h>

#include <stone/io/socket.h>

#include "auth.h"
#include "read_line.h"
#include "recv.h"

int stad_auth_do(struct st_io_socket * socket) {
	if (!socket->ops->is_connected(socket))
		return -1;

	struct st_stream_reader * sr = socket->ops->get_output_stream(socket);

	int status = 0;
	json_t * lines = json_array();
	json_t * data = 0;

	enum stad_query query = stad_recv(sr, 0, &status, lines, &data);
	if (query == stad_query_unknown) {
		sr->ops->free(sr);
		return -1;
	}

	size_t i, nb_elts = json_array_size(lines);
	for (i = 0; i < nb_elts; i++)
		printf("%s\n", json_string_value(json_array_get(lines, i)));

	char * user = stad_rl_get_line("User: ");

	json_decref(lines);
	json_decref(data);

	if (!user) {
		sr->ops->free(sr);
		return -1;
	}

	data = json_object();
	json_object_set_new(data, "line", json_string(user));
	free(user);

	struct st_stream_writer * sw = socket->ops->get_input_stream(socket);
	if (stad_send(sw, data)) {
		json_decref(data);
		sr->ops->free(sr);
		sw->ops->free(sw);
		return -1;
	}

	json_decref(data);

	char * password = stad_rl_get_password("Password: ");
	if (!password) {
		sr->ops->free(sr);
		sw->ops->free(sw);
		return -1;
	}

	query = stad_recv(sr, 0, &status, 0, &data);
	if (query == stad_query_unknown) {
		sr->ops->free(sr);
		sw->ops->free(sw);
		return -1;
	}

	json_t * salt = json_object_get(data, "auth salt");
	json_t * challenge = json_object_get(data, "auth challenge");

	if (!salt || !challenge) {
		json_decref(data);
		sr->ops->free(sr);
		sw->ops->free(sw);
		return -1;
	}

	// compute password which will be send to daemon
	ssize_t password_length = strlen(password);

	const char * csalt = json_string_value(salt);
	const char * cchalenge = json_string_value(challenge);

	size_t password2_length = password_length + strlen(csalt);
	if (password2_length < 40 + strlen(cchalenge))
		password2_length = 40 + strlen(cchalenge);

	char * password2 = malloc(password2_length + 1);
	strncpy(password2, password, password_length >> 1);
	strcpy(password2 + (password_length >> 1), json_string_value(salt));
	strcat(password2, password + (password_length >> 1));

	unsigned char digest[SHA_DIGEST_LENGTH];
	SHA1((unsigned char *) password2, strlen(password2), digest);
	for (i = 0; i < 20; i++)
		snprintf(password2 + (i << 1), 3, "%02x", digest[i]);
	password2[i << 1] = '\0';

	strcat(password2, json_string_value(challenge));
	SHA1((unsigned char *) password2, strlen(password2), digest);
	for (i = 0; i < 20; i++)
		snprintf(password2 + (i << 1), 3, "%02x", digest[i]);
	password2[i << 1] = '\0';

	json_decref(data);

	data = json_object();
	json_object_set_new(data, "line", json_string(password2));

	int failed = stad_send(sw, data);

	// free some memory
	free(password2);
	free(password);
	json_decref(data);

	if (failed) {
		sr->ops->free(sr);
		sw->ops->free(sw);
		return -1;
	}

	lines = json_array();
	query = stad_recv(sr, 0, &status, lines, 0);

	nb_elts = json_array_size(lines);
	for (i = 0; i < nb_elts; i++)
		printf("%s\n", json_string_value(json_array_get(lines, i)));

	json_decref(lines);
	sr->ops->free(sr);
	sw->ops->free(sw);

	return status != 200;
}

