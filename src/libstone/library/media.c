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
*  Last modified: Fri, 03 Aug 2012 23:56:42 +0200                         *
\*************************************************************************/

// pthread_mutex_lock, pthread_mutex_unlock
#include <pthread.h>
// malloc, realloc
#include <stdlib.h>
// memcpy, strcmp
#include <string.h>
// bzero
#include <strings.h>

#include <libstone/database.h>
#include <libstone/library/media.h>


static pthread_mutex_t st_media_lock = PTHREAD_MUTEX_INITIALIZER;
static struct st_media ** st_medias = 0;
static unsigned int st_media_nb_medias = 0;

static pthread_mutex_t st_media_format_lock = PTHREAD_MUTEX_INITIALIZER;
static struct st_media_format ** st_media_formats = 0;
static unsigned int st_media_format_nb_formats = 0;

static void st_media_add(struct st_media * media);
static int st_media_retrieve(struct st_media ** media, const char * uuid, const char * medium_serial_number, const char * label);

static int st_media_format_retrieve(struct st_media_format ** media_format, unsigned char density_code, enum st_media_format_mode mode);


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


void st_media_add(struct st_media * media) {
	void * new_addr = realloc(st_medias, (st_media_nb_medias + 1) * sizeof(struct st_media *));
	if (new_addr) {
		st_medias = new_addr;
		st_medias[st_media_nb_medias] = media;
		st_media_nb_medias++;
	}
}

struct st_media * st_media_get_by_label(const char * label) {
	if (!label)
		return 0;

	pthread_mutex_lock(&st_media_lock);

	struct st_media * media = 0;
	unsigned int i;
	for (i = 0; i < st_media_nb_medias; i++)
		if (!strcmp(label, st_medias[i]->label)) {
			media = st_medias[i];
			break;
		}

	if (!media && st_media_retrieve(&media, 0, 0, label))
		st_media_add(media);

	pthread_mutex_unlock(&st_media_lock);

	return media;
}

struct st_media * st_media_get_by_medium_serial_number(const char * medium_serial_number) {
	if (!medium_serial_number)
		return 0;

	pthread_mutex_lock(&st_media_lock);

	struct st_media * media = 0;
	unsigned int i;
	for (i = 0; i < st_media_nb_medias; i++)
		if (!strcmp(medium_serial_number, st_medias[i]->medium_serial_number)) {
			media = st_medias[i];
			break;
		}

	if (!media && st_media_retrieve(&media, 0, medium_serial_number, 0))
		st_media_add(media);

	pthread_mutex_unlock(&st_media_lock);

	return media;
}

struct st_media * st_media_get_by_uuid(const char * uuid) {
	if (!uuid)
		return 0;

	pthread_mutex_lock(&st_media_lock);

	struct st_media * media = 0;
	unsigned int i;
	for (i = 0; i < st_media_nb_medias; i++)
		if (!strcmp(uuid, st_medias[i]->uuid)) {
			media = st_medias[i];
			break;
		}

	if (!media && st_media_retrieve(&media, uuid, 0, 0))
		st_media_add(media);

	pthread_mutex_unlock(&st_media_lock);

	return media;
}

int st_media_retrieve(struct st_media ** media, const char * uuid, const char * medium_serial_number, const char * label) {
	struct st_database * db = st_database_get_default_driver();
	struct st_database_config * config = 0;
	struct st_database_connection * connect = 0;

	if (db)
		config = db->ops->get_default_config();

	if (config)
		connect = config->ops->connect(config);

	if (connect) {
		struct st_media media2;
		bzero(&media2, sizeof(struct st_media));

		short ok = 0;
		if (!connect->ops->get_media(connect, &media2, uuid, medium_serial_number, label)) {
			if (!*media)
				*media = malloc(sizeof(struct st_media));
			memcpy(*media, &media2, sizeof(struct st_media));
			ok = 1;
		}

		connect->ops->close(connect);
		connect->ops->free(connect);

		return ok;
	}

	return 0;
}


struct st_media_format * st_media_format_get_by_density_code(unsigned char density_code, enum st_media_format_mode mode) {
	pthread_mutex_lock(&st_media_format_lock);

	struct st_media_format * media_format = 0;
	unsigned int i;
	for (i = 0; i < st_media_format_nb_formats; i++)
		if (st_media_formats[i]->density_code == density_code && st_media_formats[i]->mode == mode) {
			media_format = st_media_formats[i];
			break;
		}

	if (!media_format && st_media_format_retrieve(&media_format, density_code, mode)) {
		void * new_addr = realloc(st_media_formats, (st_media_format_nb_formats + 1) * sizeof(struct st_media_format *));
		if (new_addr) {
			st_media_formats = new_addr;
			st_media_formats[st_media_format_nb_formats] = media_format;
			st_media_format_nb_formats++;
		}
	}

	pthread_mutex_unlock(&st_media_format_lock);

	return media_format;
}

int st_media_format_retrieve(struct st_media_format ** media_format, unsigned char density_code, enum st_media_format_mode mode) {
	struct st_database * db = st_database_get_default_driver();
	struct st_database_config * config = 0;
	struct st_database_connection * connect = 0;

	if (db)
		config = db->ops->get_default_config();

	if (config)
		connect = config->ops->connect(config);

	if (connect) {
		struct st_media_format format;
		bzero(&format, sizeof(struct st_media_format));

		short ok = 0;
		if (!connect->ops->get_media_format(connect, &format, density_code, mode)) {
			if (!*media_format)
				*media_format = malloc(sizeof(struct st_media_format));
			memcpy(*media_format, &format, sizeof(struct st_media_format));
			ok = 1;
		}

		connect->ops->close(connect);
		connect->ops->free(connect);

		return ok;
	}

	return 0;
}

