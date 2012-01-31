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
*  Last modified: Mon, 30 Jan 2012 20:46:02 +0100                         *
\*************************************************************************/

#define _GNU_SOURCE
#include <fcntl.h>
#include <jansson.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <stone/checksum.h>
#include <stone/io/socket.h>
#include <stone/library/changer.h>
#include <stone/library/ressource.h>
#include <stone/library/tape.h>
#include <stone/threadpool.h>
#include <stone/user.h>

#include "admin.h"
#include "config.h"

struct st_admin_context {
	char * query;

	struct st_changer * changer;

	json_t * result;
	json_t * lines;
};

static short st_admin_do_authen(struct st_io_socket * socket);
static void st_admin_com_help(struct st_admin_context * context);
static void st_admin_com_list_changers(struct st_admin_context * context);
static void st_admin_com_list_slots(struct st_admin_context * context);
static void st_admin_com_select_changer(struct st_admin_context * context);
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

void st_admin_com_help(struct st_admin_context * context) {
	json_array_append_new(context->lines, json_string("List of commands:"));
	json_array_append_new(context->lines, json_string(" h, ?, help:         Show this"));
	json_array_append_new(context->lines, json_string(" lc, list changers:  List available changers"));
	json_array_append_new(context->lines, json_string(" ls, list slots:     List available slots of selected changer"));
	json_array_append_new(context->lines, json_string(" sl, select changer: Select changer by number (see command `cl`)"));

	json_object_set_new(context->result, "status code", json_integer(200));
	json_object_set_new(context->result, "status", json_string("Ok"));
}

void st_admin_com_list_changers(struct st_admin_context * context) {
	struct st_changer * ch = st_changer_get_first_changer();
	int i;
	for (i = 0; ch; i++) {
		char * line = 0;
		asprintf(&line, "%d. %s %s; rev: %s, serial: %s", i, ch->vendor, ch->model, ch->revision, ch->serial_number);

		json_array_append_new(context->lines, json_string(line));

		free(line);

		ch = st_changer_get_next_changer(ch);
	}
	json_object_set_new(context->result, "status code", json_integer(200));
	json_object_set_new(context->result, "status", json_string("Ok"));
}

void st_admin_com_list_slots(struct st_admin_context * context) {
	if (!context->changer) {
		context->changer = st_changer_get_first_changer();
		while (context->changer && context->changer->lock->ops->trylock(context->changer->lock))
			context->changer = st_changer_get_next_changer(context->changer);

		if (!context->changer) {
			json_array_append_new(context->lines, json_string("No changer available"));
			json_object_set_new(context->result, "status code", json_integer(410));
			json_object_set_new(context->result, "status", json_string("Gone"));
			return;
		} else {
			char * line;
			asprintf(&line, "Auto selecting changer: %s %s, rev: %s, serial: %s", context->changer->vendor, context->changer->model, context->changer->revision, context->changer->serial_number);
			json_array_append_new(context->lines, json_string(line));
			free(line);
		}
	}

	unsigned int i;
	for (i = 0; i < context->changer->nb_slots; i++) {
		struct st_slot * slot = context->changer->slots + i;

		char * line;
		if (slot->tape) {
			char * tape;
			asprintf(&tape, "status: %s, location: %s, nb loaded: %ld, block size: %zd, nb block available: %zd, available size: %zd", st_tape_status_to_string(slot->tape->status), st_tape_location_to_string(slot->tape->location), slot->tape->load_count, slot->tape->block_size, slot->tape->available_block, slot->tape->block_size * slot->tape->available_block);

			if (!*slot->tape->label && slot->tape->medium_serial_number)
				asprintf(&line, "%u. Full: medium serial number %s, %s", i, slot->tape->medium_serial_number, tape);
			else
				asprintf(&line, "%u. Full: label: %s, %s", i, slot->tape->label, tape);

			free(tape);

		} else {
			asprintf(&line, "%u. Empty", i);
		}

		json_array_append_new(context->lines, json_string(line));
		free(line);
	}

	json_object_set_new(context->result, "status code", json_integer(200));
	json_object_set_new(context->result, "status", json_string("Ok"));
}

void st_admin_com_select_changer(struct st_admin_context * context) {
	const char * params = context->query;
	if (params[3] == '\0')
		params = 0;
	else if (params[3] == ' ')
		params += 3;
	else
		params += 16;

	int index = 0;
	if (params)
		sscanf(params, "%d", &index);

	struct st_changer * ch = st_changer_get_first_changer();
	int i;
	for (i = 0; i < index; i++)
		ch = st_changer_get_next_changer(ch);

	int status;
	const char * status_string;

	if (ch && ch != context->changer) {
		if (ch->lock->ops->trylock(ch->lock)) {
			json_array_append_new(context->lines, json_string("Changer is busy"));

			status = 410;
			status_string = "Gone";
		} else {
			json_array_append_new(context->lines, json_string("Changer is now selected"));

			if (context->changer)
				context->changer->lock->ops->unlock(context->changer->lock);
			context->changer = ch;

			status = 200;
			status_string = "Ok";
		}
	} else if (ch) {
		json_array_append_new(context->lines, json_string("Changer previously selected"));

		status = 200;
		status_string = "Ok";
	} else if (!ch) {
		json_array_append_new(context->lines, json_string("Changer not found"));

		status = 404;
		status_string = "Not found";
	}

	json_object_set_new(context->result, "status code", json_integer(status));
	json_object_set_new(context->result, "status", json_string(status_string));
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

	struct st_admin_context context = {
		.changer = 0,
		.result  = 0,
	};

	for (;;) {
		char buffer[4096];
		ssize_t nb_read = sr->ops->read(sr, buffer, 4096);
		if (nb_read <= 0)
			break;

		json_error_t error;
		json_t * query_root = json_loadb(buffer, nb_read, 0, &error);
		if (!query_root)
			break;

		json_t * query = json_object_get(query_root, "query");
		if (!query) {
			json_decref(query_root);
			break;
		}

		context.query = strdup(json_string_value(query));

		static const struct st_admin_command {
			const char * name;
			enum {
				COM_HELP = 1,
				COM_LIST_CHANGERS,
				COM_LIST_SLOTS,
				COM_SELECT_CHANGER,
			} command;
		} commands[] = {
			{ "h",              COM_HELP },
			{ "help",           COM_HELP },
			{ "lc",             COM_LIST_CHANGERS },
			{ "list changers",  COM_LIST_CHANGERS },
			{ "list slots",     COM_LIST_SLOTS },
			{ "ls",             COM_LIST_SLOTS },
			{ "select changer", COM_SELECT_CHANGER },
			{ "sl",             COM_SELECT_CHANGER },
			{ "?",              COM_HELP },

			{ 0, 0 },
		};

		const struct st_admin_command * com;
		for (com = commands; com->name; com++)
			if (!strncmp(context.query, com->name, strlen(com->name)))
				break;

		json_decref(query_root);

		context.result = json_object();
		context.lines = json_array();
		json_object_set_new(context.result, "response", context.lines);

		switch (com->command) {
			case COM_HELP:
				st_admin_com_help(&context);
				break;

			case COM_LIST_CHANGERS:
				st_admin_com_list_changers(&context);
				break;

			case COM_LIST_SLOTS:
				st_admin_com_list_slots(&context);
				break;

			case COM_SELECT_CHANGER:
				st_admin_com_select_changer(&context);
				break;

			default:
				json_array_append_new(context.lines, json_string("Command not found"));
				json_object_set_new(context.result, "status code", json_integer(404));
				json_object_set_new(context.result, "status", json_string("Not found"));
				break;
		}

		char * message = json_dumps(context.result, 0);

		ssize_t nb_write = sw->ops->write(sw, message, strlen(message));

		free(message);
		json_decref(context.result);
		free(context.query);

		if (nb_write < 0)
			break;
	}

	if (context.changer)
		context.changer->lock->ops->unlock(context.changer->lock);

	sr->ops->free(sr);
	sw->ops->free(sw);
	socket->ops->free(socket);
}

