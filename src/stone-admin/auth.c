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
*  Last modified: Thu, 26 Jan 2012 17:49:50 +0100                         *
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
	if (nb_read == 0) {
		sr->ops->free(sr);
		return -1; // data was expected
	}

	json_error_t error;
	json_t * root = json_loadb(buffer, nb_read, 0, &error);
	if (!root) {
		sr->ops->free(sr);
		return -1;
	}

	json_t * daemon_version = json_object_get(root, "daemon version");
	json_t * daemon_uptime = json_object_get(root, "daemon uptime");
	json_t * challenge = json_object_get(root, "auth challenge");
	if (!challenge) {
		sr->ops->free(sr);
		return -1;
	}

	printf("Connected to stone (version: %s, uptime: %s)\n", json_string_value(daemon_version), json_string_value(daemon_uptime));
	char * user = stad_rl_get_line("User: ");
	if (!user)
		return -1;
	char * password = stad_rl_get_password("Password: ");
	if (!password)
		return -1;

	// compute password which will be send to daemon
	ssize_t digest_length = strlen(json_string_value(challenge));
	ssize_t password_length = strlen(password);

	char * password2 = malloc(digest_length + password_length + 1);
	strncpy(password2, password, password_length >> 1);
	strcpy(password2 + (password_length >> 1), json_string_value(challenge));
	strcat(password2, password + (password_length >> 1));

	unsigned char * digest = SHA1((unsigned char *) password2, digest_length + password_length, 0);
	char hexDigest[41];

	int i;
	for (i = 0; i < 20; i++)
		snprintf(hexDigest + (i << 1), 3, "%02x", digest[i]);
	hexDigest[i << 1] = '\0';

	json_t * root2 = json_object();
	json_object_set_new(root2, "user", json_string(user));
	json_object_set_new(root2, "password", json_string(hexDigest));

	char * message = json_dumps(root2, 0);

	struct st_stream_writer * sw = socket->ops->get_input_stream(socket);
	sw->ops->write(sw, message, strlen(message));

	// free some memory
	free(message);
	json_decref(root2);
	free(password2);
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

