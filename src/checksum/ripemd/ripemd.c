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
// RIPEMD_Final, RIPEMD_Init, RIPEMD_Update
#include <openssl/ripemd.h>
// strdup
#include <string.h>

#include <libstoriqone/checksum.h>

#include <libchecksum-ripemd.chcksum>

struct so_checksum_ripemd_private {
	RIPEMD160_CTX ripemd;
	char digest[RIPEMD160_DIGEST_LENGTH * 2 + 1];
};

static char * so_checksum_ripemd_digest(struct so_checksum * checksum);
static void so_checksum_ripemd_free(struct so_checksum * checksum);
static struct so_checksum * so_checksum_ripemd_new_checksum(void);
static void so_checksum_ripemd_init(void) __attribute__((constructor));
static ssize_t so_checksum_ripemd_update(struct so_checksum * checksum, const void * data, ssize_t length);

static struct so_checksum_driver so_checksum_ripemd_driver = {
	.name			  = "ripemd",
	.default_checksum = false,
	.new_checksum	  = so_checksum_ripemd_new_checksum,
	.cookie			  = NULL,
	.src_checksum     = STORIQONE_CHECKSUM_RIPEMD_SRCSUM,
};

static struct so_checksum_ops so_checksum_ripemd_ops = {
	.digest	= so_checksum_ripemd_digest,
	.free	= so_checksum_ripemd_free,
	.update	= so_checksum_ripemd_update,
};


static char * so_checksum_ripemd_digest(struct so_checksum * checksum) {
	if (checksum == NULL)
		return NULL;

	struct so_checksum_ripemd_private * self = checksum->data;
	if (self->digest[0] != '\0')
		return strdup(self->digest);

	RIPEMD160_CTX ripemd = self->ripemd;
	unsigned char digest[RIPEMD160_DIGEST_LENGTH];
	if (!RIPEMD160_Final(digest, &ripemd))
		return NULL;

	so_checksum_convert_to_hex(digest, RIPEMD160_DIGEST_LENGTH, self->digest);

	return strdup(self->digest);
}

static void so_checksum_ripemd_free(struct so_checksum * checksum) {
	if (checksum == NULL)
		return;

	struct so_checksum_ripemd_private * self = checksum->data;

	unsigned char digest[RIPEMD160_DIGEST_LENGTH];
	RIPEMD160_Final(digest, &self->ripemd);

	free(self);

	checksum->data = NULL;
	checksum->ops = NULL;
	checksum->driver = NULL;

	free(checksum);
}

static void so_checksum_ripemd_init(void) {
	so_checksum_register_driver(&so_checksum_ripemd_driver);
}

static struct so_checksum * so_checksum_ripemd_new_checksum(void) {
	struct so_checksum * checksum = malloc(sizeof(struct so_checksum));
	checksum->ops = &so_checksum_ripemd_ops;
	checksum->driver = &so_checksum_ripemd_driver;

	struct so_checksum_ripemd_private * self = malloc(sizeof(struct so_checksum_ripemd_private));
	RIPEMD160_Init(&self->ripemd);
	*self->digest = '\0';

	checksum->data = self;
	return checksum;
}

static ssize_t so_checksum_ripemd_update(struct so_checksum * checksum, const void * data, ssize_t length) {
	if (checksum == NULL || data == NULL || length < 1)
		return -1;

	struct so_checksum_ripemd_private * self = checksum->data;
	if (RIPEMD160_Update(&self->ripemd, data, length)) {
		*self->digest = '\0';
		return length;
	}

	return -1;
}

