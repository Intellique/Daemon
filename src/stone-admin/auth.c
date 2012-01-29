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
*  Last modified: Fri, 27 Jan 2012 09:36:35 +0100                         *
\*************************************************************************/

#include <jansson.h>
#include <openssl/sha.h>
// printf
#include <stdio.h>
#include <string.h>

#include <stone/io/socket.h>

#include "auth.h"
#include "read_line.h"

int stad_auth_do(struct st_io_socket * socket) {
	if (!socket->ops->is_connected(socket))
		return -1;

	struct st_stream_reader * sr = socket->ops->get_output_stream(socket);
	char buffer[4096];

	ssize_t nb_read = sr->ops->read(sr, buffer, 4096);
	if (nb_read < 0) {
		sr->ops->free(sr);
		return -1; // error while reading data from server
	}

	json_error_t error;
	json_t * root = json_loadb(buffer, nb_read, 0, &error);
	if (!root) {
		sr->ops->free(sr);
		return -1;
	}

	json_t * daemon_version = json_object_get(root, "daemon version");
	json_t * daemon_uptime = json_object_get(root, "daemon uptime");
	printf("Connected to stone (version: %s, uptime: %s)\n", json_string_value(daemon_version), json_string_value(daemon_uptime));

	json_decref(root);

	char * user = stad_rl_get_line("User: ");
	if (!user) {
		sr->ops->free(sr);
		return -1;
	}

	root = json_object();
	json_object_set_new(root, "user", json_string(user));

	char * message = json_dumps(root, 0);

	struct st_stream_writer * sw = socket->ops->get_input_stream(socket);
	ssize_t nb_write = sw->ops->write(sw, message, strlen(message));

	free(message);
	json_decref(root);

	if (nb_write < 0) {
		sr->ops->free(sr);
		sw->ops->free(sw);
		return -1;
	}

	char * password = stad_rl_get_password("Password: ");
	if (!password) {
		sr->ops->free(sr);
		sw->ops->free(sw);
		return -1;
	}

	nb_read = sr->ops->read(sr, buffer, 4096);
	if (nb_read < 0) {
		sr->ops->free(sr);
		sw->ops->free(sw);
		return -1;
	}

	root = json_loadb(buffer, nb_read, 0, &error);
	if (!root) {
		sr->ops->free(sr);
		sw->ops->free(sw);
		return -1;
	}

	json_t * salt = json_object_get(root, "auth salt");
	json_t * challenge = json_object_get(root, "auth challenge");

	if (!salt || !challenge) {
		json_decref(root);
		sr->ops->free(sr);
		sw->ops->free(sw);
		return -1;
	}

	// compute password which will be send to daemon
	ssize_t password_length = strlen(password);

	char * password2 = malloc(81);
	strncpy(password2, password, password_length >> 1);
	strcpy(password2 + (password_length >> 1), json_string_value(salt));
	strcat(password2, password + (password_length >> 1));

	unsigned char digest[SHA_DIGEST_LENGTH];
	SHA1((unsigned char *) password2, strlen(password2), digest);
	int i;
	for (i = 0; i < 20; i++)
		snprintf(password2 + (i << 1), 3, "%02x", digest[i]);
	password2[i << 1] = '\0';

	strcat(password2, json_string_value(challenge));
	SHA1((unsigned char *) password2, strlen(password2), digest);
	for (i = 0; i < 20; i++)
		snprintf(password2 + (i << 1), 3, "%02x", digest[i]);
	password2[i << 1] = '\0';

	json_decref(root);

	root = json_object();
	json_object_set_new(root, "user", json_string(user));
	json_object_set_new(root, "password", json_string(password2));
	message = json_dumps(root, 0);

	sw = socket->ops->get_input_stream(socket);
	sw->ops->write(sw, message, strlen(message));

	// free some memory
	free(password2);
	free(message);
	free(password);
	free(user);
	json_decref(root);

	nb_read = sr->ops->read(sr, buffer, 4096);

	root = json_loadb(buffer, nb_read, 0, &error);
	if (!root) {
		sr->ops->free(sr);
		sw->ops->free(sw);
		return -1;
	}

	json_t * access = json_object_get(root, "access");
	if (!access) {
		sr->ops->free(sr);
		sw->ops->free(sw);
		return -1;
	}

	const char * sok = json_string_value(access);

	int ok = strcmp(sok, "granted");

	json_decref(root);
	sr->ops->free(sr);
	sw->ops->free(sw);

	return ok;
}

