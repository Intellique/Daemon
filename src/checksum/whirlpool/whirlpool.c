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
*  Copyright (C) 2013-2018, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// free, malloc
#include <malloc.h>
// WHIRLPOOL_Final, WHIRLPOOL_Init, WHIRLPOOL_Update
#include <openssl/whrlpool.h>
// strdup
#include <string.h>

#include <libstoriqone/checksum.h>

#include <libchecksum-whirlpool.chcksum>

struct so_checksum_whirlpool_private {
	WHIRLPOOL_CTX whirlpool;
	char digest[WHIRLPOOL_DIGEST_LENGTH * 2 + 1];
};

static char * so_checksum_whirlpool_digest(struct so_checksum * checksum);
static void so_checksum_whirlpool_free(struct so_checksum * checksum);
static struct so_checksum * so_checksum_whirlpool_new_checksum(void);
static void so_checksum_whirlpool_init(void) __attribute__((constructor));
static void so_checksum_whirlpool_reset(struct so_checksum * checksum);
static ssize_t so_checksum_whirlpool_update(struct so_checksum * checksum, const void * data, ssize_t length);

static struct so_checksum_driver so_checksum_whirlpool_driver = {
	.name             = "whirlpool",
	.default_checksum = false,
	.new_checksum     = so_checksum_whirlpool_new_checksum,
	.cookie           = NULL,
	.src_checksum     = STORIQONE_CHECKSUM_WHIRLPOOL_SRCSUM,
};

static struct so_checksum_ops so_checksum_whirlpool_ops = {
	.digest = so_checksum_whirlpool_digest,
	.free   = so_checksum_whirlpool_free,
	.reset  = so_checksum_whirlpool_reset,
	.update = so_checksum_whirlpool_update,
};


char * so_checksum_whirlpool_digest(struct so_checksum * checksum) {
	if (checksum == NULL)
		return NULL;

	struct so_checksum_whirlpool_private * self = checksum->data;
	if (self->digest[0] != '\0')
		return strdup(self->digest);

	WHIRLPOOL_CTX whirlpool = self->whirlpool;
	unsigned char digest[WHIRLPOOL_DIGEST_LENGTH];
	if (!WHIRLPOOL_Final(digest, &whirlpool))
		return NULL;

	so_checksum_convert_to_hex(digest, WHIRLPOOL_DIGEST_LENGTH, self->digest);

	return strdup(self->digest);
}

void so_checksum_whirlpool_free(struct so_checksum * checksum) {
	if (!checksum)
		return;

	struct so_checksum_whirlpool_private * self = checksum->data;

	unsigned char digest[WHIRLPOOL_DIGEST_LENGTH];
	WHIRLPOOL_Final(digest, &self->whirlpool);

	free(self);

	checksum->data = NULL;
	checksum->ops = NULL;
	checksum->driver = NULL;

	free(checksum);
}

void so_checksum_whirlpool_init() {
	so_checksum_register_driver(&so_checksum_whirlpool_driver);
}

struct so_checksum * so_checksum_whirlpool_new_checksum(void) {
	struct so_checksum * checksum = malloc(sizeof(struct so_checksum));
	checksum->ops = &so_checksum_whirlpool_ops;
	checksum->driver = &so_checksum_whirlpool_driver;

	struct so_checksum_whirlpool_private * self = malloc(sizeof(struct so_checksum_whirlpool_private));
	WHIRLPOOL_Init(&self->whirlpool);
	*self->digest = '\0';

	checksum->data = self;
	return checksum;
}

static void so_checksum_whirlpool_reset(struct so_checksum * checksum) {
	if (checksum == NULL)
		return;

	struct so_checksum_whirlpool_private * self = checksum->data;
	WHIRLPOOL_Init(&self->whirlpool);
	*self->digest = '\0';
}

ssize_t so_checksum_whirlpool_update(struct so_checksum * checksum, const void * data, ssize_t length) {
	if (checksum == NULL || data == NULL || length < 1)
		return -1;

	struct so_checksum_whirlpool_private * self = checksum->data;
	if (WHIRLPOOL_Update(&self->whirlpool, data, length)) {
		*self->digest = '\0';
		return length;
	}

	return -1;
}
