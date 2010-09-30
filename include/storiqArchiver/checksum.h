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
*  Last modified: Thu, 30 Sep 2010 11:11:38 +0200                       *
\***********************************************************************/

#ifndef __STORIQARCHIVER_CHECKSUM_H__
#define __STORIQARCHIVER_CHECKSUM_H__

struct checksum;

struct checksum_ops {
	char * (*finish)(struct checksum * checksum);
	void (*free)(struct checksum * checksum);
	int (*update)(struct checksum * checksum, const char * data, unsigned int length);
};

struct checksum {
	struct checksum_ops * ops;
	void * data;
};

struct checksum_driver {
	char name[16];
	struct checksum * (*new_checksum)(struct checksum_driver * driver, struct checksum * checksum);
	void * data;
	void * cookie;
};


void checksum_convert2Hex(unsigned char * digest, int length, char * hexDigest);
struct checksum_driver * checksum_getDriver(const char * driver);
int checksum_loadDriver(const char * checksum);
void checksum_registerDriver(struct checksum_driver * driver);

#endif

