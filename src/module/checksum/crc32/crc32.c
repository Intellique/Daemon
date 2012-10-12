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
*  Last modified: Sat, 13 Oct 2012 00:06:42 +0200                         *
\*************************************************************************/

// free, malloc
#include <malloc.h>
// strdup
#include <string.h>
// crc32
#include <zlib.h>

#include <libstone/checksum.h>

struct st_checksum_crc32_private {
	uLong crc32;
	char digest[9];
};

static char * st_checksum_crc32_digest(struct st_checksum * checksum);
static void st_checksum_crc32_free(struct st_checksum * checksum);
static struct st_checksum * st_checksum_crc32_new_checksum(void);
static void st_checksum_crc32_init(void) __attribute__((constructor));
static ssize_t st_checksum_crc32_update(struct st_checksum * checksum, const void * data, ssize_t length);

static struct st_checksum_driver st_checksum_crc32_driver = {
	.name			= "crc32",
	.new_checksum	= st_checksum_crc32_new_checksum,
	.cookie			= NULL,
	.api_level      = {
		.checksum = STONE_CHECKSUM_API_LEVEL,
		.database = 0,
		.job      = 0,
	},
};

static struct st_checksum_ops st_checksum_crc32_ops = {
	.digest	= st_checksum_crc32_digest,
	.free	= st_checksum_crc32_free,
	.update	= st_checksum_crc32_update,
};


static char * st_checksum_crc32_digest(struct st_checksum * checksum) {
	if (checksum == NULL)
		return NULL;

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

static void st_checksum_crc32_free(struct st_checksum * checksum) {
	if (checksum == NULL)
		return;

	free(checksum->data);

	checksum->data = NULL;
	checksum->ops = NULL;
	checksum->driver = NULL;

	free(checksum);
}

static void st_checksum_crc32_init(void) {
	st_checksum_register_driver(&st_checksum_crc32_driver);
}

static struct st_checksum * st_checksum_crc32_new_checksum(void) {
	struct st_checksum * checksum = malloc(sizeof(struct st_checksum));
	checksum->ops = &st_checksum_crc32_ops;
	checksum->driver = &st_checksum_crc32_driver;

	struct st_checksum_crc32_private * self = malloc(sizeof(struct st_checksum_crc32_private));
	self->crc32 = crc32(0L, Z_NULL, 0);
	*self->digest = '\0';

	checksum->data = self;
	return checksum;
}

static ssize_t st_checksum_crc32_update(struct st_checksum * checksum, const void * data, ssize_t length) {
	if (checksum == NULL || data == NULL || length < 1)
		return -1;

	struct st_checksum_crc32_private * self = checksum->data;
	self->crc32 = crc32(self->crc32, data, length);
	*self->digest = '\0';

	return length;
}

