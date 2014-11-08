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
*  Copyright (C) 2014, Clercin guillaume <gclercin@intellique.com>           *
\****************************************************************************/

// free, malloc
#include <stdlib.h>
// SHA224_Final, SHA224_Init, SHA224_Update
#include <openssl/sha.h>
// strdup
#include <string.h>

#include <libstoriqone/checksum.h>

#include <libchecksum-sha224.chcksum>

struct so_checksum_sha224_private {
	SHA256_CTX sha224;
	char digest[SHA224_DIGEST_LENGTH * 2 + 1];
};

static char * so_checksum_sha224_digest(struct so_checksum * checksum);
static void so_checksum_sha224_free(struct so_checksum * checksum);
static struct so_checksum * so_checksum_sha224_new_checksum(void);
static void so_checksum_sha224_init(void) __attribute__((constructor));
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
	.update	= so_checksum_sha224_update,
};


static char * so_checksum_sha224_digest(struct so_checksum * checksum) {
	if (checksum == NULL)
		return NULL;

	struct so_checksum_sha224_private * self = checksum->data;
	if (self->digest[0] != '\0')
		return strdup(self->digest);

	SHA256_CTX sha224 = self->sha224;
	unsigned char digest[SHA224_DIGEST_LENGTH];
	if (!SHA224_Final(digest, &sha224))
		return NULL;

	so_checksum_convert_to_hex(digest, SHA224_DIGEST_LENGTH, self->digest);

	return strdup(self->digest);
}

static void so_checksum_sha224_free(struct so_checksum * checksum) {
	if (checksum == NULL)
		return;

	struct so_checksum_sha224_private * self = checksum->data;

	unsigned char digest[SHA224_DIGEST_LENGTH];
	SHA224_Final(digest, &self->sha224);

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
	SHA224_Init(&self->sha224);
	*self->digest = '\0';

	checksum->data = self;
	return checksum;
}

static ssize_t so_checksum_sha224_update(struct so_checksum * checksum, const void * data, ssize_t length) {
	if (checksum == NULL || data == NULL || length < 1)
		return -1;

	struct so_checksum_sha224_private * self = checksum->data;
	if (SHA224_Update(&self->sha224, data, length)) {
		*self->digest = '\0';
		return length;
	}

	return -1;
}

