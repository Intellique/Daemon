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
*  Copyright (C) 2013-2016, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// free, malloc
#include <stdlib.h>
// sha256_digest, sha256_init, sha256_update
#include <nettle/sha.h>
// strdup
#include <string.h>

#include <libstoriqone/checksum.h>

#include <libchecksum-sha256.chcksum>

struct so_checksum_sha256_private {
	struct sha256_ctx sha256;
	char digest[SHA256_DIGEST_SIZE * 2 + 1];
};

static char * so_checksum_sha256_digest(struct so_checksum * checksum);
static void so_checksum_sha256_free(struct so_checksum * checksum);
static struct so_checksum * so_checksum_sha256_new_checksum(void);
static void so_checksum_sha256_init(void) __attribute__((constructor));
static void so_checksum_sha256_reset(struct so_checksum * checksum);
static ssize_t so_checksum_sha256_update(struct so_checksum * checksum, const void * data, ssize_t length);

static struct so_checksum_driver so_checksum_sha256_driver = {
	.name			  = "sha256",
	.default_checksum = false,
	.new_checksum	  = so_checksum_sha256_new_checksum,
	.cookie			  = NULL,
	.src_checksum     = STORIQONE_CHECKSUM_SHA256_SRCSUM,
};

static struct so_checksum_ops so_checksum_sha256_ops = {
	.digest	= so_checksum_sha256_digest,
	.free	= so_checksum_sha256_free,
	.reset  = so_checksum_sha256_reset,
	.update	= so_checksum_sha256_update,
};


static char * so_checksum_sha256_digest(struct so_checksum * checksum) {
	if (checksum == NULL)
		return NULL;

	struct so_checksum_sha256_private * self = checksum->data;
	if (self->digest[0] != '\0')
		return strdup(self->digest);

	struct sha256_ctx sha256 = self->sha256;
	unsigned char digest[SHA256_DIGEST_SIZE];
	sha256_digest(&sha256, SHA256_DIGEST_SIZE, digest);

	so_checksum_convert_to_hex(digest, SHA256_DIGEST_SIZE, self->digest);

	return strdup(self->digest);
}

static void so_checksum_sha256_free(struct so_checksum * checksum) {
	if (checksum == NULL)
		return;

	struct so_checksum_sha256_private * self = checksum->data;

	unsigned char digest[SHA256_DIGEST_SIZE];
	sha256_digest(&self->sha256, SHA256_DIGEST_SIZE, digest);

	free(self);

	checksum->data = NULL;
	checksum->ops = NULL;
	checksum->driver = NULL;

	free(checksum);
}

static void so_checksum_sha256_init(void) {
	so_checksum_register_driver(&so_checksum_sha256_driver);
}

static struct so_checksum * so_checksum_sha256_new_checksum(void) {
	struct so_checksum * checksum = malloc(sizeof(struct so_checksum));
	checksum->ops = &so_checksum_sha256_ops;
	checksum->driver = &so_checksum_sha256_driver;

	struct so_checksum_sha256_private * self = malloc(sizeof(struct so_checksum_sha256_private));
	sha256_init(&self->sha256);
	*self->digest = '\0';

	checksum->data = self;
	return checksum;
}

static void so_checksum_sha256_reset(struct so_checksum * checksum) {
	if (checksum == NULL)
		return;

	struct so_checksum_sha256_private * self = checksum->data;
	sha256_init(&self->sha256);
	*self->digest = '\0';
}

static ssize_t so_checksum_sha256_update(struct so_checksum * checksum, const void * data, ssize_t length) {
	if (checksum == NULL || data == NULL || length < 1)
		return -1;

	struct so_checksum_sha256_private * self = checksum->data;
	sha256_update(&self->sha256, length, data);
	return length;
}

