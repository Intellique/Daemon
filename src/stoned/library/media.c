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
*  Last modified: Tue, 31 Jul 2012 22:42:37 +0200                         *
\*************************************************************************/

// pthread_mutex_lock, pthread_mutex_unlock
#include <pthread.h>
// malloc, realloc
#include <stdlib.h>
// strcmp
#include <string.h>
// bzero
#include <strings.h>

#include <libstone/database.h>
#include <stoned/library/media.h>

static pthread_mutex_t st_media_lock = PTHREAD_MUTEX_INITIALIZER;
static struct st_media ** st_medias = 0;
static unsigned int st_media_nb_medias = 0;

static pthread_mutex_t st_media_format_lock = PTHREAD_MUTEX_INITIALIZER;
static struct st_media_format ** st_media_formats = 0;
static unsigned int st_media_format_nb_formats = 0;

static void st_media_add(struct st_media * media);
static int st_media_retrieve(struct st_media ** media, const char * uuid, const char * medium_serial_number, const char * label);

static int st_media_format_retrieve(struct st_media_format ** media_format, unsigned char density_code, enum st_media_format_mode mode);

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
		if (!*media)
			*media = malloc(sizeof(struct st_media));
		bzero(*media, sizeof(struct st_media));

		// TODO

		connect->ops->close(connect);
		connect->ops->free(connect);

		return 1;
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
		if (!*media_format)
			*media_format = malloc(sizeof(struct st_media_format));
		bzero(*media_format, sizeof(struct st_media_format));

		// TODO

		connect->ops->close(connect);
		connect->ops->free(connect);

		return 1;
	}

	return 0;
}

