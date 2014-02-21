/****************************************************************************\
*                             __________                                     *
*                            / __/_  __/__  ___  ___                         *
*                           _\ \  / / / _ \/ _ \/ -_)                        *
*                          /___/ /_/  \___/_//_/\__/                         *
*                                                                            *
*  ------------------------------------------------------------------------  *
*  This file is a part of STone                                              *
*                                                                            *
*  STone is free software; you can redistribute it and/or modify             *
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
*  Last modified: Fri, 21 Feb 2014 10:51:05 +0100                            *
\****************************************************************************/

// free, malloc
#include <stdlib.h>
// SHA384_Final, SHA384_Init, SHA384_Update
#include <openssl/sha.h>
// strdup
#include <string.h>

#include <libstone/checksum.h>

#include <libchecksum-sha384.chcksum>

struct st_checksum_sha384_private {
	SHA512_CTX sha384;
	char digest[SHA384_DIGEST_LENGTH * 2 + 1];
};

static char * st_checksum_sha384_digest(struct st_checksum * checksum);
static void st_checksum_sha384_free(struct st_checksum * checksum);
static struct st_checksum * st_checksum_sha384_new_checksum(void);
static void st_checksum_sha384_init(void) __attribute__((constructor));
static ssize_t st_checksum_sha384_update(struct st_checksum * checksum, const void * data, ssize_t length);

static struct st_checksum_driver st_checksum_sha384_driver = {
	.name			  = "sha384",
	.default_checksum = false,
	.new_checksum	  = st_checksum_sha384_new_checksum,
	.cookie			  = NULL,
	.api_level        = {
		.checksum = STONE_CHECKSUM_API_LEVEL,
		.database = 0,
		.job      = 0,
	},
	.src_checksum     = STONE_CHECKSUM_SHA384_SRCSUM,
};

static struct st_checksum_ops st_checksum_sha384_ops = {
	.digest	= st_checksum_sha384_digest,
	.free	= st_checksum_sha384_free,
	.update	= st_checksum_sha384_update,
};


static char * st_checksum_sha384_digest(struct st_checksum * checksum) {
	if (checksum == NULL)
		return NULL;

	struct st_checksum_sha384_private * self = checksum->data;
	if (self->digest[0] != '\0')
		return strdup(self->digest);

	SHA512_CTX sha384 = self->sha384;
	unsigned char digest[SHA384_DIGEST_LENGTH];
	if (!SHA384_Final(digest, &sha384))
		return NULL;

	st_checksum_convert_to_hex(digest, SHA384_DIGEST_LENGTH, self->digest);

	return strdup(self->digest);
}

static void st_checksum_sha384_free(struct st_checksum * checksum) {
	if (checksum == NULL)
		return;

	struct st_checksum_sha384_private * self = checksum->data;

	unsigned char digest[SHA384_DIGEST_LENGTH];
	SHA384_Final(digest, &self->sha384);

	free(self);

	checksum->data = NULL;
	checksum->ops = NULL;
	checksum->driver = NULL;

	free(checksum);
}

static void st_checksum_sha384_init(void) {
	st_checksum_register_driver(&st_checksum_sha384_driver);
}

static struct st_checksum * st_checksum_sha384_new_checksum(void) {
	struct st_checksum * checksum = malloc(sizeof(struct st_checksum));
	checksum->ops = &st_checksum_sha384_ops;
	checksum->driver = &st_checksum_sha384_driver;

	struct st_checksum_sha384_private * self = malloc(sizeof(struct st_checksum_sha384_private));
	SHA384_Init(&self->sha384);
	*self->digest = '\0';

	checksum->data = self;
	return checksum;
}

static ssize_t st_checksum_sha384_update(struct st_checksum * checksum, const void * data, ssize_t length) {
	if (checksum == NULL || data == NULL || length < 1)
		return -1;

	struct st_checksum_sha384_private * self = checksum->data;
	if (SHA384_Update(&self->sha384, data, length)) {
		*self->digest = '\0';
		return length;
	}

	return -1;
}

