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
*  Last modified: Wed, 25 Jul 2012 22:32:37 +0200                         *
\*************************************************************************/

// strcmp
#include <string.h>

#include <libstone/library/media.h>

static const struct st_media_format_data_type2 {
	char * name;
	enum st_media_format_data_type type;
} st_media_format_data_types[] = {
	{ "audio",    st_media_format_data_audio },
	{ "cleaning", st_media_format_data_cleaning },
	{ "data",     st_media_format_data_data },
	{ "video",    st_media_format_data_video },

	{ 0, st_media_format_data_unknown },
};

static const struct st_media_format_mode2 {
	char * name;
	enum st_media_format_mode mode;
} st_media_format_modes[] = {
	{ "disk",    st_media_format_mode_disk },
	{ "linear",  st_media_format_mode_linear },
	{ "optical", st_media_format_mode_optical },

	{ 0, st_media_format_mode_unknown },
};

static const struct st_media_location2 {
	char * name;
	enum st_media_location location;
} st_media_locations[] = {
	{ "in drive", st_media_location_indrive },
	{ "offline",  st_media_location_offline },
	{ "online",   st_media_location_online },

	{ 0, st_media_location_unknown },
};

static const struct st_media_status2 {
	char * name;
	enum st_media_status status;
} st_media_status[] = {
	{ "erasable",          st_media_status_erasable },
	{ "error",             st_media_status_error },
	{ "foreign",           st_media_status_foreign },
	{ "in use",            st_media_status_in_use },
	{ "locked",            st_media_status_locked },
	{ "needs replacement", st_media_status_needs_replacement },
	{ "new",               st_media_status_new },
	{ "pooled",            st_media_status_pooled },

	{ 0, st_media_status_unknown },
};


const char * st_media_format_data_to_string(enum st_media_format_data_type type) {
	const struct st_media_format_data_type2 * ptr;
	for (ptr = st_media_format_data_types; ptr->name; ptr++)
		if (ptr->type == type)
			return ptr->name;

	return "unknown";
}

const char * st_media_format_mode_to_string(enum st_media_format_mode mode) {
	const struct st_media_format_mode2 * ptr;
	for (ptr = st_media_format_modes; ptr->name; ptr++)
		if (ptr->mode == mode)
			return ptr->name;

	return "unknown";
}

const char * st_media_location_to_string(enum st_media_location location) {
	const struct st_media_location2 * ptr;
	for (ptr = st_media_locations; ptr->name; ptr++)
		if (ptr->location == location)
			return ptr->name;

	return "unknown";
}

const char * st_media_status_to_string(enum st_media_status status) {
	const struct st_media_status2 * ptr;
	for (ptr = st_media_status; ptr->name; ptr++)
		if (ptr->status == status)
			return ptr->name;

	return "unknown";
}

enum st_media_location st_media_string_to_location(const char * location) {
	if (!location)
		return st_media_location_unknown;

	const struct st_media_location2 * ptr;
	for (ptr = st_media_locations; ptr->name; ptr++)
		if (!strcmp(location, ptr->name))
			return ptr->location;

	return ptr->location;
}

enum st_media_status st_media_string_to_status(const char * status) {
	if (!status)
		return st_media_status_unknown;

	const struct st_media_status2 * ptr;
	for (ptr = st_media_status; ptr->name; ptr++)
		if (!strcmp(status, ptr->name))
			return ptr->status;

	return ptr->status;
}

enum st_media_format_data_type st_media_string_to_format_data(const char * type) {
	if (!type)
		return st_media_format_data_unknown;

	const struct st_media_format_data_type2 * ptr;
	for (ptr = st_media_format_data_types; ptr->name; ptr++)
		if (!strcmp(type, ptr->name))
			return ptr->type;

	return ptr->type;
}

enum st_media_format_mode st_media_string_to_format_mode(const char * mode) {
	if (!mode)
		return st_media_format_mode_unknown;

	const struct st_media_format_mode2 * ptr;
	for (ptr = st_media_format_modes; ptr->name; ptr++)
		if (!strcmp(mode, ptr->name))
			return ptr->mode;

	return ptr->mode;
}

