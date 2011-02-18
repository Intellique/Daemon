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
*  Last modified: Thu, 21 Oct 2010 15:48:50 +0200                       *
\***********************************************************************/

#ifndef __STORIQARCHIVER_CHECKSUM_H__
#define __STORIQARCHIVER_CHECKSUM_H__

// forward declaration
struct checksum;

/**
 * \addtogroup UseChecksum Use checksum module
 * This page show how to compute checksum with this module
 *
 * \par This example shows how can this module be used :
 * \code
 * // compute md5 sum of 'Hello, world!!!' string
 * // get the md5 driver
 * struct checksum_driver * driver = checksum_getDriver("md5");
 * // don't forget to check value of driver
 * // get an md5 handler
 * struct checksum * handler = driver->new_checksum(driver, 0);
 * // handler is not supposed to be NULL
 * handler->ops->update(handler, "Hello, ", 7);
 * handler->ops->update(handler, "world !!!", 9);
 * // compute digest and result should be 'e7a20f74338897759a77a1497314719f'
 * char * digest = handler->ops->finish(handler);
 * // don't forget to release memory
 * free(digest);
 * handler->ops->free(handler);
 * // handler has been allocated by new_checksum with malloc
 * free(handler);
 * \endcode
 *
 * \par Get a new handler with an handler
 * \code
 * // get the driver
 * struct checksum_driver * driver = handler->driver;
 * // get a second handler
 * struct checksum * handler2 = driver->new_checksum(driver, 0);
 * \endcode
 *
 * \par Another way to use new_checksum
 * \code
 * // get the sha1 driver
 * struct checksum_driver * driver = checksum_getDriver("sha1");
 * struct checksum handler;
 * driver->new_checksum(driver, &handler);
 * // ...
 * handler.ops->free(&handler);
 * // Warning: you SHOULD NOT call free(&hanler)
 * \endcode
 *
 * \par Another example
 * \code
 * // get the md5 driver
 * struct checksum_driver * driver_md5 = checksum_getDriver("md5");
 * // get the sha1 driver
 * struct checksum_driver * driver_sha1 = checksum_getDriver("sha1");
 *
 * // an array which contains two handlers
 * struct checksum * handler = calloc(2, sizeof(struct checksum));
 * driver_md5->new_checksum(driver_md5, handler);
 * driver_sha1->new_checksum(driver_sha1, handler + 1);
 *
 * // Supposes that fd is valid file descriptor
 * char buffer[1024];
 * int nbRead, i;
 * while ((nbRead = read(fd, buffer, 1024)) > 0) {
 *    for (i = 0; i < 2; i++)
 *       handler[i].ops->update(handler + i, buffer, nbRead);
 * }
 *
 * // now, get the results
 * char * digest_md5 = handler[0].ops->finish(handler);
 * char * digest_sha1 = handler[1].ops->finish(handler + 1);
 *
 * // release memory
 * for (i = 0; i < 2; i++)
 *    handler[i].ops->free(handler + i);
 * free(handler);
 *
 * // don't forget to release digest_md5 and digest_sha1
 * free(digest_md5);
 * free(digest_sha1);
 * \endcode
 */

/**
 * \addtogroup WriteChecksum Write a checksum module
 * This page show how to write a checksum module
 *
 * This example show you how the md5 checksum is implemeted
 * \code
 * // malloc
 * #include <malloc.h>
 * // MD5_Final, MD5_Init, MD5_Update
 * #include <openssl/md5.h>
 *
 * // checksum_convert2Hex
 * #include <storiqArchiver/checksum.h>
 *
 * // structure used to store private data into handler.data
 * struct checksum_md5_private {
 *    MD5_CTX md5;
 *    short finished;
 * };
 *
 * static char * checksum_md5_finish(struct checksum * checksum);
 * static void checksum_md5_free(struct checksum * checksum);
 * static struct checksum * checksum_md5_new_checksum(struct checksum_driver * driver, struct checksum * checksum);
 * static int checksum_md5_update(struct checksum * checksum, const char * data, unsigned int length);
 *
 * static struct checksum_driver checksum_md5_driver = {
 *    .name = "md5",
 *    .new_checksum = checksum_md5_new_checksum,
 *    .cookie = 0,
 * };
 *
 * static struct checksum_ops checksum_md5_ops = {
 *    .finish = checksum_md5_finish,
 *    .free = checksum_md5_free,
 *    .update = checksum_md5_update,
 * };
 *
 *
 * char * checksum_md5_finish(struct checksum * checksum) {
 *    struct checksum_md5_private * self = checksum->data;
 *
 *    if (self->finished)
 *       return 0;
 *
 *    unsigned char digest[MD5_DIGEST_LENGTH];
 *
 *    short ok = MD5_Final(digest, &self->md5);
 *    self->finished = 1;
 *    if (!ok)
 *       return 0;
 *
 *    char * hexDigest = malloc(MD5_DIGEST_LENGTH * 2 + 1);
 *    checksum_convert2Hex(digest, MD5_DIGEST_LENGTH, hexDigest);
 *
 *    return hexDigest;
 * }
 *
 * void checksum_md5_free(struct checksum * checksum) {
 *    struct checksum_md5_private * self = checksum->data;
 *
 *    if (!self->finished) {
 *       unsigned char digest[MD5_DIGEST_LENGTH];
 *       MD5_Final(digest, &self->md5);
 *       self->finished = 1;
 *    }
 *
 *    free(self);
 *
 *    checksum->data = 0;
 *    checksum->ops = 0;
 *    checksum->driver = 0;
 * }
 *
 * // When you load this module, this function is automaticaly called
 * __attribute__((constructor))
 * static void checksum_md5_init() {
 *    checksum_registerDriver(&checksum_md5_driver);
 * }
 *
 * struct checksum * checksum_md5_new_checksum(struct checksum * checksum) {
 *    // if checksum is NULL, you allocate a new with malloc
 *    if (!checksum)
 *       checksum = malloc(sizeof(struct checksum));
 *
 *    checksum->ops = &checksum_md5_ops;
 *    checksum->driver = driver;
 *
 *    struct checksum_md5_private * self = malloc(sizeof(struct checksum_md5_private));
 *    MD5_Init(&self->md5);
 *    self->finished = 0;
 *
 *    checksum->data = self;
 *    return checksum;
 * }
 *
 * int checksum_md5_update(struct checksum * checksum, const char * data, unsigned int length) {
 *    struct checksum_md5_private * self = checksum->data;
 *
 *    if (self->finished)
 *       return -1;
 *
 *    if (MD5_Update(&self->md5, data, length))
 *       return length;
 *
 *    return -1;
 * }
 * \endcode
 */

/**
 * \struct checksum_ops
 * \brief this structure contains only pointer functions
 * \note all functions \b SHOULD point on <b>real function</b>
 */
struct checksum_ops {
	/**
	 * \brief this function computes a digest
	 * \param checksum : a checksum handler
	 * \return a dynamically allocated string which contains a digest in hexadecimal form
	 */
	char * (*finish)(struct checksum * checksum);
	/**
	 * \brief this function releases all memory associated to ckecksum
	 * \param checksum : a checksum handler
	 * \warning this function SHOULD NOT call :
	 * \code
	 * free(checksum);
	 * \endcode
	 */
	void (*free)(struct checksum * checksum);
	/**
	 * \brief this function reads some data
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
	 * \note <b>SHOULD NOT BE NULL</b>
	 */
	struct checksum_driver * driver;
};

/**
 * \struct checksum_driver
 * \brief This structure allows to get new checksum handler
 */
struct checksum_driver {
	/**
	 * \brief name of the driver
	 */
	char * name;
	/**
	 * \brief get a new checksum handler
	 * \param checksum : an allocated checksum or \b NULL
	 * \return same value of \a checksum if \a checksum if <b>NOT NULL</b> or new value
	 * \note if \a checksum is \b NULL, this function allocate enough memory with \a malloc
	 */
	struct checksum * (*new_checksum)(struct checksum * checksum);
	/**
	 * \brief private data used by checksum_loadDriver
	 * \note <b>SHOULD NOT be MODIFIED</b>
	 */
	void * cookie;
};


/**
 * \brief This function converts a digest into hexadecimal form
 * \param digest : a digest
 * \param length : length of digest in bytes
 * \param hexDigest : result of convertion
 * \note this function supposed that hexDigest is already allocated
 * and its size is, at least, \f$ 2 length + 1 \f$
 */
void checksum_convert2Hex(unsigned char * digest, int length, char * hexDigest);

/**
 * \brief get a checksum driver
 * \param driver : driver's name
 * \return 0 if failed
 * \note if this driver is not loaded, we try to load it
 * \warning <b>DO NOT RELEASE</b> memory with \a free
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
 * \param driver : a statically allocated <c>struct checksum_driver</c>
 * \code
 * __attribute__((constructor))
 * static void checksum_mychecksum_init() {
 *    checksum_registerDriver(&checksum_mychecksum_driver);
 * }
 * \endcode
 */
void checksum_registerDriver(struct checksum_driver * driver);

#endif

