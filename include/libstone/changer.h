/****************************************************************************\
*                             __________                                     *
*                            / __/_  __/__  ___  ___                         *
*                           _\ \  / / / _ \/ _ \/ -_)                        *
*                          /___/ /_/  \___/_//_/\__/                         *
*                                                                            *
*  ------------------------------------------------------------------------  *
*  This file is a part of STone                                              *
*                                                                            *
*  STone is free software; you can redistribute it and/or modify             *
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

#ifndef __LIBSTONE_CHANGER_H__
#define __LIBSTONE_CHANGER_H__

// bool
#include <stdbool.h>

struct st_drive;
struct st_slot;
struct st_value;

/**
 * \enum st_changer_action
 * \brief Next action of changer
 */
enum st_changer_action {
	/**
	 * \brief No action
	 */
	st_changer_action_none = 0x1,
	st_changer_action_put_offline = 0x2,
	st_changer_action_put_online = 0x3,

	/**
	 * \brief Used in case of errors
	 */
	st_changer_action_unknown = 0x0,
};

/**
 * \enum st_changer_status
 * \brief Status of changer
 */
enum st_changer_status {
	st_changer_status_error = 0x1,
	st_changer_status_exporting = 0x2,
	/**
	 * \brief The library is going offline
	 */
	st_changer_status_go_offline = 0x3,
	/**
	 * \brief The library is going online
	 */
	st_changer_status_go_online = 0x4,
	/**
	 * \brief The library is waiting for a command
	 */
	st_changer_status_idle = 0x5,
	st_changer_status_importing = 0x6,
	/**
	 * \brief The library is loading a media into a drive
	 */
	st_changer_status_loading = 0x7,
	/**
	 * \brief The library is offline
	 */
	st_changer_status_offline = 0x8,
	/**
	 * \brief The library is unloading a media from a drive
	 */
	st_changer_status_unloading = 0x9,

	st_changer_status_unknown = 0x0,
};

/**
 * \struct st_changer
 * \brief Common structure for all changers
 */
struct st_changer {
	/**
	 * \brief Model of this \a changer
	 */
	char * model;
	/**
	 * \brief Vendor of this \a changer
	 */
	char * vendor;
	/**
	 * \brief Revision of this \a changer
	 */
	char * revision;
	/**
	 * \brief Serial number of this \a changer
	 */
	char * serial_number;
	/**
	 * \brief World Wide Name of device
	 */
	char * wwn;
	/**
	 * \brief This changer has got a barcode reader
	 */
	bool barcode;

	/**
	 * \brief Status of changer
	 */
	enum st_changer_status status;
	enum st_changer_action next_action;
	bool is_online;
	/**
	 * \brief Can use this \a changer
	 */
	bool enable;

	/**
	 * \brief Drives into this \a changer
	 */
	struct st_drive * drives;
	/**
	 * \brief Number of drives into this \a changer
	 */
	unsigned int nb_drives;

	/**
	 * \brief Slots into this \a changer
	 */
	struct st_slot * slots;
	/**
	 * \brief Number of slots into this \a changer
	 */
	unsigned int nb_slots;

	struct st_changer_ops * ops;

	void * data;
	/**
	 * \brief Private data used by database plugin
	 */
	struct st_value * db_data;
};

/**
 * \brief Convert an enumeration info static allocated string
 *
 * \param[in] action : a value of enum st_changer_action
 * \return a static allocated string representing \a action
 */
const char * st_changer_action_to_string(enum st_changer_action action);
/**
 * \brief Convert an instance of \a changer into an object
 */
struct st_value * st_changer_convert(struct st_changer * changer) __attribute__((nonnull,warn_unused_result));
void st_changer_free(struct st_changer * changer) __attribute__((nonnull));
void st_changer_free2(void * changer) __attribute__((nonnull));
const char * st_changer_status_to_string(enum st_changer_status status);
enum st_changer_action st_changer_string_to_action(const char * action) __attribute__((nonnull));
enum st_changer_status st_changer_string_to_status(const char * status) __attribute__((nonnull));
void st_changer_sync(struct st_changer * changer, struct st_value * new_changer) __attribute__((nonnull));

#endif

