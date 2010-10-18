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
*  Last modified: Mon, 18 Oct 2010 17:26:46 +0200                       *
\***********************************************************************/

// malloc
#include <malloc.h>
// SHA512_Final, SHA512_Init, SHA512_Update
#include <openssl/sha.h>

#include <storiqArchiver/checksum.h>

struct checksum_sha512_private {
	SHA512_CTX sha512;
	short finished;
};

static char * checksum_sha512_finish(struct checksum * checksum);
static void checksum_sha512_free(struct checksum * checksum);
static struct checksum * checksum_sha512_new_checksum(struct checksum_driver * driver, struct checksum * checksum);
static int checksum_sha512_update(struct checksum * checksum, const char * data, unsigned int length);

static struct checksum_driver checksum_sha512_driver = {
	.name = "sha512",
	.new_checksum = checksum_sha512_new_checksum,
	.cookie = 0,
};

static struct checksum_ops checksum_sha512_ops = {
	.finish = checksum_sha512_finish,
	.free = checksum_sha512_free,
	.update = checksum_sha512_update,
};


char * checksum_sha512_finish(struct checksum * checksum) {
	struct checksum_sha512_private * self = checksum->data;

	if (self->finished)
		return 0;

	unsigned char digest[SHA512_DIGEST_LENGTH];

	short ok = SHA512_Final(digest, &self->sha512);
	self->finished = 1;
	if (!ok)
		return 0;

	char * hexDigest = malloc(SHA512_DIGEST_LENGTH * 2 + 1);
	checksum_convert2Hex(digest, SHA512_DIGEST_LENGTH, hexDigest);

	return hexDigest;
}

void checksum_sha512_free(struct checksum * checksum) {
	struct checksum_sha512_private * self = checksum->data;

	if (!self->finished) {
		unsigned char digest[SHA512_DIGEST_LENGTH];
		SHA512_Final(digest, &self->sha512);
		self->finished = 1;
	}

	free(self);

	checksum->data = 0;
	checksum->ops = 0;
	checksum->driver = 0;
}

__attribute__((constructor))
static void checksum_sha512_init() {
	checksum_registerDriver(&checksum_sha512_driver);
}

struct checksum * checksum_sha512_new_checksum(struct checksum_driver * driver, struct checksum * checksum) {
	if (!checksum)
		checksum = malloc(sizeof(struct checksum));
	checksum->ops = &checksum_sha512_ops;
	checksum->driver = driver;

	struct checksum_sha512_private * self = malloc(sizeof(struct checksum_sha512_private));
	SHA512_Init(&self->sha512);
	self->finished = 0;

	checksum->data = self;
	return checksum;
}

int checksum_sha512_update(struct checksum * checksum, const char * data, unsigned int length) {
	struct checksum_sha512_private * self = checksum->data;

	if (self->finished)
		return -1;

	if (SHA512_Update(&self->sha512, data, length))
		return length;

	return -1;
}

