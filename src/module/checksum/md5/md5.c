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
*  Last modified: Sat, 08 Dec 2012 22:57:28 +0100                         *
\*************************************************************************/

// free, malloc
#include <malloc.h>
// MD5_Final, MD5_Init, MD5_Update
#include <openssl/md5.h>
// strdup
#include <string.h>

#include <libstone/checksum.h>

struct st_checksum_md5_private {
	MD5_CTX md5;
	char digest[MD5_DIGEST_LENGTH * 2 + 1];
};

static char * st_checksum_md5_digest(struct st_checksum * checksum);
static void st_checksum_md5_free(struct st_checksum * checksum);
static struct st_checksum * st_checksum_md5_new_checksum(void);
static void st_checksum_md5_init(void) __attribute__((constructor));
static ssize_t st_checksum_md5_update(struct st_checksum * checksum, const void * data, ssize_t length);

static struct st_checksum_driver st_checksum_md5_driver = {
	.name			= "md5",
	.new_checksum	= st_checksum_md5_new_checksum,
	.cookie			= NULL,
	.api_level      = {
		.checksum = STONE_CHECKSUM_API_LEVEL,
		.database = 0,
		.job      = 0,
	},
};

static struct st_checksum_ops st_checksum_md5_ops = {
	.digest	= st_checksum_md5_digest,
	.free	= st_checksum_md5_free,
	.update	= st_checksum_md5_update,
};


static char * st_checksum_md5_digest(struct st_checksum * checksum) {
	if (checksum == NULL)
		return NULL;

	struct st_checksum_md5_private * self = checksum->data;
	if (self->digest[0] != '\0')
		return strdup(self->digest);

	MD5_CTX md5 = self->md5;
	unsigned char digest[MD5_DIGEST_LENGTH];
	if (!MD5_Final(digest, &md5))
		return NULL;

	st_checksum_convert_to_hex(digest, MD5_DIGEST_LENGTH, self->digest);

	return strdup(self->digest);
}

static void st_checksum_md5_free(struct st_checksum * checksum) {
	if (checksum == NULL)
		return;

	struct st_checksum_md5_private * self = checksum->data;

	unsigned char digest[MD5_DIGEST_LENGTH];
	MD5_Final(digest, &self->md5);

	free(self);

	checksum->data = NULL;
	checksum->ops = NULL;
	checksum->driver = NULL;

	free(checksum);
}

static void st_checksum_md5_init(void) {
	st_checksum_register_driver(&st_checksum_md5_driver);
}

static struct st_checksum * st_checksum_md5_new_checksum(void) {
	struct st_checksum * checksum = malloc(sizeof(struct st_checksum));
	checksum->ops = &st_checksum_md5_ops;
	checksum->driver = &st_checksum_md5_driver;

	struct st_checksum_md5_private * self = malloc(sizeof(struct st_checksum_md5_private));
	MD5_Init(&self->md5);
	*self->digest = '\0';

	checksum->data = self;
	return checksum;
}

static ssize_t st_checksum_md5_update(struct st_checksum * checksum, const void * data, ssize_t length) {
	if (checksum == NULL || data == NULL || length < 1)
		return -1;

	struct st_checksum_md5_private * self = checksum->data;
	if (MD5_Update(&self->md5, data, length)) {
		*self->digest = '\0';
		return length;
	}

	return -1;
}

