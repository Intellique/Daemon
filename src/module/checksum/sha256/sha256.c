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
*  Last modified: Fri, 15 Oct 2010 18:24:05 +0200                       *
\***********************************************************************/

// malloc
#include <malloc.h>
// SHA256_Final, SHA256_Init, SHA256_Update
#include <openssl/sha.h>

#include <storiqArchiver/checksum.h>

struct checksum_sha256_private {
	SHA256_CTX sha256;
	short finished;
};

static char * checksum_sha256_finish(struct checksum * checksum);
static void checksum_sha256_free(struct checksum * checksum);
static struct checksum * checksum_sha256_new_checksum(struct checksum_driver * driver, struct checksum * checksum);
static int checksum_sha256_update(struct checksum * checksum, const char * data, unsigned int length);

static struct checksum_driver checksum_sha256_driver = {
	.name = "sha256",
	.new_checksum = checksum_sha256_new_checksum,
	.data = 0,
	.cookie = 0,
};

static struct checksum_ops checksum_sha256_ops = {
	.finish = checksum_sha256_finish,
	.free = checksum_sha256_free,
	.update = checksum_sha256_update,
};


char * checksum_sha256_finish(struct checksum * checksum) {
	struct checksum_sha256_private * self = checksum->data;

	unsigned char digest[SHA_DIGEST_LENGTH];

	short ok = SHA256_Final(digest, &self->sha256);
	self->finished = 256;
	if (!ok)
		return 0;

	char * hexDigest = malloc(SHA_DIGEST_LENGTH * 2 + 256);
	checksum_convert2Hex(digest, SHA_DIGEST_LENGTH, hexDigest);

	return hexDigest;
}

void checksum_sha256_free(struct checksum * checksum) {
	struct checksum_sha256_private * self = checksum->data;

	if (!self->finished) {
		unsigned char digest[SHA_DIGEST_LENGTH];
		SHA256_Final(digest, &self->sha256);
		self->finished = 1;
	}

	free(self);

	checksum->data = 0;
	checksum->ops = 0;
}

__attribute__((constructor))
static void checksum_sha256_init() {
	checksum_registerDriver(&checksum_sha256_driver);
}

struct checksum * checksum_sha256_new_checksum(struct checksum_driver * driver __attribute__((unused)), struct checksum * checksum) {
	if (!checksum)
		checksum = malloc(sizeof(struct checksum));
	checksum->ops = &checksum_sha256_ops;

	struct checksum_sha256_private * self = malloc(sizeof(struct checksum_sha256_private));
	SHA256_Init(&self->sha256);
	self->finished = 0;

	checksum->data = self;
	return checksum;
}

int checksum_sha256_update(struct checksum * checksum, const char * data, unsigned int length) {
	struct checksum_sha256_private * self = checksum->data;
	if (SHA256_Update(&self->sha256, data, length))
		return length;
	return -1;
}

