/***********************************************************************\
*       ______           ____     ___           __   _                  *
*      / __/ /____  ____/  _/__ _/ _ | ________/ /  (_)  _____ ____     *
*     _\ \/ __/ _ \/ __// // _ `/ __ |/ __/ __/ _ \/ / |/ / -_) __/     *
*    /___/\__/\___/_/ /___/\_, /_/ |_/_/  \__/_//_/_/|___/\__/_/        *
*                           /_/                                         *
*  -------------------------------------------------------------------  *
*  This file is a part of StorIqArchiver                                *
*                                                                       *
*  StorIqArchiver is free software; you can redistribute it and/or      *
*  modify it under the terms of the GNU General Public License          *
*  as published by the Free Software Foundation; either version 3       *
*  of the License, or (at your option) any later version.               *
*                                                                       *
*  This program is distributed in the hope that it will be useful,      *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of       *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
*  GNU General Public License for more details.                         *
*                                                                       *
*  You should have received a copy of the GNU General Public License    *
*  along with this program; if not, write to the Free Software          *
*  Foundation, Inc., 51 Franklin Street, Fifth Floor,                   *
*  Boston, MA  02110-1301, USA.                                         *
*                                                                       *
*  -------------------------------------------------------------------  *
*  Copyright (C) 2010, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Wed, 23 Feb 2011 20:58:39 +0100                       *
\***********************************************************************/

// free, malloc
#include <malloc.h>
// SHA512_Final, SHA512_Init, SHA512_Update
#include <openssl/sha.h>
// strdup
#include <string.h>

#include <storiqArchiver/checksum.h>

struct checksum_sha512_private {
	SHA512_CTX sha512;
	char * digest;
};

static struct checksum * checksum_sha512_clone(struct checksum * new_checksum, struct checksum * current_checksum);
static char * checksum_sha512_digest(struct checksum * checksum);
static void checksum_sha512_free(struct checksum * checksum);
static struct checksum * checksum_sha512_new_checksum(struct checksum * checksum);
static int checksum_sha512_update(struct checksum * checksum, const char * data, unsigned int length);

static struct checksum_driver checksum_sha512_driver = {
	.name			= "sha512",
	.new_checksum	= checksum_sha512_new_checksum,
	.cookie			= 0,
};

static struct checksum_ops checksum_sha512_ops = {
	.clone	= checksum_sha512_clone,
	.digest	= checksum_sha512_digest,
	.free	= checksum_sha512_free,
	.update	= checksum_sha512_update,
};


struct checksum * checksum_sha512_clone(struct checksum * new_checksum, struct checksum * current_checksum) {
	if (!current_checksum)
		return 0;

	struct checksum_sha512_private * current_self = current_checksum->data;

	if (!new_checksum)
		new_checksum = malloc(sizeof(struct checksum));

	new_checksum->ops = &checksum_sha512_ops;
	new_checksum->driver = &checksum_sha512_driver;

	struct checksum_sha512_private * new_self = malloc(sizeof(struct checksum_sha512_private));
	*new_self = *current_self;

	new_checksum->data = new_self;
	return new_checksum;
}

char * checksum_sha512_digest(struct checksum * checksum) {
	if (!checksum)
		return 0;

	struct checksum_sha512_private * self = checksum->data;

	if (self->digest)
		return strdup(self->digest);

	unsigned char digest[SHA512_DIGEST_LENGTH];

	if (!SHA512_Final(digest, &self->sha512))
		return 0;

	self->digest = malloc(SHA512_DIGEST_LENGTH * 2 + 1);
	checksum_convert2Hex(digest, SHA512_DIGEST_LENGTH, self->digest);

	return strdup(self->digest);
}

void checksum_sha512_free(struct checksum * checksum) {
	if (!checksum)
		return;

	struct checksum_sha512_private * self = checksum->data;

	if (self) {
		if (self->digest)
			free(self->digest);
		self->digest = 0;

		free(self);
	}

	checksum->data = 0;
	checksum->ops = 0;
	checksum->driver = 0;
}

__attribute__((constructor))
static void checksum_sha512_init() {
	checksum_registerDriver(&checksum_sha512_driver);
}

struct checksum * checksum_sha512_new_checksum(struct checksum * checksum) {
	if (!checksum)
		checksum = malloc(sizeof(struct checksum));

	checksum->ops = &checksum_sha512_ops;
	checksum->driver = &checksum_sha512_driver;

	struct checksum_sha512_private * self = malloc(sizeof(struct checksum_sha512_private));
	SHA512_Init(&self->sha512);
	self->digest = 0;

	checksum->data = self;
	return checksum;
}

int checksum_sha512_update(struct checksum * checksum, const char * data, unsigned int length) {
	if (!checksum)
		return -1;

	struct checksum_sha512_private * self = checksum->data;

	if (self->digest)
		free(self->digest);
	self->digest = 0;

	if (SHA512_Update(&self->sha512, data, length))
		return length;

	return -1;
}

