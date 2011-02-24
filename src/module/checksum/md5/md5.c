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
*  Last modified: Thu, 24 Feb 2011 10:32:23 +0100                       *
\***********************************************************************/

// free, malloc
#include <malloc.h>
// MD5_Final, MD5_Init, MD5_Update
#include <openssl/md5.h>
// strdup
#include <string.h>

#include <storiqArchiver/checksum.h>

struct checksum_md5_private {
	MD5_CTX md5;
	char * digest;
};

static struct checksum * checksum_md5_clone(struct checksum * new_checksum, struct checksum * current_checksum);
static char * checksum_md5_digest(struct checksum * checksum);
static void checksum_md5_free(struct checksum * checksum);
static struct checksum * checksum_md5_new_checksum(struct checksum * checksum);
static void checksum_md5_reset(struct checksum * checksum);
static int checksum_md5_update(struct checksum * checksum, const char * data, unsigned int length);

static struct checksum_driver checksum_md5_driver = {
	.name			= "md5",
	.new_checksum	= checksum_md5_new_checksum,
	.cookie			= 0,
};

static struct checksum_ops checksum_md5_ops = {
	.clone	= checksum_md5_clone,
	.digest	= checksum_md5_digest,
	.free	= checksum_md5_free,
	.reset	= checksum_md5_reset,
	.update	= checksum_md5_update,
};


struct checksum * checksum_md5_clone(struct checksum * new_checksum, struct checksum * current_checksum) {
	if (!current_checksum)
		return 0;

	struct checksum_md5_private * current_self = current_checksum->data;

	if (!new_checksum)
		new_checksum = malloc(sizeof(struct checksum));

	new_checksum->ops = &checksum_md5_ops;
	new_checksum->driver = &checksum_md5_driver;

	struct checksum_md5_private * new_self = malloc(sizeof(struct checksum_md5_private));
	*new_self = *current_self;

	new_checksum->data = new_self;
	return new_checksum;
}

char * checksum_md5_digest(struct checksum * checksum) {
	if (!checksum)
		return 0;

	struct checksum_md5_private * self = checksum->data;

	if (self->digest)
		return strdup(self->digest);

	MD5_CTX md5 = self->md5;
	unsigned char digest[MD5_DIGEST_LENGTH];
	if (!MD5_Final(digest, &md5))
		return 0;

	self->digest = malloc(MD5_DIGEST_LENGTH * 2 + 1);
	checksum_convert2Hex(digest, MD5_DIGEST_LENGTH, self->digest);

	return strdup(self->digest);
}

void checksum_md5_free(struct checksum * checksum) {
	if (!checksum)
		return;

	struct checksum_md5_private * self = checksum->data;

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
static void checksum_md5_init() {
	checksum_registerDriver(&checksum_md5_driver);
}

struct checksum * checksum_md5_new_checksum(struct checksum * checksum) {
	if (!checksum)
		checksum = malloc(sizeof(struct checksum));

	checksum->ops = &checksum_md5_ops;
	checksum->driver = &checksum_md5_driver;

	struct checksum_md5_private * self = malloc(sizeof(struct checksum_md5_private));
	MD5_Init(&self->md5);
	self->digest = 0;

	checksum->data = self;
	return checksum;
}

void checksum_md5_reset(struct checksum * checksum) {
	if (!checksum)
		return;

	struct checksum_md5_private * self = checksum->data;
	if (self) {
		MD5_Init(&self->md5);

		if (self->digest)
			free(self->digest);
		self->digest = 0;
	}
}

int checksum_md5_update(struct checksum * checksum, const char * data, unsigned int length) {
	if (!checksum)
		return -1;

	struct checksum_md5_private * self = checksum->data;

	if (self->digest)
		free(self->digest);
	self->digest = 0;

	if (MD5_Update(&self->md5, data, length))
		return length;

	return -1;
}

