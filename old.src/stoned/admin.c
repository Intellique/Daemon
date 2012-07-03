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
*  Last modified: Tue, 01 May 2012 00:14:51 +0200                         *
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

	int status;
	json_t * lines;
};

static short st_admin_do_authen(struct st_io_socket * socket);
static void st_admin_com_help(struct st_admin_context * context);
static void st_admin_com_list_changers(struct st_admin_context * context);
static void st_admin_com_list_slots(struct st_admin_context * context);
static void st_admin_com_select_changer(struct st_admin_context * context);
static char * st_admin_get_line(struct st_stream_reader * reader, struct st_stream_writer * writer, char * prompt, int status, json_t * lines, json_t * data);
static char * st_admin_get_password(struct st_stream_reader * reader, struct st_stream_writer * writer, char * prompt, int status, json_t * lines, json_t * data);
static void st_admin_init(void) __attribute__((constructor));
static void st_admin_listen(void * arg);
static void st_admin_send_status(struct st_stream_writer * writer, int status, json_t * lines, json_t * data);
static void st_admin_work(void * arg);

static time_t st_admin_boottime = 0;


short st_admin_do_authen(struct st_io_socket * socket) {
	struct st_stream_reader * sr = socket->ops->get_output_stream(socket);
	struct st_stream_writer * sw = socket->ops->get_input_stream(socket);

	time_t now = time(0);
	long uptime = now - st_admin_boottime;
	char suptime[64];
	snprintf(suptime, 32, "stoned uptime : %lds", uptime);

	json_t * lines = json_array();
	json_array_append_new(lines, json_string("stoned version: " STONE_VERSION));
	json_array_append_new(lines, json_string(suptime));

	char * str_user = st_admin_get_line(sr, sw, "User > ", 100, lines, 0);
	if (!str_user) {
		sr->ops->free(sr);
		sw->ops->free(sw);
		return 0;
	}

	struct st_user * usr = st_user_get(-1, str_user);

	struct tm current;
	localtime_r(&now, &current);
	char challenge[32];
	strftime(challenge, 32, "%F %T %Z", &current);
	char * digest = st_checksum_compute("sha1", challenge, strlen(challenge));

	json_t * data = json_object();
	if (usr) {
		json_object_set_new(data, "auth salt", json_string(usr->salt));
	} else {
		int fd = open("/dev/urandom", O_RDONLY);
		unsigned char raw[8];
		char salt[17];

		read(fd, raw, 8);
		close(fd);

		st_checksum_convert_to_hex(raw, 8, salt);

		json_object_set_new(data, "auth salt", json_string(salt));
	}
	json_object_set_new(data, "auth challenge", json_string(digest));

	// compute password that stone-admin should send
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

	char * password = st_admin_get_password(sr, sw, "Password > ", 100, 0, data);
	if (!password) {
		free(challenge_ok);
		sr->ops->free(sr);
		sw->ops->free(sw);
		return 0;
	}

	short ok = 0;
	if (usr && !strcmp(password, challenge_ok)) {
		lines = json_array();
		json_array_append_new(lines, json_string("access granted"));
		st_admin_send_status(sw, 200, lines, 0);

		ok = 1;
	} else {
		lines = json_array();
		json_array_append_new(lines, json_string("access refused"));
		st_admin_send_status(sw, 401, lines, 0);
	}

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

	context->status = 200;
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
	context->status = 200;
}

void st_admin_com_list_slots(struct st_admin_context * context) {
	if (!context->changer) {
		context->changer = st_changer_get_first_changer();
		while (context->changer && context->changer->lock->ops->trylock(context->changer->lock))
			context->changer = st_changer_get_next_changer(context->changer);

		if (!context->changer) {
			json_array_append_new(context->lines, json_string("No changer available"));
			context->status = 410;
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

	context->status = 200;
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

	if (ch && ch != context->changer) {
		if (ch->lock->ops->trylock(ch->lock)) {
			json_array_append_new(context->lines, json_string("Changer is busy"));
			context->status = 410;
		} else {
			json_array_append_new(context->lines, json_string("Changer is now selected"));

			if (context->changer)
				context->changer->lock->ops->unlock(context->changer->lock);
			context->changer = ch;

			context->status = 200;
		}
	} else if (ch) {
		json_array_append_new(context->lines, json_string("Changer previously selected"));

		context->status = 200;
	} else if (!ch) {
		json_array_append_new(context->lines, json_string("Changer not found"));

		context->status = 404;
	}
}

char * st_admin_get_line(struct st_stream_reader * reader, struct st_stream_writer * writer, char * prompt, int status, json_t * lines, json_t * data) {
	json_t * root = json_object();
	json_object_set_new(root, "type", json_string("getline"));
	json_object_set_new(root, "prompt", json_string(prompt));
	json_object_set_new(root, "status", json_integer(status));
	if (lines)
		json_object_set_new(root, "lines", lines);
	if (data)
		json_object_set_new(root, "data", data);

	char * message = json_dumps(root, 0);
	json_decref(root);

	ssize_t nb_write = writer->ops->write(writer, message, strlen(message));
	free(message);

	if (nb_write < 0)
		return 0;

	ssize_t buffer_size = 4096;
	ssize_t buffer_pos = 0;
	char * buffer = malloc(buffer_size);
	for (;;) {
		ssize_t nb_read = reader->ops->read(reader, buffer + buffer_pos, buffer_size - buffer_pos);

		if (nb_read < 0) {
			free(buffer);
			return 0;
		}

		buffer_pos += nb_read;

		json_error_t error;
		root = json_loadb(buffer, buffer_pos, 0, &error);

		if (!root && buffer_pos + 512 > buffer_size) {
			buffer_size <<= 1;
			buffer = realloc(buffer, buffer_size);
		}

		if (root) {
			free(buffer);

			char * str_line = 0;
			json_t * line = json_object_get(root, "line");
			if (line)
				str_line = strdup(json_string_value(line));
			json_decref(root);

			return str_line;
		}
	}

	return 0;
}

char * st_admin_get_password(struct st_stream_reader * reader, struct st_stream_writer * writer, char * prompt, int status, json_t * lines, json_t * data) {
	json_t * root = json_object();
	json_object_set_new(root, "type", json_string("getpassword"));
	json_object_set_new(root, "prompt", json_string(prompt));
	json_object_set_new(root, "status", json_integer(status));
	if (lines)
		json_object_set_new(root, "lines", lines);
	if (data)
		json_object_set_new(root, "data", data);

	char * message = json_dumps(root, 0);
	json_decref(root);

	ssize_t nb_write = writer->ops->write(writer, message, strlen(message));
	free(message);

	if (nb_write < 0)
		return 0;

	ssize_t buffer_size = 4096;
	ssize_t buffer_pos = 0;
	char * buffer = malloc(buffer_size);
	for (;;) {
		ssize_t nb_read = reader->ops->read(reader, buffer + buffer_pos, buffer_size - buffer_pos);

		if (nb_read < 0) {
			free(buffer);
			return 0;
		}

		buffer_pos += nb_read;

		json_error_t error;
		root = json_loadb(buffer, buffer_pos, 0, &error);

		if (!root && buffer_pos + 512 > buffer_size) {
			buffer_size <<= 1;
			buffer = realloc(buffer, buffer_size);
		}

		if (root) {
			free(buffer);

			char * str_line = 0;
			json_t * line = json_object_get(root, "line");
			if (line)
				str_line = strdup(json_string_value(line));
			json_decref(root);

			return str_line;
		}
	}

	return 0;
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

void st_admin_send_status(struct st_stream_writer * writer, int status, json_t * lines, json_t * data) {
	json_t * root = json_object();
	json_object_set_new(root, "type", json_string("status"));
	json_object_set_new(root, "status", json_integer(status));
	if (lines)
		json_object_set_new(root, "lines", lines);
	if (data)
		json_object_set_new(root, "data", data);

	char * message = json_dumps(root, 0);
	json_decref(root);

	writer->ops->write(writer, message, strlen(message));
	free(message);
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
	};

	for (;;) {
		context.query = st_admin_get_line(sr, sw, "> ", 100, 0, 0);

		if (!context.query)
			break;

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

		context.status = 0;
		context.lines = json_array();

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
				context.status = 404;
				json_array_append_new(context.lines, json_string("Command not found"));
				break;
		}

		st_admin_send_status(sw, context.status, context.lines, 0);
	}

	if (context.changer)
		context.changer->lock->ops->unlock(context.changer->lock);

	sr->ops->free(sr);
	sw->ops->free(sw);
	socket->ops->free(socket);
}

