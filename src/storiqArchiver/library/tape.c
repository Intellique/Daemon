/*************************************************************************\
*        ______           ____     ___           __   _                   *
*       / __/ /____  ____/  _/__ _/ _ | ________/ /  (_)  _____ ____      *
*      _\ \/ __/ _ \/ __// // _ `/ __ |/ __/ __/ _ \/ / |/ / -_) __/      *
*     /___/\__/\___/_/ /___/\_, /_/ |_/_/  \__/_//_/_/|___/\__/_/         *
*                            /_/                                          *
*  ---------------------------------------------------------------------  *
*  This file is a part of StorIqArchiver                                  *
*                                                                         *
*  StorIqArchiver is free software; you can redistribute it and/or        *
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
*  Last modified: Wed, 07 Dec 2011 20:27:06 +0100                         *
\*************************************************************************/

// sscanf
#include <stdio.h>
// malloc, realloc
#include <stdlib.h>
// strcmp, strcpy
#include <string.h>
// time
#include <time.h>

#include <storiqArchiver/database.h>
#include <storiqArchiver/io.h>
#include <storiqArchiver/library/changer.h>
#include <storiqArchiver/library/drive.h>
#include <storiqArchiver/library/tape.h>

static const struct sa_tape_format_data_type2 {
	char * name;
	enum sa_tape_format_data_type type;
} sa_tape_format_data_types[] = {
	{ "audio",    SA_TAPE_FORMAT_DATA_AUDIO },
	{ "cleaning", SA_TAPE_FORMAT_DATA_CLEANING },
	{ "data",     SA_TAPE_FORMAT_DATA_DATA },
	{ "video",    SA_TAPE_FORMAT_DATA_VIDEO },

	{ 0, SA_TAPE_FORMAT_DATA_UNKNOWN },
};

static const struct sa_tape_format_mode2 {
	char * name;
	enum sa_tape_format_mode mode;
} sa_tape_format_modes[] = {
	{ "disk",    SA_TAPE_FORMAT_MODE_DISK },
	{ "linear",  SA_TAPE_FORMAT_MODE_LINEAR },
	{ "optical", SA_TAPE_FORMAT_MODE_OPTICAL },

	{ 0, SA_TAPE_FORMAT_MODE_UNKNOWN },
};

static const struct sa_tape_location2 {
	char * name;
	enum sa_tape_location location;
} sa_tape_locations[] = {
	{ "offline", SA_TAPE_LOCATION_OFFLINE },
	{ "online",  SA_TAPE_LOCATION_ONLINE },

	{ 0, SA_TAPE_LOCATION_UNKNOWN },
};

static const struct sa_tape_status2 {
	char * name;
	enum sa_tape_status status;
} sa_tape_status[] = {
	{ "erasable",          SA_TAPE_STATUS_ERASABLE },
	{ "error",             SA_TAPE_STATUS_ERROR },
	{ "foreign",           SA_TAPE_STATUS_FOREIGN },
	{ "in use",            SA_TAPE_STATUS_IN_USE },
	{ "locked",            SA_TAPE_STATUS_LOCKED },
	{ "needs replacement", SA_TAPE_STATUS_NEEDS_REPLACEMENT },
	{ "new",               SA_TAPE_STATUS_NEW },
	{ "pooled",            SA_TAPE_STATUS_POOLED },

	{ 0, SA_TAPE_STATUS_UNKNOWN },
};

static struct sa_tape_format * sa_tape_formats = 0;
static unsigned int sa_tape_format_nb_formats = 0;


const char * sa_tape_format_data_to_string(enum sa_tape_format_data_type type) {
	const struct sa_tape_format_data_type2 * ptr;
	for (ptr = sa_tape_format_data_types; ptr->name; ptr++)
		if (ptr->type == type)
			return ptr->name;
	return "unknown";
}

const char * sa_tape_format_mode_to_string(enum sa_tape_format_mode mode) {
	const struct sa_tape_format_mode2 * ptr;
	for (ptr = sa_tape_format_modes; ptr->name; ptr++)
		if (ptr->mode == mode)
			return ptr->name;
	return "unknown";
}

const char * sa_tape_location_to_string(enum sa_tape_location location) {
	const struct sa_tape_location2 * ptr;
	for (ptr = sa_tape_locations; ptr->name; ptr++)
		if (ptr->location == location)
			return ptr->name;
	return "unknown";
}

const char * sa_tape_status_to_string(enum sa_tape_status status) {
	const struct sa_tape_status2 * ptr;
	for (ptr = sa_tape_status; ptr->name; ptr++)
		if (ptr->status == status)
			return ptr->name;
	return "unknown";
}

enum sa_tape_location sa_tape_string_to_location(const char * location) {
	const struct sa_tape_location2 * ptr;
	for (ptr = sa_tape_locations; ptr->name; ptr++)
		if (!strcmp(location, ptr->name))
			return ptr->location;
	return ptr->location;
}

enum sa_tape_status sa_tape_string_to_status(const char * status) {
	const struct sa_tape_status2 * ptr;
	for (ptr = sa_tape_status; ptr->name; ptr++)
		if (!strcmp(status, ptr->name))
			return ptr->status;
	return ptr->status;
}

enum sa_tape_format_data_type sa_tape_string_to_format_data(const char * type) {
	const struct sa_tape_format_data_type2 * ptr;
	for (ptr = sa_tape_format_data_types; ptr->name; ptr++)
		if (!strcmp(type, ptr->name))
			return ptr->type;
	return ptr->type;
}

enum sa_tape_format_mode sa_tape_string_to_format_mode(const char * mode) {
	const struct sa_tape_format_mode2 * ptr;
	for (ptr = sa_tape_format_modes; ptr->name; ptr++)
		if (!strcmp(mode, ptr->name))
			return ptr->mode;
	return ptr->mode;
}


void sa_tape_detect(struct sa_drive * dr) {
	struct sa_tape * tape = dr->slot->tape;
	if (!tape)
		return;

	dr->ops->rewind_tape(dr);
	dr->ops->get_block_size(dr);

	char buffer[512];
	struct sa_stream_reader * reader = dr->ops->get_reader(dr);
	ssize_t nb_read = reader->ops->read(reader, buffer, 512);
	reader->ops->close(reader);
	reader->ops->free(reader);

	strcpy(tape->name, dr->slot->volume_name);
	tape->location = SA_TAPE_LOCATION_ONLINE;
	tape->first_used = time(0);
	tape->use_before = tape->first_used + tape->format->life_span;

	if (nb_read == 0) {
		tape->status = SA_TAPE_STATUS_NEW;
		tape->load_count = 1;
		return;
	}

	char name[65];
	char label[37];
	ssize_t block_size;
	char digest[9];
	if (sscanf(buffer, "StoriqArchiver\nversion: 0.1\nname: %64s\nuuid: %36s\nblocksize: %zd\ncrc32: %8s\n", name, label, &block_size, digest) == 4) {
		 strcpy(tape->label, label);
		 strcpy(tape->name, name);
		 tape->status = SA_TAPE_STATUS_IN_USE;
		 tape->block_size = block_size;
	} else {
		 tape->status = SA_TAPE_STATUS_FOREIGN;
	}

	dr->ops->eod(dr);

	tape->nb_files = dr->nb_files;

	static struct sa_database_connection * con = 0;
	if (!con) {
		struct sa_database * db = sa_db_get_default_db();
		con = db->ops->connect(db, 0);
	}

	con->ops->sync_tape(con, tape);
}

struct sa_tape * sa_tape_new(struct sa_drive * dr) {
	struct sa_tape * tape = malloc(sizeof(struct sa_tape));
	tape->id = -1;
	*tape->label = '\0';
	*tape->name = '\0';
	tape->status = SA_TAPE_STATUS_UNKNOWN;
	tape->location = SA_TAPE_LOCATION_UNKNOWN;
	tape->first_used = 0;
	tape->use_before = 0;
	tape->load_count = 1;
	tape->read_count = 0;
	tape->write_count = 0;
	tape->end_position = 0;
	tape->nb_files = 0;
	tape->has_partition = 0;
	tape->format = sa_tape_format_get_by_density_code(dr->density_code);
	tape->block_size = 0;
	tape->pool = 0;
	return tape;
}


struct sa_tape_format * sa_tape_format_get_by_density_code(unsigned char density_code) {
	unsigned int i;
	for (i = 0; i < sa_tape_format_nb_formats; i++)
		if (sa_tape_formats[i].density_code == density_code)
			return sa_tape_formats + i;

	struct sa_database * db = sa_db_get_default_db();
	struct sa_database_connection * con = db->ops->connect(db, 0);

	sa_tape_formats = realloc(sa_tape_formats, (sa_tape_format_nb_formats + 1) * sizeof(struct sa_tape_format));
	con->ops->get_tape_format(con, sa_tape_formats + sa_tape_format_nb_formats, density_code);
	sa_tape_format_nb_formats++;

	con->ops->close(con);
	con->ops->free(con);
	free(con);

	return sa_tape_formats + sa_tape_format_nb_formats - 1;
}

