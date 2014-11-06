/****************************************************************************\
*                    ______           _      ____                            *
*                   / __/ /____  ____(_)__ _/ __ \___  ___                   *
*                  _\ \/ __/ _ \/ __/ / _ `/ /_/ / _ \/ -_)                  *
*                 /___/\__/\___/_/ /_/\_, /\____/_//_/\__/                   *
*                                      /_/                                   *
*  ------------------------------------------------------------------------  *
*  This file is a part of Storiq One                                         *
*                                                                            *
*  Storiq One is free software; you can redistribute it and/or modify        *
*  it under the terms of the GNU Affero General Public License               *
*  as published by the Free Software Foundation; either version 3            *
*  of the License, or (at your option) any later version.                    *
*                                                                            *
*  This program is distributed in the hope that it will be useful,           *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of            *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
*  GNU Affero General Public License for more details.                       *
*                                                                            *
*  You should have received a copy of the GNU Affero General Public License  *
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.     *
*                                                                            *
*  ------------------------------------------------------------------------  *
*  Copyright (C) 2014, Clercin guillaume <gclercin@intellique.com>           *
\****************************************************************************/

#ifndef __LIBSTORIQONE_CHECKSUM_H__
#define __LIBSTORIQONE_CHECKSUM_H__

// bool
#include <stdbool.h>
// ssize_t
#include <sys/types.h>

/**
 * \addtogroup UseChecksum Use checksum module
 * This page show how to compute checksum with this module
 *
 * \par This example shows how can this module be used :
 * \code
 * // compute md5 checksum of 'Hello, world!!!' string
 * // get the md5 driver
 * struct so_checksum_driver * driver = so_checksum_get_driver("md5");
 * // don't forget to check value of driver
 * // get an md5 handler
 * struct so_checksum * handler = driver->new_checksum();
 * // handler is not supposed to be NULL
 * handler->ops->update(handler, "Hello, ", 7);
 * handler->ops->update(handler, "world !!!", 9);
 * // compute digest and result should be 'e7a20f74338897759a77a1497314719f'
 * char * digest = handler->ops->digest(handler);
 * // don't forget to release memory
 * free(digest);
 * handler->ops->free(handler);
 * \endcode
 *
 * \par Get a new handler with an handler
 * \code
 * // get the driver
 * struct so_checksum_driver * driver = handler->driver;
 * // get a second handler
 * struct so_checksum * handler2 = driver->new_checksum();
 * \endcode
 *
 * \par Continue with same handler
 * \code
 * // get the md5 driver
 * struct so_checksum_driver * driver = so_checksum_get_driver("md5");
 * // get an md5 handler
 * struct so_checksum * handler = driver->new_checksum();
 * handler->ops->update(handler, "Hello, ", 7);
 * // compute digest and result should be 'c84cabbaebee9a9631c8be234ac64c26'
 * char * digest = handler->ops->digest(handler);
 * // release digest
 * free(digest);
 * // continue
 * handler->ops->update(handler, "world !!!", 9);
 * // compute digest and, now, result should be 'e7a20f74338897759a77a1497314719f'
 * digest = handler->ops->digest(handler);
 * // don't forget to release memory
 * free(digest);
 * handler->ops->free(handler);
 * \endcode
 */

/**
 * \addtogroup WriteChecksum Write a checksum module
 * This page show how to write a checksum module
 *
 * This example show you how the md5 checksum is implemeted
 * \code
 * // free, malloc
 * #include <stdlib.h>
 * // MD5_Final, MD5_Init, MD5_Update
 * #include <openssl/md5.h>
 * // strdup
 * #include <string.h>
 *
 * #include <libstone/checksum.h>
 *
 * #include <libchecksum-md5.chcksum>
 *
 * struct so_checksum_md5_private {
 * 	  MD5_CTX md5;
 * 	  char digest[MD5_DIGEST_LENGTH * 2 + 1];
 * };
 *
 * static char * so_checksum_md5_digest(struct so_checksum * checksum);
 * static void so_checksum_md5_free(struct so_checksum * checksum);
 * static struct so_checksum * so_checksum_md5_new_checksum(void);
 * static void so_checksum_md5_init(void) __attribute__((constructor));
 * static ssize_t so_checksum_md5_update(struct so_checksum * checksum, const void * data, ssize_t length);
 *
 * static struct so_checksum_driver so_checksum_md5_driver = {
 *    .name             = "md5",
 *    .default_checksum = false,
 *    .new_checksum     = so_checksum_md5_new_checksum,
 *    .cookie           = NULL,
 *    .api_level        = {
 *        .checksum = STONE_CHECKSUM_API_LEVEL,
 *        .database = 0,
 *        .job      = 0,
 *    },
 *    .src_checksum     = STONE_CHECKSUM_MD5_SRCSUM,
 * };
 *
 * static struct so_checksum_ops so_checksum_md5_ops = {
 *    .digest = so_checksum_md5_digest,
 *    .free   = so_checksum_md5_free,
 *    .update = so_checksum_md5_update,
 * };
 *
 *
 * char * so_checksum_md5_digest(struct so_checksum * checksum) {
 *    if (checksum == NULL)
 *       return NULL;
 *
 *    struct so_checksum_md5_private * self = checksum->data;
 *    if (self->digest[0] != '\0')
 *       return strdup(self->digest);
 *
 *    MD5_CTX md5 = self->md5;
 *    unsigned char digest[MD5_DIGEST_LENGTH];
 *    if (!MD5_Final(digest, &md5))
 *       return NULL;
 *
 *    so_checksum_convert_to_hex(digest, MD5_DIGEST_LENGTH, self->digest);
 *
 *    return strdup(self->digest);
 * }
 *
 * void so_checksum_md5_free(struct so_checksum * checksum) {
 *    if (checksum == NULL)
 *       return;
 *
 *    struct so_checksum_md5_private * self = checksum->data;
 *
 *    unsigned char digest[MD5_DIGEST_LENGTH];
 *    MD5_Final(digest, &self->md5);
 *
 *    free(self);
 *
 *    checksum->data = NULL;
 *    checksum->ops = NULL;
 *    checksum->driver = NULL;
 *
 *    free(checksum);
 * }
 *
 * void so_checksum_md5_init() {
 *    so_checksum_register_driver(&so_checksum_md5_driver);
 * }
 *
 * struct so_checksum * so_checksum_md5_new_checksum(struct so_checksum * checksum) {
 *    struct so_checksum * checksum = malloc(sizeof(struct so_checksum));
 *    checksum->ops = &so_checksum_md5_ops;
 *    checksum->driver = &so_checksum_md5_driver;
 *
 *    struct so_checksum_md5_private * self = malloc(sizeof(struct so_checksum_md5_private));
 *    MD5_Init(&self->md5);
 *    *self->digest = '\0';
 *
 *    checksum->data = self;
 *    return checksum;
 * }
 *
 * ssize_t so_checksum_md5_update(struct so_checksum * checksum, const void * data, ssize_t length) {
 *    if (!checksum || !data || length < 1)
 *       return -1;
 *
 *    struct so_checksum_md5_private * self = checksum->data;
 *    if (MD5_Update(&self->md5, data, length)) {
 *       *self->digest = '\0';
 *       return length;
 *    }
 *
 *    return -1;
 * }
 *
 * \endcode
 */

/**
 * \struct so_checksum
 * \brief This structure is used as an handler to a specific checksum
 */
struct so_checksum {
	/**
	 * \struct so_checksum_ops
	 * \brief This structure contains all functions associated to one checksum handler
	 */
	struct so_checksum_ops {
		/**
		 * \brief Compute digest and return it
		 *
		 * \param[in] checksum a checksum handler
		 * \return a malloc allocated string which contains a digest in hexadecimal form
		 */
		char * (*digest)(struct so_checksum * checksum) __attribute__((nonnull,warn_unused_result));
		/**
		 * \brief This function releases all memory associated to checksum
		 *
		 * \param[in] checksum a checksum handler
		 */
		void (*free)(struct so_checksum * checksum) __attribute__((nonnull));
		/**
		 * \brief This function reads some data
		 *
		 * \param[in] checksum a checksum handler
		 * \param[in] data some or full data
		 * \param[in] length length of data
		 * \return < 0 if error
		 * \li -1 if param error
		 * \li \a length is ok
		 *
		 * \note this function can be called after function digest
		 */
		ssize_t (*update)(struct so_checksum * checksum, const void * data, ssize_t length) __attribute__((nonnull(1)));
	} * ops;

	/**
	 * \brief Driver associated
	 *
	 * \note Should not be NULL
	 */
	struct so_checksum_driver * driver;
	/**
	 * \brief Private data of one checksum
	 */
	void * data;
};

/**
 * \struct so_checksum_driver
 * \brief This structure allows to get new checksum handler
 */
struct so_checksum_driver {
	/**
	 * \brief Name of the driver
	 *
	 * \note Should be unique and equals to libchecksum-name.so where name is the name of driver.
	 */
	const char * name;
	/**
	 * \brief Is this checksum should be used by default
	 */
	bool default_checksum;

	/**
	 * \brief Get a new checksum handler
	 *
	 * \return a structure which allows to compute checksum
	 */
	struct so_checksum * (*new_checksum)(void) __attribute__((warn_unused_result));

	/**
	 * \brief Private data used by each checksum handler
	 */
	void * cookie;
	/**
	 * \brief Sha1 sum of plugins source code
	 */
	const char * src_checksum;
};


/**
 * \brief Simple function to compute checksum provided for convenience.
 *
 * \param[in] checksum name of checksum plugin
 * \param[in] data compute with this \a data
 * \param[in] length length of \a data
 * \return a malloc allocated string which contains checksum or \b NULL if failed
 * \note Returned value should be release with \a free
 */
char * so_checksum_compute(const char * checksum, const void * data, ssize_t length) __attribute__((nonnull(1),warn_unused_result));

/**
 * \brief This function converts a digest into hexadecimal form
 *
 * \param[in] digest a digest
 * \param[in] length length of digest in bytes
 * \param[out] hex_digest result of conversion
 *
 * \note this function suppose that \a hexDigest is already allocated
 * and its size is, at least, \f$ 2 length + 1 \f$
 */
void so_checksum_convert_to_hex(unsigned char * digest, ssize_t length, char * hex_digest) __attribute__((nonnull));

char * so_checksum_gen_salt(const char * checksum, size_t length) __attribute__((warn_unused_result));

struct so_checksum_driver * so_checksum_get_driver(const char * driver) __attribute__((nonnull));

/**
 * \brief Register a checksum driver
 *
 * \param[in] driver a statically allocated <c>struct checksum_driver</c>
 *
 * \note Each checksum driver should call this function only one time
 * \code
 * static void mychecksum_init() __attribute__((constructor)) {
 *    checksum_registerDriver(&mychecksum_driver);
 * }
 * \endcode
 */
void so_checksum_register_driver(struct so_checksum_driver * driver) __attribute__((nonnull));

char * so_checksum_salt_password(const char * checksum, const char * password, const char * salt) __attribute__((nonnull,warn_unused_result));

#endif
