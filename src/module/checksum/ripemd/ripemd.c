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
*  Copyright (C) 2013, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Tue, 05 Feb 2013 21:44:00 +0100                         *
\*************************************************************************/

// free, malloc
#include <malloc.h>
// RIPEMD_Final, RIPEMD_Init, RIPEMD_Update
#include <openssl/ripemd.h>
// strdup
#include <string.h>

#include <libstone/checksum.h>

#include <libchecksum-ripemd.chcksum>

struct st_checksum_ripemd_private {
	RIPEMD160_CTX ripemd;
	char digest[RIPEMD160_DIGEST_LENGTH * 2 + 1];
};

static char * st_checksum_ripemd_digest(struct st_checksum * checksum);
static void st_checksum_ripemd_free(struct st_checksum * checksum);
static struct st_checksum * st_checksum_ripemd_new_checksum(void);
static void st_checksum_ripemd_init(void) __attribute__((constructor));
static ssize_t st_checksum_ripemd_update(struct st_checksum * checksum, const void * data, ssize_t length);

static struct st_checksum_driver st_checksum_ripemd_driver = {
	.name			= "ripemd",
	.new_checksum	= st_checksum_ripemd_new_checksum,
	.cookie			= NULL,
	.api_level      = {
		.checksum = STONE_CHECKSUM_API_LEVEL,
		.database = 0,
		.job      = 0,
	},
	.src_checksum   = STONE_CHECKSUM_RIPEMD_SRCSUM,
};

static struct st_checksum_ops st_checksum_ripemd_ops = {
	.digest	= st_checksum_ripemd_digest,
	.free	= st_checksum_ripemd_free,
	.update	= st_checksum_ripemd_update,
};


static char * st_checksum_ripemd_digest(struct st_checksum * checksum) {
	if (checksum == NULL)
		return NULL;

	struct st_checksum_ripemd_private * self = checksum->data;
	if (self->digest[0] != '\0')
		return strdup(self->digest);

	RIPEMD160_CTX ripemd = self->ripemd;
	unsigned char digest[RIPEMD160_DIGEST_LENGTH];
	if (!RIPEMD160_Final(digest, &ripemd))
		return NULL;

	st_checksum_convert_to_hex(digest, RIPEMD160_DIGEST_LENGTH, self->digest);

	return strdup(self->digest);
}

static void st_checksum_ripemd_free(struct st_checksum * checksum) {
	if (checksum == NULL)
		return;

	struct st_checksum_ripemd_private * self = checksum->data;

	unsigned char digest[RIPEMD160_DIGEST_LENGTH];
	RIPEMD160_Final(digest, &self->ripemd);

	free(self);

	checksum->data = NULL;
	checksum->ops = NULL;
	checksum->driver = NULL;

	free(checksum);
}

static void st_checksum_ripemd_init(void) {
	st_checksum_register_driver(&st_checksum_ripemd_driver);
}

static struct st_checksum * st_checksum_ripemd_new_checksum(void) {
	struct st_checksum * checksum = malloc(sizeof(struct st_checksum));
	checksum->ops = &st_checksum_ripemd_ops;
	checksum->driver = &st_checksum_ripemd_driver;

	struct st_checksum_ripemd_private * self = malloc(sizeof(struct st_checksum_ripemd_private));
	RIPEMD160_Init(&self->ripemd);
	*self->digest = '\0';

	checksum->data = self;
	return checksum;
}

static ssize_t st_checksum_ripemd_update(struct st_checksum * checksum, const void * data, ssize_t length) {
	if (checksum == NULL || data == NULL || length < 1)
		return -1;

	struct st_checksum_ripemd_private * self = checksum->data;
	if (RIPEMD160_Update(&self->ripemd, data, length)) {
		*self->digest = '\0';
		return length;
	}

	return -1;
}

