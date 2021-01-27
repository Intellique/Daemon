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
*  Copyright (C) 2013-2021, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

// free, malloc
#include <stdlib.h>
// strdup
#include <string.h>

#include <libstoriqone/checksum.h>

#include "xxhash.h"

#include <libchecksum-xxhash.chcksum>

struct XXH64_state_s {
    unsigned long long total_len;
    unsigned long long seed;
    unsigned long long v1;
    unsigned long long v2;
    unsigned long long v3;
    unsigned long long v4;
    unsigned long long mem64[4];   /* defined as U64 for alignment */
    unsigned int memsize;
};   /* typedef'd to XXH64_state_t within xxhash.h */

struct so_checksum_xxhash_private {
	struct XXH64_state_s xxhash;
	char digest[17];
};

static char * so_checksum_xxhash_digest(struct so_checksum * checksum);
static void so_checksum_xxhash_free(struct so_checksum * checksum);
static struct so_checksum * so_checksum_xxhash_new_checksum(void);
static void so_checksum_xxhash_init(void) __attribute__((constructor));
static void so_checksum_xxhash_reset(struct so_checksum * checksum);
static ssize_t so_checksum_xxhash_update(struct so_checksum * checksum, const void * data, ssize_t length);

static struct so_checksum_driver so_checksum_xxhash_driver = {
	.name             = "xxhash",
	.default_checksum = false,
	.new_checksum     = so_checksum_xxhash_new_checksum,
	.cookie           = NULL,
	.src_checksum     = STORIQONE_CHECKSUM_XXHASH_SRCSUM,
};

static struct so_checksum_ops so_checksum_xxhash_ops = {
	.digest = so_checksum_xxhash_digest,
	.free   = so_checksum_xxhash_free,
	.reset  = so_checksum_xxhash_reset,
	.update = so_checksum_xxhash_update,
};


static char * so_checksum_xxhash_digest(struct so_checksum * checksum) {
	if (checksum == NULL)
		return NULL;

	struct so_checksum_xxhash_private * self = checksum->data;
	if (self->digest[0] != '\0')
		return strdup(self->digest);

	/**
	 * Make sure that result store a 64 bits bigger endian integer.
	 * In order to be compatible with default output of xxhsum.
	 * See issue #194
	 */
	struct XXH64_state_s xxhash = self->xxhash;
	XXH64_hash_t result = htobe64(XXH64_digest(&xxhash));

	so_checksum_convert_to_hex((unsigned char *) &result, 8, self->digest);

	return strdup(self->digest);
}

static void so_checksum_xxhash_free(struct so_checksum * checksum) {
	if (checksum == NULL)
		return;

	struct so_checksum_xxhash_private * self = checksum->data;
	XXH64_digest(&self->xxhash);

	free(self);

	checksum->data = NULL;
	checksum->ops = NULL;
	checksum->driver = NULL;

	free(checksum);
}

static void so_checksum_xxhash_init(void) {
	so_checksum_register_driver(&so_checksum_xxhash_driver);
}

static struct so_checksum * so_checksum_xxhash_new_checksum(void) {
	struct so_checksum * checksum = malloc(sizeof(struct so_checksum));
	checksum->ops = &so_checksum_xxhash_ops;
	checksum->driver = &so_checksum_xxhash_driver;

	struct so_checksum_xxhash_private * self = malloc(sizeof(struct so_checksum_xxhash_private));
	bzero(self, sizeof(struct so_checksum_xxhash_private));
	XXH64_reset(&self->xxhash, 0);

	checksum->data = self;
	return checksum;
}

static void so_checksum_xxhash_reset(struct so_checksum * checksum) {
	if (checksum == NULL)
		return;

	struct so_checksum_xxhash_private * self = checksum->data;
	XXH64_reset(&self->xxhash, 0);
	*self->digest = '\0';
}

static ssize_t so_checksum_xxhash_update(struct so_checksum * checksum, const void * data, ssize_t length) {
	if (checksum == NULL || data == NULL || length < 1)
		return -1;

	struct so_checksum_xxhash_private * self = checksum->data;
	XXH64_update(&self->xxhash, data, length);
	return length;
}

