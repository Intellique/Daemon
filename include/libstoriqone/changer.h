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
*  Copyright (C) 2013-2016, Guillaume Clercin <gclercin@intellique.com>      *
\****************************************************************************/

#ifndef __LIBSTORIQONE_CHANGER_H__
#define __LIBSTORIQONE_CHANGER_H__

// bool
#include <stdbool.h>

struct so_drive;
struct so_slot;
struct so_value;

/**
 * \enum so_changer_action
 * \brief Next action to be performed on changer
 */
enum so_changer_action {
	/**
	 * \brief No action scheduled
	 */
	so_changer_action_none = 0x1,
	/**
	 * \brief Resquest changer to be offline
	 */
	so_changer_action_put_offline = 0x2,
	/**
	 * \brief Request changer to be online
	 */
	so_changer_action_put_online = 0x3,

	/**
	 * \brief Used in case of errors
	 *
	 * \note Should not be used
	 */
	so_changer_action_unknown = 0x0,
};

/**
 * \enum so_changer_status
 * \brief Status of changer
 */
enum so_changer_status {
	/**
	 * \brief Harware failure
	 */
	so_changer_status_error = 0x1,
	so_changer_status_exporting = 0x2,
	/**
	 * \brief The library is going offline
	 */
	so_changer_status_go_offline = 0x3,
	/**
	 * \brief The library is going online
	 */
	so_changer_status_go_online = 0x4,
	/**
	 * \brief The library is waiting for a command
	 */
	so_changer_status_idle = 0x5,
	so_changer_status_importing = 0x6,
	/**
	 * \brief The library is loading a media into a drive
	 */
	so_changer_status_loading = 0x7,
	/**
	 * \brief The library is offline
	 */
	so_changer_status_offline = 0x8,
	/**
	 * \brief The library is unloading a media from a drive
	 */
	so_changer_status_unloading = 0x9,

	/**
	 * \brief Used in case of errors
	 *
	 * \note Should not be used
	 */
	so_changer_status_unknown = 0x0,
};

/**
 * \struct so_changer
 * \brief Common structure for all changers
 */
struct so_changer {
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
	enum so_changer_status status;
	/**
	 * \brief Next action scheduled
	 */
	enum so_changer_action next_action;
	/**
	 * \brief status of changer
	 */
	bool is_online;
	/**
	 * \brief Can use this \a changer
	 */
	bool enable;

	/**
	 * \brief Drives into this \a changer
	 */
	struct so_drive * drives;
	/**
	 * \brief Number of drives into this \a changer
	 */
	unsigned int nb_drives;

	/**
	 * \brief Slots into this \a changer
	 */
	struct so_slot * slots;
	/**
	 * \brief Number of slots into this \a changer
	 */
	unsigned int nb_slots;

	/**
	 * \brief method of changer
	 * \note definition of <em>struct so_changer_ops</em> is define into others files.
	 * And there can be define is multiple files. See libstoriq-one-*\/changer.h
	 */
	struct so_changer_ops * ops;

	/**
	 * \brief Private data
	 */
	void * data;
	/**
	 * \brief Private data used by database plugin
	 */
	struct so_value * db_data;
};

/**
 * \brief Convert an enumeration into static allocated string
 * \param[in] action : a value of enum so_changer_action
 * \param[in] translate : translate returned value
 * \return a static allocated string representing \a action
 */
const char * so_changer_action_to_string(enum so_changer_action action, bool translate);

/**
 * \brief Convert an instance of \a changer into an object
 * \param[in] changer : a changer
 * \return a value
 * \note To encode an archive into JSON, we need to call so_json_encode with
 * returned value
 * \see so_json_encode_to_fd
 * \see so_json_encode_to_file
 * \see so_json_encode_to_string
 */
struct so_value * so_changer_convert(struct so_changer * changer) __attribute__((warn_unused_result));

/**
 * \brief Release memory allocated for \a changer
 * \param[in] changer : a changer
 */
void so_changer_free(struct so_changer * changer);

/**
 * \brief Function to pass to so_value_new_custom
 * This function is a wrapper to so_changer_free used by so_value_free
 * \param[in] changer : a changer
 * \see so_changer_free
 * \see so_value_free
 * \see so_value_new_custom
 */
void so_changer_free2(void * changer);

/**
 * \brief Convert a status to string
 * \param[in] status : a status
 * \param[in] translate : translate result into current locale by using gettext
 * \return a statically allocated string representing \a status
 */
const char * so_changer_status_to_string(enum so_changer_status status, bool translate);

/**
 * \brief Convert an action to an enumeration
 * \param[in] action : action name
 * \param[in] translate : compare with translated value
 * \return an enumeration value of \a action
 */
enum so_changer_action so_changer_string_to_action(const char * action, bool translate);

/**
 * \brief Convert a status to an enumeration
 * \param[in] status : status name
 * \param[in] translate : compare with translated value
 * \return an enumeration value of \a status
 */
enum so_changer_status so_changer_string_to_status(const char * status, bool translate);

/**
 * \brief Synchronize data of \a changer with \a new_changer
 * \param[in,out] changer : update this \a changer
 * \param[in] new_changer : new data for \a changer
 */
void so_changer_sync(struct so_changer * changer, struct so_value * new_changer);

#endif

