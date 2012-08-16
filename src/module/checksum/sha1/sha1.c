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
*  Last modified: Thu, 16 Aug 2012 23:02:11 +0200                         *
\*************************************************************************/

// free, malloc
#include <malloc.h>
// SHA1_Final, SHA1_Init, SHA1_Update
#include <openssl/sha.h>
// strdup
#include <string.h>

#include <libstone/checksum.h>

struct st_checksum_sha1_private {
	SHA_CTX sha1;
	char digest[SHA_DIGEST_LENGTH * 2 + 1];
};

static char * st_checksum_sha1_digest(struct st_checksum * checksum);
static void st_checksum_sha1_free(struct st_checksum * checksum);
static struct st_checksum * st_checksum_sha1_new_checksum(void);
static void st_checksum_sha1_init(void) __attribute__((constructor));
static ssize_t st_checksum_sha1_update(struct st_checksum * checksum, const void * data, ssize_t length);

static struct st_checksum_driver st_checksum_sha1_driver = {
	.name			= "sha1",
	.new_checksum	= st_checksum_sha1_new_checksum,
	.cookie			= NULL,
	.api_level      = STONE_CHECKSUM_API_LEVEL,
};

static struct st_checksum_ops st_checksum_sha1_ops = {
	.digest	= st_checksum_sha1_digest,
	.free	= st_checksum_sha1_free,
	.update	= st_checksum_sha1_update,
};


static char * st_checksum_sha1_digest(struct st_checksum * checksum) {
	if (checksum == NULL)
		return NULL;

	struct st_checksum_sha1_private * self = checksum->data;
	if (self->digest[0] != '\0')
		return strdup(self->digest);

	SHA_CTX sha1 = self->sha1;
	unsigned char digest[SHA_DIGEST_LENGTH];
	if (!SHA1_Final(digest, &sha1))
		return NULL;

	st_checksum_convert_to_hex(digest, SHA_DIGEST_LENGTH, self->digest);

	return strdup(self->digest);
}

static void st_checksum_sha1_free(struct st_checksum * checksum) {
	if (checksum == NULL)
		return;

	struct st_checksum_sha1_private * self = checksum->data;

	unsigned char digest[SHA_DIGEST_LENGTH];
	SHA1_Final(digest, &self->sha1);

	free(self);

	checksum->data = NULL;
	checksum->ops = NULL;
	checksum->driver = NULL;
}

static void st_checksum_sha1_init(void) {
	st_checksum_register_driver(&st_checksum_sha1_driver);
}

static struct st_checksum * st_checksum_sha1_new_checksum(void) {
	struct st_checksum * checksum = malloc(sizeof(struct st_checksum));
	checksum->ops = &st_checksum_sha1_ops;
	checksum->driver = &st_checksum_sha1_driver;

	struct st_checksum_sha1_private * self = malloc(sizeof(struct st_checksum_sha1_private));
	SHA1_Init(&self->sha1);
	*self->digest = '\0';

	checksum->data = self;
	return checksum;
}

static ssize_t st_checksum_sha1_update(struct st_checksum * checksum, const void * data, ssize_t length) {
	if (checksum == NULL || data == NULL || length < 1)
		return -1;

	struct st_checksum_sha1_private * self = checksum->data;
	if (SHA1_Update(&self->sha1, data, length)) {
		*self->digest = '\0';
		return length;
	}

	return -1;
}

