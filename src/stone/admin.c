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
*  Last modified: Sun, 29 Jan 2012 10:47:49 +0100                         *
\*************************************************************************/

#include <jansson.h>

#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>


#include <stone/checksum.h>
#include <stone/io/socket.h>
#include <stone/threadpool.h>
#include <stone/user.h>

#include "admin.h"
#include "config.h"

static short st_admin_do_authen(struct st_io_socket * socket);
static void st_admin_init(void) __attribute__((constructor));
static void st_admin_listen(void * arg);
static void st_admin_work(void * arg);

static time_t st_admin_boottime = 0;


short st_admin_do_authen(struct st_io_socket * socket) {
	struct st_stream_writer * sw = socket->ops->get_input_stream(socket);

	time_t now = time(0);
	long uptime = now - st_admin_boottime;
	char suptime[32];
	snprintf(suptime, 32, "%lds", uptime);

	json_t * root = json_object();
	json_object_set_new(root, "daemon version", json_string(STONE_VERSION));
	json_object_set_new(root, "daemon uptime", json_string(suptime));

	char * message = json_dumps(root, 0);
	// sending welcome message
	ssize_t nb_write = sw->ops->write(sw, message, strlen(message));

	free(message);
	json_decref(root);

	if (nb_write < 0) {
		sw->ops->free(sw);
		return 0;
	}

	char buffer[4096];
	// waiting username
	struct st_stream_reader * sr = socket->ops->get_output_stream(socket);
	ssize_t nb_read = sr->ops->read(sr, buffer, 4096);
	if (nb_read < 0) {
		sr->ops->free(sr);
		sw->ops->free(sw);
		return 0;
	}

	json_error_t error;
	root = json_loadb(buffer, nb_read, 0, &error);
	if (!root) {
		sr->ops->free(sr);
		sw->ops->free(sw);
		return 0;
	}

	json_t * user = json_object_get(root, "user");
	struct st_user * usr = 0;
	if (user)
		usr = st_user_get(-1, json_string_value(user));

	json_decref(root);

	struct tm current;
	localtime_r(&now, &current);
	char challenge[32];
	strftime(challenge, 32, "%F %T %Z", &current);
	char * digest = st_checksum_compute("sha1", challenge, strlen(challenge));

	root = json_object();
	if (usr) {
		json_object_set_new(root, "auth salt", json_string(usr->salt));
	} else {
		int fd = open("/dev/urandom", O_RDONLY);
		unsigned char data[8];
		char salt[17];

		read(fd, data, 8);
		close(fd);

		st_checksum_convert_to_hex(data, 8, salt);

		json_object_set_new(root, "auth salt", json_string(salt));
	}
	json_object_set_new(root, "auth challenge", json_string(digest));

	message = json_dumps(root, 0);
	// send challenge
	nb_write = sw->ops->write(sw, message, strlen(message));

	free(message);
	json_decref(root);

	if (nb_write < 0) {
		free(digest);
		sr->ops->free(sr);
		sw->ops->free(sw);
		return 0;
	}

	// compute password that stone-admin will send
	char * challenge_ok = 0;
	if (usr) {
		ssize_t length = strlen(usr->password) + strlen(digest);
		char * pass = malloc(length + 1);

		strcpy(pass, usr->password);
		strcat(pass, digest);

		challenge_ok = st_checksum_compute("sha1", pass, length);

		free(pass);
	}

	free(digest);

	nb_read = sr->ops->read(sr, buffer, 4096);
	if (nb_read < 0) {
		free(challenge_ok);
		sr->ops->free(sr);
		sw->ops->free(sw);
		return 0;
	}

	root = json_loadb(buffer, nb_read, 0, &error);
	if (!root) {
		free(challenge_ok);
		sr->ops->free(sr);
		sw->ops->free(sw);
		return 0;
	}

	json_t * jpass = json_object_get(root, "password");
	char * password = 0;
	if (jpass)
		password = strdup(json_string_value(jpass));

	json_decref(root);

	root = json_object();

	short ok = 0;
	if (!usr || !password) {
		json_object_set_new(root, "access", json_string("refused"));
	} else if (password && !strcmp(password, challenge_ok)) {
		json_object_set_new(root, "access", json_string("granted"));
		ok = 1;
	} else {
		json_object_set_new(root, "access", json_string("refused"));
	}

	message = json_dumps(root, 0);
	nb_write = sw->ops->write(sw, message, strlen(message));

	free(password);
	free(challenge_ok);

	sr->ops->free(sr);
	sw->ops->free(sw);

	return ok;
}

void st_admin_init() {
	st_admin_boottime = time(0);
}

void st_admin_listen(void * arg __attribute__((unused))) {
	struct st_io_socket * socket = st_io_socket_new();
	if (socket->ops->bind(socket, 0, 4862)) {
		socket->ops->free(socket);
		return;
	}

	struct st_io_socket * client;
	while ((client = socket->ops->accept(socket))) {
		st_threadpool_run(st_admin_work, client);
		sleep(1);
	}

	socket->ops->free(socket);
}

void st_admin_start() {
	st_threadpool_run(st_admin_listen, 0);
}

void st_admin_work(void * arg) {
	struct st_io_socket * socket = arg;

	if (!st_admin_do_authen(socket)) {
		socket->ops->free(socket);
		return;
	}

	struct st_stream_reader * sr = socket->ops->get_output_stream(socket);
	struct st_stream_writer * sw = socket->ops->get_input_stream(socket);

	for (;;) {
		char buffer[4096];
		ssize_t nb_read = sr->ops->read(sr, buffer, 4096);
		if (nb_read < 0)
			break;

		json_error_t error;
		json_t * query = json_loadb(buffer, nb_read, 0, &error);
	}

	sr->ops->free(sr);
	sw->ops->free(sw);
	socket->ops->free(socket);
}

