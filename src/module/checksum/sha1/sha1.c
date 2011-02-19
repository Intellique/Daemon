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
*  Copyright (C) 2011, Clercin guillaume <gclercin@intellique.com>      *
*  Last modified: Sat, 19 Feb 2011 13:30:53 +0100                       *
\***********************************************************************/

// malloc
#include <malloc.h>
// SHA1_Final, SHA1_Init, SHA1_Update
#include <openssl/sha.h>

#include <storiqArchiver/checksum.h>

struct checksum_sha1_private {
	SHA_CTX sha1;
	short finished;
};

static struct checksum * checksum_sha1_clone(struct checksum * new_checksum, struct checksum * current_checksum);
static char * checksum_sha1_finish(struct checksum * checksum);
static void checksum_sha1_free(struct checksum * checksum);
static struct checksum * checksum_sha1_new_checksum(struct checksum * checksum);
static int checksum_sha1_update(struct checksum * checksum, const char * data, unsigned int length);

static struct checksum_driver checksum_sha1_driver = {
	.name			= "sha1",
	.new_checksum	= checksum_sha1_new_checksum,
	.cookie			= 0,
};

static struct checksum_ops checksum_sha1_ops = {
	.clone	= checksum_sha1_clone,
	.finish	= checksum_sha1_finish,
	.free	= checksum_sha1_free,
	.update	= checksum_sha1_update,
};


struct checksum * checksum_sha1_clone(struct checksum * new_checksum, struct checksum * current_checksum) {
	if (!current_checksum)
		return 0;

	struct checksum_sha1_private * current_self = current_checksum->data;

	if (!new_checksum)
		new_checksum = malloc(sizeof(struct checksum));

	new_checksum->ops = &checksum_sha1_ops;
	new_checksum->driver = &checksum_sha1_driver;

	struct checksum_sha1_private * new_self = malloc(sizeof(struct checksum_sha1_private));
	*new_self = *current_self;

	new_checksum->data = new_self;
	return new_checksum;
}

char * checksum_sha1_finish(struct checksum * checksum) {
	if (!checksum)
		return 0;

	struct checksum_sha1_private * self = checksum->data;

	if (self->finished)
		return 0;

	unsigned char digest[SHA_DIGEST_LENGTH];

	short ok = SHA1_Final(digest, &self->sha1);
	self->finished = 1;
	if (!ok)
		return 0;

	char * hexDigest = malloc(SHA_DIGEST_LENGTH * 2 + 1);
	checksum_convert2Hex(digest, SHA_DIGEST_LENGTH, hexDigest);

	return hexDigest;
}

void checksum_sha1_free(struct checksum * checksum) {
	if (!checksum)
		return;

	struct checksum_sha1_private * self = checksum->data;

	if (!self->finished) {
		unsigned char digest[SHA_DIGEST_LENGTH];
		SHA1_Final(digest, &self->sha1);
		self->finished = 1;
	}

	free(self);

	checksum->data = 0;
	checksum->ops = 0;
	checksum->driver = 0;
}

__attribute__((constructor))
static void checksum_sha1_init() {
	checksum_registerDriver(&checksum_sha1_driver);
}

struct checksum * checksum_sha1_new_checksum(struct checksum * checksum) {
	if (!checksum)
		checksum = malloc(sizeof(struct checksum));

	checksum->ops = &checksum_sha1_ops;
	checksum->driver = &checksum_sha1_driver;

	struct checksum_sha1_private * self = malloc(sizeof(struct checksum_sha1_private));
	SHA1_Init(&self->sha1);
	self->finished = 0;

	checksum->data = self;
	return checksum;
}

int checksum_sha1_update(struct checksum * checksum, const char * data, unsigned int length) {
	if (!checksum)
		return -1;

	struct checksum_sha1_private * self = checksum->data;

	if (self->finished)
		return -1;

	if (SHA1_Update(&self->sha1, data, length))
		return length;

	return -1;
}

