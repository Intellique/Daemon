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
// sha224_digest, sha224_init, sha224_update
#include <nettle/sha2.h>
// strdup
#include <string.h>

#include <libstoriqone/checksum.h>

#include <libchecksum-sha224.chcksum>

struct so_checksum_sha224_private {
	struct sha256_ctx sha224;
	char digest[SHA224_DIGEST_SIZE * 2 + 1];
};

static char * so_checksum_sha224_digest(struct so_checksum * checksum);
static void so_checksum_sha224_free(struct so_checksum * checksum);
static struct so_checksum * so_checksum_sha224_new_checksum(void);
static void so_checksum_sha224_init(void) __attribute__((constructor));
static void so_checksum_sha224_reset(struct so_checksum * checksum);
static ssize_t so_checksum_sha224_update(struct so_checksum * checksum, const void * data, ssize_t length);

static struct so_checksum_driver so_checksum_sha224_driver = {
	.name			  = "sha224",
	.default_checksum = false,
	.new_checksum	  = so_checksum_sha224_new_checksum,
	.cookie			  = NULL,
	.src_checksum     = STORIQONE_CHECKSUM_SHA224_SRCSUM,
};

static struct so_checksum_ops so_checksum_sha224_ops = {
	.digest	= so_checksum_sha224_digest,
	.free	= so_checksum_sha224_free,
	.reset  = so_checksum_sha224_reset,
	.update	= so_checksum_sha224_update,
};


static char * so_checksum_sha224_digest(struct so_checksum * checksum) {
	if (checksum == NULL)
		return NULL;

	struct so_checksum_sha224_private * self = checksum->data;
	if (self->digest[0] != '\0')
		return strdup(self->digest);

	struct sha224_ctx sha224 = self->sha224;
	unsigned char digest[SHA224_DIGEST_SIZE];
	sha224_digest(&sha224, SHA224_DIGEST_SIZE, digest);

	so_checksum_convert_to_hex(digest, SHA224_DIGEST_SIZE, self->digest);

	return strdup(self->digest);
}

static void so_checksum_sha224_free(struct so_checksum * checksum) {
	if (checksum == NULL)
		return;

	struct so_checksum_sha224_private * self = checksum->data;

	unsigned char digest[SHA224_DIGEST_SIZE];
	sha224_digest(&self->sha224, SHA224_DIGEST_SIZE, digest);

	free(self);

	checksum->data = NULL;
	checksum->ops = NULL;
	checksum->driver = NULL;

	free(checksum);
}

static void so_checksum_sha224_init(void) {
	so_checksum_register_driver(&so_checksum_sha224_driver);
}

static struct so_checksum * so_checksum_sha224_new_checksum(void) {
	struct so_checksum * checksum = malloc(sizeof(struct so_checksum));
	checksum->ops = &so_checksum_sha224_ops;
	checksum->driver = &so_checksum_sha224_driver;

	struct so_checksum_sha224_private * self = malloc(sizeof(struct so_checksum_sha224_private));
	sha224_init(&self->sha224);
	*self->digest = '\0';

	checksum->data = self;
	return checksum;
}

static void so_checksum_sha224_reset(struct so_checksum * checksum) {
	if (checksum == NULL)
		return;

	struct so_checksum_sha224_private * self = checksum->data;
	sha224_init(&self->sha224);
	*self->digest = '\0';
}

static ssize_t so_checksum_sha224_update(struct so_checksum * checksum, const void * data, ssize_t length) {
	if (checksum == NULL || data == NULL || length < 1)
		return -1;

	struct so_checksum_sha224_private * self = checksum->data;
	sha224_update(&self->sha224, length, data);
	return length;
}

