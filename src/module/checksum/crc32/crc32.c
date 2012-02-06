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
*  Last modified: Sun, 18 Dec 2011 11:25:16 +0100                         *
\*************************************************************************/

// free, malloc
#include <malloc.h>
// strdup
#include <string.h>
// crc32
#include <zlib.h>

#include <stone/checksum.h>

struct st_checksum_crc32_private {
	uLong crc32;
	char digest[9];
};

static struct st_checksum * st_checksum_crc32_clone(struct st_checksum * new_checksum, struct st_checksum * current_checksum);
static char * st_checksum_crc32_digest(struct st_checksum * checksum);
static void st_checksum_crc32_free(struct st_checksum * checksum);
static struct st_checksum * st_checksum_crc32_new_checksum(struct st_checksum * checksum);
static void st_checksum_crc32_init(void) __attribute__((constructor));
static ssize_t st_checksum_crc32_update(struct st_checksum * checksum, const void * data, ssize_t length);

static struct st_checksum_driver st_checksum_crc32_driver = {
	.name			= "crc32",
	.new_checksum	= st_checksum_crc32_new_checksum,
	.cookie			= 0,
	.api_version    = STONE_CHECKSUM_APIVERSION,
};

static struct st_checksum_ops st_checksum_crc32_ops = {
	.clone	= st_checksum_crc32_clone,
	.digest	= st_checksum_crc32_digest,
	.free	= st_checksum_crc32_free,
	.update	= st_checksum_crc32_update,
};


struct st_checksum * st_checksum_crc32_clone(struct st_checksum * new_checksum, struct st_checksum * current_checksum) {
	if (!current_checksum)
		return 0;

	struct st_checksum_crc32_private * current_self = current_checksum->data;

	if (!new_checksum)
		new_checksum = malloc(sizeof(struct st_checksum));

	new_checksum->ops = &st_checksum_crc32_ops;
	new_checksum->driver = &st_checksum_crc32_driver;

	struct st_checksum_crc32_private * new_self = malloc(sizeof(struct st_checksum_crc32_private));
	*new_self = *current_self;

	new_checksum->data = new_self;
	return new_checksum;
}

char * st_checksum_crc32_digest(struct st_checksum * checksum) {
	if (!checksum)
		return 0;

	struct st_checksum_crc32_private * self = checksum->data;

	if (self->digest[0] != '\0')
		return strdup(self->digest);

	unsigned char digest[4] = {
		(self->crc32 >> 24) & 0xFF,
		(self->crc32 >> 16) & 0xFF,
		(self->crc32 >> 8)  & 0xFF,
		self->crc32         & 0xFF,
	};

	st_checksum_convert_to_hex(digest, 4, self->digest);

	return strdup(self->digest);
}

void st_checksum_crc32_free(struct st_checksum * checksum) {
	if (!checksum)
		return;

	struct st_checksum_crc32_private * self = checksum->data;

	if (self)
		free(self);

	checksum->data = 0;
	checksum->ops = 0;
	checksum->driver = 0;
}

void st_checksum_crc32_init() {
	st_checksum_register_driver(&st_checksum_crc32_driver);
}

struct st_checksum * st_checksum_crc32_new_checksum(struct st_checksum * checksum) {
	if (!checksum)
		checksum = malloc(sizeof(struct st_checksum));

	checksum->ops = &st_checksum_crc32_ops;
	checksum->driver = &st_checksum_crc32_driver;

	struct st_checksum_crc32_private * self = malloc(sizeof(struct st_checksum_crc32_private));
	self->crc32 = crc32(0L, Z_NULL, 0);
	*self->digest = '\0';

	checksum->data = self;
	return checksum;
}

ssize_t st_checksum_crc32_update(struct st_checksum * checksum, const void * data, ssize_t length) {
	if (!checksum || !data || length < 1)
		return -1;

	struct st_checksum_crc32_private * self = checksum->data;
	if (*self->digest != '\0')
		return -2;

	self->crc32 = crc32(self->crc32, data, length);
	return length;
}

