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
*  Last modified: Mon, 18 Oct 2010 17:21:26 +0200                       *
\***********************************************************************/

#ifndef __STORIQARCHIVER_CHECKSUM_H__
#define __STORIQARCHIVER_CHECKSUM_H__

struct checksum;

/**
 * \struct checksum_ops
 * \brief this structure contains only pointer functions
 * \note all functions should point on real function
 */
struct checksum_ops {
	/**
	 * \brief this function should be used to compute a digest
	 * \param checksum : a checksum handler
	 * \return a dynamically allocated string which contains a digest in hexadecimal form
	 */
	char * (*finish)(struct checksum * checksum);
	/**
	 * \brief this function should release all memory associated with ckecksum
	 * \param checksum : a checksum handler
	 * \warning this function SHOULD NOT call :
	 * \code
	 * free(checksum);
	 * \endcode
	 */
	void (*free)(struct checksum * checksum);
	/**
	 * \brief this function is used to read some data or a
	 * \param checksum : a checksum handler
	 * \param data : some or full data
	 * \param length : length of data
	 * \return < 0 if error
	 * \note this function can be called one or many times
	 */
	int (*update)(struct checksum * checksum, const char * data, unsigned int length);
};

/**
 * \struct checksum
 * \brief this structure is used as an handler to a specific checksum
 */
struct checksum {
	/**
	 * \brief contains a checksum function
	 */
	struct checksum_ops * ops;
	/**
	 * \brief private data of one checksum
	 */
	void * data;
	/**
	 * \brief associated driver
	 */
	struct checksum_driver * driver;
};

/**
 * \struct checksum_driver
 */
struct checksum_driver {
	/**
	 * \brief name of the driver
	 */
	char * name;
	/**
	 * \brief get a new checksum handler
	 * \param driver : a checksum driver
	 * \param checksum : an already allocated checksum or NULL
	 * \note if checksum is NULL, this function allocate enough memory with malloc
	 */
	struct checksum * (*new_checksum)(struct checksum_driver * driver, struct checksum * checksum);
	/**
	 * \brief private data used by checksum_loadDriver
	 * \note should not be modified
	 */
	void * cookie;
};


/**
 * \brief checksum_convert2Hex convert a digest into hexadecimal form
 * \param digest : a digest
 * \param length : length of digest in bytes
 * \param hexDigest : result of convertion
 * \note this function supposed that hexDigest is already allocated
 * and its size is, at least, 2 * length + 1
 */
void checksum_convert2Hex(unsigned char * digest, int length, char * hexDigest);

/**
 * \brief get a checksum driver
 * \param driver : driver's name
 * \return 0 if failed
 * \note if this driver is not loaded, we try to load it
 */
struct checksum_driver * checksum_getDriver(const char * driver);

/**
 * \brief checksum_loadDriver will try to load a checksum driver
 * \param checksum : name of checksum's driver
 * \return a value which correspond to
 * \li 0 if ok
 * \li 1 if permission error
 * \li 2 if module didn't call checksum_registerDriver
 * \li 3 if checksum is null
 */
int checksum_loadDriver(const char * checksum);

/**
 * \brief Each checksum driver should call this function only one time
 * \param driver : a statically allocated struct checksum_driver
 * \code
 * __attribute__((constructor))
 * static void checksum_mychecksum_init() {
 *    checksum_registerDriver(&checksum_mychecksum_driver);
 * }
 * \endcode
 */
void checksum_registerDriver(struct checksum_driver * driver);

#endif

