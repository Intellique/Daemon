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
*  Last modified: Mon, 28 Nov 2011 21:12:09 +0100                         *
\*************************************************************************/

// strcmp
#include <string.h>

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

