/*************************************************************************\
*        ______           ____     ___           __   _                   *
*       / __/ /____  ____/  _/__ _/ _ | ________/ /  (_)  _____ ____      *
*      _\ \/ __/ _ \/ __// // _ `/ __ |/ __/ __/ _ \/ / |/ / -_) __/      *
*     /___/\__/\___/_/ /___/\_, /_/ |_/_/  \__/_//_/_/|___/\__/_/         *
*                            /_/                                          *
*  ---------------------------------------------------------------------  *
*  This file is a part of StorIqArchiver                                  *
*                                                                         *
*  StorIqArchiver is free software; you can redistribute it and/or        *
*  modify it under the terms of the GNU General Public License            *
*  as published by the Free Software Foundation; either version 3         *
*  of the License, or (at your option) any later version.                 *
*                                                                         *
*  This program is distributed in the hope that it will be useful,        *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
*  GNU General Public License for more details.                           *
*                                                                         *
*  You should have received a copy of the GNU General Public License      *
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  Copyright (C) 2011, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Tue, 22 Nov 2011 11:20:52 +0100                         *
\*************************************************************************/

#ifndef __STORIQARCHIVER_CHECKSUM_H__
#define __STORIQARCHIVER_CHECKSUM_H__

/**
 * \addtogroup UseChecksum Use checksum module
 * This page show how to compute checksum with this module
 *
 * \par This example shows how can this module be used :
 * \code
 * // compute md5 sum of 'Hello, world!!!' string
 * // get the md5 driver
 * struct sa_checksum_driver * driver = sa_checksum_get_driver("md5");
 * // don't forget to check value of driver
 * // get an md5 handler
 * struct sa_checksum * handler = driver->new_checksum(0);
 * // handler is not supposed to be NULL
 * handler->ops->update(handler, "Hello, ", 7);
 * handler->ops->update(handler, "world !!!", 9);
 * // compute digest and result should be 'e7a20f74338897759a77a1497314719f'
 * char * digest = handler->ops->digest(handler);
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
 * struct sa_checksum_driver * driver = handler->driver;
 * // get a second handler
 * struct sa_checksum * handler2 = driver->new_checksum(0);
 * \endcode
 *
 * \par Another way to use new_checksum
 * \code
 * // get the sha1 driver
 * struct sa_checksum_driver * driver = sa_checksum_get_driver("sha1");
 * struct sa_checksum handler;
 * driver->new_checksum(&handler);
 * // ...
 * handler.ops->free(&handler);
 * // Warning: you SHOULD NOT call free(&hanler)
 * \endcode
 *
 * \par Another example
 * \code
 * // get the md5 driver
 * struct sa_checksum_driver * driver_md5 = sa_checksum_get_driver("md5");
 * // get the sha1 driver
 * struct sa_checksum_driver * driver_sha1 = sa_checksum_get_driver("sha1");
 *
 * // an array which contains two handlers
 * struct sa_checksum * handler = calloc(2, sizeof(struct checksum));
 * driver_md5->new_checksum(handler);
 * driver_sha1->new_checksum(handler + 1);
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
 * char * digest_md5 = handler[0].ops->digest(handler);
 * char * digest_sha1 = handler[1].ops->digest(handler + 1);
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
 * // free, malloc
 * #include <malloc.h>
 * // MD5_Final, MD5_Init, MD5_Update
 * #include <openssl/md5.h>
 * // strdup
 * #include <string.h>
 * 
 * #include <storiqArchiver/checksum.h>
 * 
 * struct sa_checksum_md5_private {
 * 	MD5_CTX md5;
 * 	char digest[MD5_DIGEST_LENGTH * 2 + 1];
 * };
 * 
 * static struct sa_checksum * sa_checksum_md5_clone(struct sa_checksum * new_checksum, struct sa_checksum * current_checksum);
 * static char * sa_checksum_md5_digest(struct sa_checksum * checksum);
 * static void sa_checksum_md5_free(struct sa_checksum * checksum);
 * static struct sa_checksum * sa_checksum_md5_new_checksum(struct sa_checksum * checksum);
 * static void sa_checksum_md5_init(void) __attribute__((constructor));
 * static ssize_t sa_checksum_md5_update(struct sa_checksum * checksum, const void * data, ssize_t length);
 * 
 * static struct sa_checksum_driver sa_checksum_md5_driver = {
 * 	.name         = "md5",
 * 	.new_checksum = sa_checksum_md5_new_checksum,
 * 	.cookie       = 0,
 * 	.api_version  = STORIQARCHIVER_CHECKSUM_APIVERSION,
 * };
 * 
 * static struct sa_checksum_ops sa_checksum_md5_ops = {
 * 	.clone  = sa_checksum_md5_clone,
 * 	.digest = sa_checksum_md5_digest,
 * 	.free   = sa_checksum_md5_free,
 * 	.update = sa_checksum_md5_update,
 * };
 * 
 * 
 * struct sa_checksum * sa_checksum_md5_clone(struct sa_checksum * new_checksum, struct sa_checksum * current_checksum) {
 * 	if (!current_checksum)
 * 		return 0;
 * 
 * 	struct sa_checksum_md5_private * current_self = current_checksum->data;
 * 
 * 	if (!new_checksum)
 * 		new_checksum = malloc(sizeof(struct sa_checksum));
 * 
 * 	new_checksum->ops = &sa_checksum_md5_ops;
 * 	new_checksum->driver = &sa_checksum_md5_driver;
 * 
 * 	struct sa_checksum_md5_private * new_self = malloc(sizeof(struct sa_checksum_md5_private));
 * 	*new_self = *current_self;
 * 
 * 	new_checksum->data = new_self;
 * 	return new_checksum;
 * }
 * 
 * char * sa_checksum_md5_digest(struct sa_checksum * checksum) {
 * 	if (!checksum)
 * 		return 0;
 * 
 * 	struct sa_checksum_md5_private * self = checksum->data;
 * 
 * 	if (self->digest)
 * 		return strdup(self->digest);
 * 
 * 	MD5_CTX md5 = self->md5;
 * 	unsigned char digest[MD5_DIGEST_LENGTH];
 * 	if (!MD5_Final(digest, &md5))
 * 		return 0;
 * 
 * 	sa_checksum_convert_to_hex(digest, MD5_DIGEST_LENGTH, self->digest);
 * 
 * 	return strdup(self->digest);
 * }
 * 
 * void sa_checksum_md5_free(struct sa_checksum * checksum) {
 * 	if (!checksum)
 * 		return;
 * 
 * 	struct sa_checksum_md5_private * self = checksum->data;
 * 
 * 	if (self)
 * 		free(self);
 * 
 * 	checksum->data = 0;
 * 	checksum->ops = 0;
 * 	checksum->driver = 0;
 * }
 * 
 * void sa_checksum_md5_init() {
 * 	sa_checksum_register_driver(&sa_checksum_md5_driver);
 * }
 * 
 * struct sa_checksum * sa_checksum_md5_new_checksum(struct sa_checksum * checksum) {
 * 	if (!checksum)
 * 		checksum = malloc(sizeof(struct sa_checksum));
 * 
 * 	checksum->ops = &sa_checksum_md5_ops;
 * 	checksum->driver = &sa_checksum_md5_driver;
 * 
 * 	struct sa_checksum_md5_private * self = malloc(sizeof(struct sa_checksum_md5_private));
 * 	MD5_Init(&self->md5);
 * 	*self->digest = '\0';
 * 
 * 	checksum->data = self;
 * 	return checksum;
 * }
 * 
 * ssize_t sa_checksum_md5_update(struct sa_checksum * checksum, const void * data, ssize_t length) {
 * 	if (!checksum)
 * 		return -1;
 * 
 * 	struct sa_checksum_md5_private * self = checksum->data;
 * 	*self->digest = '\0';
 * 
 * 	if (MD5_Update(&self->md5, data, length))
 * 		return length;
 * 
 * 	return -1;
 * }
 * 
 * \endcode
 */

/**
 * \struct sa_checksum
 * \brief this structure is used as an handler to a specific checksum
 */
struct sa_checksum {
	/**
	 * \struct sa_checksum_ops
	 * \brief this structure contains only pointer functions
	 * \note all functions \b SHALL point on <b>real function</b>
	 */
	struct sa_checksum_ops {
		/**
		 * \brief Create new checksum by cloning a checksum
		 * \param[out] new_checksum : an allocated checksum or \b NULL
		 * \param[in] current_checksum : clone this checksum
		 * \return same value of \a new_checksum if \a new_checksum if <b>NOT NULL</b> or new value or NULL if \a current_checksum is NULL
		 * \note current_checksum SHOULD NOT BE NULL, if \a new_checksum is \b NULL, this function allocate enough memory with \a malloc
		 */
		struct sa_checksum * (*clone)(struct sa_checksum * new_checksum, struct sa_checksum * current_checksum);
		/**
		 * \brief compute digest and return it
		 * \param[in] checksum : a checksum handler
		 * \return a dynamically allocated string which contains a digest in hexadecimal form
		 */
		char * (*digest)(struct sa_checksum * checksum);
		/**
		 * \brief this function releases all memory associated to ckecksum
		 * \param[in] checksum : a checksum handler
		 * \warning this function <b>SHALL NOT call</b> :
		 * \code
		 * free(checksum);
		 * \endcode
		 */
		void (*free)(struct sa_checksum * checksum);
		/**
		 * \brief this function reads some data
		 * \param[in] checksum : a checksum handler
		 * \param[in] data : some or full data
		 * \param[in] length : length of data
		 * \return < 0 if error
		 * \note this function can be called one or many times until function digest is called
		 */
		ssize_t (*update)(struct sa_checksum * checksum, const void * data, ssize_t length);
	} * ops;
	/**
	 * \brief private data of one checksum
	 */
	void * data;
	/**
	 * \brief driver associated
	 * \note <b>SHOULD NOT BE NULL</b>
	 */
	struct sa_checksum_driver * driver;
};

/**
 * \struct sa_checksum_driver
 * \brief This structure allows to get new checksum handler
 */
struct sa_checksum_driver {
	/**
	 * \brief name of the driver
	 */
	char * name;
	/**
	 * \brief get a new checksum handler
	 * \param[out] checksum : an allocated checksum or \b NULL
	 * \return same value of \a checksum if \a checksum if <b>NOT NULL</b> or new value
	 * \note if \a checksum is \b NULL, this function allocate enough memory with \a malloc
	 */
	struct sa_checksum * (*new_checksum)(struct sa_checksum * checksum);
	/**
	 * \brief private data used by sa_checksum_get_driver
	 * \note <b>SHOULD NOT be MODIFIED</b>
	 */
	void * cookie;
	/**
	 * \brief check if the driver have an up to date api version
	 * \note Should be defined to STORIQARCHIVER_CHECKSUM_APIVERSION
	 */
	int api_version;
};

/**
 * \brief Current api version
 *
 * Will increment from new version of struct sa_checksum_driver or struct sa_checksum
 */
#define STORIQARCHIVER_CHECKSUM_APIVERSION 1


/**
 * \brief Simple function to compute checksum
 * \param[in] checksum : name of checksum plugin
 * \param[in] data : compute with this \a data
 * \param[in] length : length of \a data
 * \return dynamically allocated string which contains checksum or NULL if failed
 */
char * sa_checksum_compute(const char * checksum, const char * data, unsigned int length);

/**
 * \brief This function converts a digest into hexadecimal form
 * \param[in] digest : a digest
 * \param[in] length : length of digest in bytes
 * \param[out] hexDigest : result of convertion
 * \note this function supposed that hexDigest is already allocated
 * and its size is, at least, \f$ 2 length + 1 \f$
 */
void sa_checksum_convert_to_hex(unsigned char * digest, int length, char * hexDigest);

/**
 * \brief get a checksum driver
 * \param[in] driver : driver's name
 * \return 0 if failed
 * \note if this driver is not loaded, we try to load it
 * \warning the returned value <b>SHALL NOT BE RELEASE</b> with \a free
 */
struct sa_checksum_driver * sa_checksum_get_driver(const char * driver);

/**
 * \brief get a thread helper
 * \param[out] helper : an allocated checksum or \b NULL
 * \param[in] checksum : clone this checksum
 * \return same value of \a helper if \a helper if <b>NOT NULL</b> or new value or NULL if \a checksum is NULL
 * \note checksum SHOULD NOT BE NULL, if \a helper is \b NULL, this function allocate enough memory with \a malloc.
 * \note checksum will be computed into another thread.
 */
struct sa_checksum * sa_checksum_get_helper(struct sa_checksum * helper, struct sa_checksum * checksum);

/**
 * \brief Register an checksum driver
 * \param[in] driver : a statically allocated <c>struct checksum_driver</c>
 * \note Each checksum driver should call this function only one time
 * \code
 * static void mychecksum_init() __attribute__((constructor)) {
 *    checksum_registerDriver(&mychecksum_driver);
 * }
 * \endcode
 */
void sa_checksum_register_driver(struct sa_checksum_driver * driver);

#endif

