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
*  Last modified: Tue, 13 Mar 2012 18:57:46 +0100                         *
\*************************************************************************/

// free, malloc
#include <malloc.h>
// WHIRLPOOL_Final, WHIRLPOOL_Init, WHIRLPOOL_Update
#include <openssl/whrlpool.h>
// strdup
#include <string.h>

#include <stone/checksum.h>

struct st_checksum_whirlpool_private {
	WHIRLPOOL_CTX whirlpool;
	char digest[WHIRLPOOL_DIGEST_LENGTH * 2 + 1];
};

static char * st_checksum_whirlpool_digest(struct st_checksum * checksum);
static void st_checksum_whirlpool_free(struct st_checksum * checksum);
static struct st_checksum * st_checksum_whirlpool_new_checksum(struct st_checksum * checksum);
static void st_checksum_whirlpool_init(void) __attribute__((constructor));
static ssize_t st_checksum_whirlpool_update(struct st_checksum * checksum, const void * data, ssize_t length);

static struct st_checksum_driver st_checksum_whirlpool_driver = {
	.name			= "whirlpool",
	.new_checksum	= st_checksum_whirlpool_new_checksum,
	.cookie			= 0,
	.api_version    = STONE_CHECKSUM_APIVERSION,
};

static struct st_checksum_ops st_checksum_whirlpool_ops = {
	.digest	= st_checksum_whirlpool_digest,
	.free	= st_checksum_whirlpool_free,
	.update	= st_checksum_whirlpool_update,
};


char * st_checksum_whirlpool_digest(struct st_checksum * checksum) {
	if (!checksum)
		return 0;

	struct st_checksum_whirlpool_private * self = checksum->data;

	if (self->digest[0] != '\0')
		return strdup(self->digest);

	unsigned char digest[WHIRLPOOL_DIGEST_LENGTH];
	if (!WHIRLPOOL_Final(digest, &self->whirlpool))
		return 0;

	st_checksum_convert_to_hex(digest, WHIRLPOOL_DIGEST_LENGTH, self->digest);

	return strdup(self->digest);
}

void st_checksum_whirlpool_free(struct st_checksum * checksum) {
	if (!checksum)
		return;

	struct st_checksum_whirlpool_private * self = checksum->data;

	if (self)
		free(self);

	checksum->data = 0;
	checksum->ops = 0;
	checksum->driver = 0;
}

void st_checksum_whirlpool_init() {
	st_checksum_register_driver(&st_checksum_whirlpool_driver);
}

struct st_checksum * st_checksum_whirlpool_new_checksum(struct st_checksum * checksum) {
	if (!checksum)
		checksum = malloc(sizeof(struct st_checksum));

	checksum->ops = &st_checksum_whirlpool_ops;
	checksum->driver = &st_checksum_whirlpool_driver;

	struct st_checksum_whirlpool_private * self = malloc(sizeof(struct st_checksum_whirlpool_private));
	WHIRLPOOL_Init(&self->whirlpool);
	*self->digest = '\0';

	checksum->data = self;
	return checksum;
}

ssize_t st_checksum_whirlpool_update(struct st_checksum * checksum, const void * data, ssize_t length) {
	if (!checksum || !data || length < 1)
		return -1;

	struct st_checksum_whirlpool_private * self = checksum->data;
	if (*self->digest != '\0')
		return -2;

	if (WHIRLPOOL_Update(&self->whirlpool, data, length))
		return length;

	return -1;
}

