/*************************************************************************\
*        ______           ____     ___           __   _                   *
*       / __/ /____  ____/  _/__ _/ _ | ________/ /  (_)  _____ ____      *
*      _\ \/ __/ _ \/ __// // _ `/ __ |/ __/ __/ _ \/ / |/ / -_) __/      *
*     /___/\__/\___/_/ /___/\_, /_/ |_/_/  \__/_//_/_/|___/\__/_/         *
*                            /_/                                          *
*  ---------------------------------------------------------------------  *
*  This file is a part of StorIqArchiver                                  *
*                                                                         *
*  StorIqArchiver is free software; you can redistribute it and/or        *
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
*  Last modified: Sat, 17 Dec 2011 17:22:30 +0100                         *
\*************************************************************************/

// free, malloc
#include <malloc.h>
// SHA256_Final, SHA256_Init, SHA256_Update
#include <openssl/sha.h>
// strdup
#include <string.h>

#include <stone/checksum.h>

struct sa_checksum_sha256_private {
	SHA256_CTX sha256;
	char digest[SHA256_DIGEST_LENGTH * 2 + 1];
};

static struct sa_checksum * sa_checksum_sha256_clone(struct sa_checksum * new_checksum, struct sa_checksum * current_checksum);
static char * sa_checksum_sha256_digest(struct sa_checksum * checksum);
static void sa_checksum_sha256_free(struct sa_checksum * checksum);
static struct sa_checksum * sa_checksum_sha256_new_checksum(struct sa_checksum * checksum);
static void sa_checksum_sha256_init(void) __attribute__((constructor));
static ssize_t sa_checksum_sha256_update(struct sa_checksum * checksum, const void * data, ssize_t length);

static struct sa_checksum_driver sa_checksum_sha256_driver = {
	.name			= "sha256",
	.new_checksum	= sa_checksum_sha256_new_checksum,
	.cookie			= 0,
	.api_version    = STORIQARCHIVER_CHECKSUM_APIVERSION,
};

static struct sa_checksum_ops sa_checksum_sha256_ops = {
	.clone	= sa_checksum_sha256_clone,
	.digest	= sa_checksum_sha256_digest,
	.free	= sa_checksum_sha256_free,
	.update	= sa_checksum_sha256_update,
};


struct sa_checksum * sa_checksum_sha256_clone(struct sa_checksum * new_checksum, struct sa_checksum * current_checksum) {
	if (!current_checksum)
		return 0;

	struct sa_checksum_sha256_private * current_self = current_checksum->data;

	if (!new_checksum)
		new_checksum = malloc(sizeof(struct sa_checksum));

	new_checksum->ops = &sa_checksum_sha256_ops;
	new_checksum->driver = &sa_checksum_sha256_driver;

	struct sa_checksum_sha256_private * new_self = malloc(sizeof(struct sa_checksum_sha256_private));
	*new_self = *current_self;

	new_checksum->data = new_self;
	return new_checksum;
}

char * sa_checksum_sha256_digest(struct sa_checksum * checksum) {
	if (!checksum)
		return 0;

	struct sa_checksum_sha256_private * self = checksum->data;

	if (self->digest[0] != '\0')
		return strdup(self->digest);

	unsigned char digest[SHA256_DIGEST_LENGTH];
	if (!SHA256_Final(digest, &self->sha256))
		return 0;

	sa_checksum_convert_to_hex(digest, SHA256_DIGEST_LENGTH, self->digest);

	return strdup(self->digest);
}

void sa_checksum_sha256_free(struct sa_checksum * checksum) {
	if (!checksum)
		return;

	struct sa_checksum_sha256_private * self = checksum->data;

	if (self)
		free(self);

	checksum->data = 0;
	checksum->ops = 0;
	checksum->driver = 0;
}

void sa_checksum_sha256_init() {
	sa_checksum_register_driver(&sa_checksum_sha256_driver);
}

struct sa_checksum * sa_checksum_sha256_new_checksum(struct sa_checksum * checksum) {
	if (!checksum)
		checksum = malloc(sizeof(struct sa_checksum));

	checksum->ops = &sa_checksum_sha256_ops;
	checksum->driver = &sa_checksum_sha256_driver;

	struct sa_checksum_sha256_private * self = malloc(sizeof(struct sa_checksum_sha256_private));
	SHA256_Init(&self->sha256);
	*self->digest = '\0';

	checksum->data = self;
	return checksum;
}

ssize_t sa_checksum_sha256_update(struct sa_checksum * checksum, const void * data, ssize_t length) {
	if (!checksum || !data || length < 1)
		return -1;

	struct sa_checksum_sha256_private * self = checksum->data;
	if (*self->digest != '\0')
		return -2;

	if (SHA256_Update(&self->sha256, data, length))
		return length;

	return -1;
}

