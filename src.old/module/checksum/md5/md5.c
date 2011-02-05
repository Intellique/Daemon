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
*  Last modified: Thu, 21 Oct 2010 15:45:48 +0200                       *
\***********************************************************************/

// malloc
#include <malloc.h>
// MD5_Final, MD5_Init, MD5_Update
#include <openssl/md5.h>

#include <storiqArchiver/checksum.h>

struct checksum_md5_private {
	MD5_CTX md5;
	short finished;
};

static char * checksum_md5_finish(struct checksum * checksum);
static void checksum_md5_free(struct checksum * checksum);
static struct checksum * checksum_md5_new_checksum(struct checksum * checksum);
static int checksum_md5_update(struct checksum * checksum, const char * data, unsigned int length);

static struct checksum_driver checksum_md5_driver = {
	.name = "md5",
	.new_checksum = checksum_md5_new_checksum,
	.cookie = 0,
};

static struct checksum_ops checksum_md5_ops = {
	.finish = checksum_md5_finish,
	.free = checksum_md5_free,
	.update = checksum_md5_update,
};


char * checksum_md5_finish(struct checksum * checksum) {
	if (!checksum)
		return 0;

	struct checksum_md5_private * self = checksum->data;

	if (self->finished)
		return 0;

	unsigned char digest[MD5_DIGEST_LENGTH];

	short ok = MD5_Final(digest, &self->md5);
	self->finished = 1;
	if (!ok)
		return 0;

	char * hexDigest = malloc(MD5_DIGEST_LENGTH * 2 + 1);
	checksum_convert2Hex(digest, MD5_DIGEST_LENGTH, hexDigest);

	return hexDigest;
}

void checksum_md5_free(struct checksum * checksum) {
	if (!checksum)
		return;

	struct checksum_md5_private * self = checksum->data;

	if (!self->finished) {
		unsigned char digest[MD5_DIGEST_LENGTH];
		MD5_Final(digest, &self->md5);
		self->finished = 1;
	}

	free(self);

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
	self->finished = 0;

	checksum->data = self;
	return checksum;
}

int checksum_md5_update(struct checksum * checksum, const char * data, unsigned int length) {
	if (!checksum)
		return -1;

	struct checksum_md5_private * self = checksum->data;

	if (self->finished)
		return -1;

	if (MD5_Update(&self->md5, data, length))
		return length;

	return -1;
}

