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
*  Last modified: Fri, 01 Jul 2011 19:14:25 +0200                       *
\***********************************************************************/

// free, malloc
#include <malloc.h>
// SHA1_Final, SHA1_Init, SHA1_Update
#include <openssl/sha.h>
// strdup
#include <string.h>

#include <storiqArchiver/checksum.h>

struct _sa_checksum_sha1_private {
	SHA_CTX sha1;
	char * digest;
};

static struct sa_checksum * _sa_checksum_sha1_clone(struct sa_checksum * new_checksum, struct sa_checksum * current_checksum);
static char * _sa_checksum_sha1_digest(struct sa_checksum * checksum);
static void _sa_checksum_sha1_free(struct sa_checksum * checksum);
static struct sa_checksum * _sa_checksum_sha1_new_checksum(struct sa_checksum * checksum);
static void _sa_checksum_sha1_reset(struct sa_checksum * checksum);
static int _sa_checksum_sha1_update(struct sa_checksum * checksum, const char * data, unsigned int length);

static struct sa_checksum_driver _sa_checksum_sha1_driver = {
	.name			= "sha1",
	.new_checksum	= _sa_checksum_sha1_new_checksum,
	.cookie			= 0,
};

static struct sa_checksum_ops _sa_checksum_sha1_ops = {
	.clone	= _sa_checksum_sha1_clone,
	.digest	= _sa_checksum_sha1_digest,
	.free	= _sa_checksum_sha1_free,
	.reset	= _sa_checksum_sha1_reset,
	.update	= _sa_checksum_sha1_update,
};


struct sa_checksum * _sa_checksum_sha1_clone(struct sa_checksum * new_checksum, struct sa_checksum * current_checksum) {
	if (!current_checksum)
		return 0;

	struct _sa_checksum_sha1_private * current_self = current_checksum->data;

	if (!new_checksum)
		new_checksum = malloc(sizeof(struct sa_checksum));

	new_checksum->ops = &_sa_checksum_sha1_ops;
	new_checksum->driver = &_sa_checksum_sha1_driver;

	struct _sa_checksum_sha1_private * new_self = malloc(sizeof(struct _sa_checksum_sha1_private));
	*new_self = *current_self;

	new_checksum->data = new_self;
	return new_checksum;
}

char * _sa_checksum_sha1_digest(struct sa_checksum * checksum) {
	if (!checksum)
		return 0;

	struct _sa_checksum_sha1_private * self = checksum->data;

	if (self->digest)
		return strdup(self->digest);

	unsigned char digest[SHA_DIGEST_LENGTH];

	if (!SHA1_Final(digest, &self->sha1))
		return 0;

	self->digest = malloc(SHA_DIGEST_LENGTH * 2 + 1);
	sa_checksum_convert_to_hex(digest, SHA_DIGEST_LENGTH, self->digest);

	return strdup(self->digest);
}

void _sa_checksum_sha1_free(struct sa_checksum * checksum) {
	if (!checksum)
		return;

	struct _sa_checksum_sha1_private * self = checksum->data;

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
static void _sa_checksum_sha1_init() {
	sa_checksum_register_driver(&_sa_checksum_sha1_driver);
}

struct sa_checksum * _sa_checksum_sha1_new_checksum(struct sa_checksum * checksum) {
	if (!checksum)
		checksum = malloc(sizeof(struct sa_checksum));

	checksum->ops = &_sa_checksum_sha1_ops;
	checksum->driver = &_sa_checksum_sha1_driver;

	struct _sa_checksum_sha1_private * self = malloc(sizeof(struct _sa_checksum_sha1_private));
	SHA1_Init(&self->sha1);
	self->digest = 0;

	checksum->data = self;
	return checksum;
}

void _sa_checksum_sha1_reset(struct sa_checksum * checksum) {
	if (!checksum)
		return;

	struct _sa_checksum_sha1_private * self = checksum->data;
	if (self) {
		SHA1_Init(&self->sha1);

		if (self->digest)
			free(self->digest);
		self->digest = 0;
	}
}

int _sa_checksum_sha1_update(struct sa_checksum * checksum, const char * data, unsigned int length) {
	if (!checksum)
		return -1;

	struct _sa_checksum_sha1_private * self = checksum->data;

	if (self->digest)
		free(self->digest);
	self->digest = 0;

	if (SHA1_Update(&self->sha1, data, length))
		return length;

	return -1;
}

