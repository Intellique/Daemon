/****************************************************************************\
*                    ______           _      ____                            *
*                   / __/ /____  ____(_)__ _/ __ \___  ___                   *
*                  _\ \/ __/ _ \/ __/ / _ `/ /_/ / _ \/ -_)                  *
*                 /___/\__/\___/_/ /_/\_, /\____/_//_/\__/                   *
*                                      /_/                                   *
*  ------------------------------------------------------------------------  *
*  This file is a part of Storiq One                                         *
*                                                                            *
*  Storiq One is free software; you can redistribute it and/or modify        *
*  it under the terms of the GNU Affero General Public License               *
*  as published by the Free Software Foundation; either version 3            *
*  of the License, or (at your option) any later version.                    *
*                                                                            *
*  This program is distributed in the hope that it will be useful,           *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of            *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
*  GNU Affero General Public License for more details.                       *
*                                                                            *
*  You should have received a copy of the GNU Affero General Public License  *
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.     *
*                                                                            *
*  ------------------------------------------------------------------------  *
*  Copyright (C) 2013-2015, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// free, malloc
#include <stdlib.h>
// strdup
#include <string.h>
// crc32
#include <zlib.h>

#include <libstoriqone/checksum.h>

#include <libchecksum-crc32.chcksum>

struct so_checksum_crc32_private {
	uLong crc32;
	char digest[9];
};

static char * so_checksum_crc32_digest(struct so_checksum * checksum);
static void so_checksum_crc32_free(struct so_checksum * checksum);
static struct so_checksum * so_checksum_crc32_new_checksum(void);
static void so_checksum_crc32_init(void) __attribute__((constructor));
static ssize_t so_checksum_crc32_update(struct so_checksum * checksum, const void * data, ssize_t length);

static struct so_checksum_driver so_checksum_crc32_driver = {
	.name			  = "crc32",
	.default_checksum = false,
	.new_checksum	  = so_checksum_crc32_new_checksum,
	.cookie			  = NULL,
	.src_checksum     = STORIQONE_CHECKSUM_CRC32_SRCSUM,
};

static struct so_checksum_ops so_checksum_crc32_ops = {
	.digest	= so_checksum_crc32_digest,
	.free	= so_checksum_crc32_free,
	.update	= so_checksum_crc32_update,
};


static char * so_checksum_crc32_digest(struct so_checksum * checksum) {
	if (checksum == NULL)
		return NULL;

	struct so_checksum_crc32_private * self = checksum->data;
	if (self->digest[0] != '\0')
		return strdup(self->digest);

	unsigned char digest[4] = {
		(self->crc32 >> 24) & 0xFF,
		(self->crc32 >> 16) & 0xFF,
		(self->crc32 >> 8)  & 0xFF,
		self->crc32         & 0xFF,
	};

	so_checksum_convert_to_hex(digest, 4, self->digest);

	return strdup(self->digest);
}

static void so_checksum_crc32_free(struct so_checksum * checksum) {
	if (checksum == NULL)
		return;

	free(checksum->data);

	checksum->data = NULL;
	checksum->ops = NULL;
	checksum->driver = NULL;

	free(checksum);
}

static void so_checksum_crc32_init(void) {
	so_checksum_register_driver(&so_checksum_crc32_driver);
}

static struct so_checksum * so_checksum_crc32_new_checksum(void) {
	struct so_checksum * checksum = malloc(sizeof(struct so_checksum));
	checksum->ops = &so_checksum_crc32_ops;
	checksum->driver = &so_checksum_crc32_driver;

	struct so_checksum_crc32_private * self = malloc(sizeof(struct so_checksum_crc32_private));
	self->crc32 = crc32(0L, Z_NULL, 0);
	*self->digest = '\0';

	checksum->data = self;
	return checksum;
}

static ssize_t so_checksum_crc32_update(struct so_checksum * checksum, const void * data, ssize_t length) {
	if (checksum == NULL || data == NULL || length < 1)
		return -1;

	struct so_checksum_crc32_private * self = checksum->data;
	self->crc32 = crc32(self->crc32, data, length);
	*self->digest = '\0';

	return length;
}

