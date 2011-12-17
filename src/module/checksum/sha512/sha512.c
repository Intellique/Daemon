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
*  Copyright (C) 2011, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Sat, 17 Dec 2011 17:41:45 +0100                         *
\*************************************************************************/

// free, malloc
#include <malloc.h>
// SHA512_Final, SHA512_Init, SHA512_Update
#include <openssl/sha.h>
// strdup
#include <string.h>

#include <stone/checksum.h>

struct st_checksum_sha512_private {
	SHA512_CTX sha512;
	char digest[SHA512_DIGEST_LENGTH * 2 + 1];
};

static struct st_checksum * st_checksum_sha512_clone(struct st_checksum * new_checksum, struct st_checksum * current_checksum);
static char * st_checksum_sha512_digest(struct st_checksum * checksum);
static void st_checksum_sha512_free(struct st_checksum * checksum);
static struct st_checksum * st_checksum_sha512_new_checksum(struct st_checksum * checksum);
static void st_checksum_sha512_init(void) __attribute__((constructor));
static ssize_t st_checksum_sha512_update(struct st_checksum * checksum, const void * data, ssize_t length);

static struct st_checksum_driver st_checksum_sha512_driver = {
	.name			= "sha512",
	.new_checksum	= st_checksum_sha512_new_checksum,
	.cookie			= 0,
	.api_version    = STONE_CHECKSUM_APIVERSION,
};

static struct st_checksum_ops st_checksum_sha512_ops = {
	.clone	= st_checksum_sha512_clone,
	.digest	= st_checksum_sha512_digest,
	.free	= st_checksum_sha512_free,
	.update	= st_checksum_sha512_update,
};


struct st_checksum * st_checksum_sha512_clone(struct st_checksum * new_checksum, struct st_checksum * current_checksum) {
	if (!current_checksum)
		return 0;

	struct st_checksum_sha512_private * current_self = current_checksum->data;

	if (!new_checksum)
		new_checksum = malloc(sizeof(struct st_checksum));

	new_checksum->ops = &st_checksum_sha512_ops;
	new_checksum->driver = &st_checksum_sha512_driver;

	struct st_checksum_sha512_private * new_self = malloc(sizeof(struct st_checksum_sha512_private));
	*new_self = *current_self;

	new_checksum->data = new_self;
	return new_checksum;
}

char * st_checksum_sha512_digest(struct st_checksum * checksum) {
	if (!checksum)
		return 0;

	struct st_checksum_sha512_private * self = checksum->data;

	if (self->digest[0] != '\0')
		return strdup(self->digest);

	unsigned char digest[SHA512_DIGEST_LENGTH];
	if (!SHA512_Final(digest, &self->sha512))
		return 0;

	st_checksum_convert_to_hex(digest, SHA512_DIGEST_LENGTH, self->digest);

	return strdup(self->digest);
}

void st_checksum_sha512_free(struct st_checksum * checksum) {
	if (!checksum)
		return;

	struct st_checksum_sha512_private * self = checksum->data;

	if (self)
		free(self);

	checksum->data = 0;
	checksum->ops = 0;
	checksum->driver = 0;
}

void st_checksum_sha512_init() {
	st_checksum_register_driver(&st_checksum_sha512_driver);
}

struct st_checksum * st_checksum_sha512_new_checksum(struct st_checksum * checksum) {
	if (!checksum)
		checksum = malloc(sizeof(struct st_checksum));

	checksum->ops = &st_checksum_sha512_ops;
	checksum->driver = &st_checksum_sha512_driver;

	struct st_checksum_sha512_private * self = malloc(sizeof(struct st_checksum_sha512_private));
	SHA512_Init(&self->sha512);
	*self->digest = '\0';

	checksum->data = self;
	return checksum;
}

ssize_t st_checksum_sha512_update(struct st_checksum * checksum, const void * data, ssize_t length) {
	if (!checksum || !data || length < 1)
		return -1;

	struct st_checksum_sha512_private * self = checksum->data;
	if (*self->digest != '\0')
		return -2;

	if (SHA512_Update(&self->sha512, data, length))
		return length;

	return -1;
}

