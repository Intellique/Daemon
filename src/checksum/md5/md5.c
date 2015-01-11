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
*  Copyright (C) 2015, Guillaume Clercin <gclercin@intellique.com>           *
\****************************************************************************/

// free, malloc
#include <stdlib.h>
// MD5_Final, MD5_Init, MD5_Update
#include <openssl/md5.h>
// strdup
#include <string.h>

#include <libstoriqone/checksum.h>

#include <libchecksum-md5.chcksum>

struct so_checksum_md5_private {
	MD5_CTX md5;
	char digest[MD5_DIGEST_LENGTH * 2 + 1];
};

static char * so_checksum_md5_digest(struct so_checksum * checksum);
static void so_checksum_md5_free(struct so_checksum * checksum);
static struct so_checksum * so_checksum_md5_new_checksum(void);
static void so_checksum_md5_init(void) __attribute__((constructor));
static ssize_t so_checksum_md5_update(struct so_checksum * checksum, const void * data, ssize_t length);

static struct so_checksum_driver so_checksum_md5_driver = {
	.name			  = "md5",
	.default_checksum = false,
	.new_checksum	  = so_checksum_md5_new_checksum,
	.cookie			  = NULL,
	.src_checksum     = STORIQONE_CHECKSUM_MD5_SRCSUM,
};

static struct so_checksum_ops so_checksum_md5_ops = {
	.digest	= so_checksum_md5_digest,
	.free	= so_checksum_md5_free,
	.update	= so_checksum_md5_update,
};


static char * so_checksum_md5_digest(struct so_checksum * checksum) {
	if (checksum == NULL)
		return NULL;

	struct so_checksum_md5_private * self = checksum->data;
	if (self->digest[0] != '\0')
		return strdup(self->digest);

	MD5_CTX md5 = self->md5;
	unsigned char digest[MD5_DIGEST_LENGTH];
	if (!MD5_Final(digest, &md5))
		return NULL;

	so_checksum_convert_to_hex(digest, MD5_DIGEST_LENGTH, self->digest);

	return strdup(self->digest);
}

static void so_checksum_md5_free(struct so_checksum * checksum) {
	if (checksum == NULL)
		return;

	struct so_checksum_md5_private * self = checksum->data;

	unsigned char digest[MD5_DIGEST_LENGTH];
	MD5_Final(digest, &self->md5);

	free(self);

	checksum->data = NULL;
	checksum->ops = NULL;
	checksum->driver = NULL;

	free(checksum);
}

static void so_checksum_md5_init(void) {
	so_checksum_register_driver(&so_checksum_md5_driver);
}

static struct so_checksum * so_checksum_md5_new_checksum(void) {
	struct so_checksum * checksum = malloc(sizeof(struct so_checksum));
	checksum->ops = &so_checksum_md5_ops;
	checksum->driver = &so_checksum_md5_driver;

	struct so_checksum_md5_private * self = malloc(sizeof(struct so_checksum_md5_private));
	MD5_Init(&self->md5);
	*self->digest = '\0';

	checksum->data = self;
	return checksum;
}

static ssize_t so_checksum_md5_update(struct so_checksum * checksum, const void * data, ssize_t length) {
	if (checksum == NULL || data == NULL || length < 1)
		return -1;

	struct so_checksum_md5_private * self = checksum->data;
	if (MD5_Update(&self->md5, data, length)) {
		*self->digest = '\0';
		return length;
	}

	return -1;
}

