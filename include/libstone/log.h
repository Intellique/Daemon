/*************************************************************************\
*                            __________                                   *
*                           / __/_  __/__  ___  ___                       *
*                          _\ \  / / / _ \/ _ \/ -_)                      *
*                         /___/ /_/  \___/_//_/\__/                       *
*                                                                         *
*  ---------------------------------------------------------------------  *
*  This file is a part of STone                                           *
*                                                                         *
*  STone is free software; you can redistribute it and/or                 *
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
*  Copyright (C) 2013, Clercin guillaume <gclercin@intellique.com>        *
*  Last modified: Tue, 10 Jul 2012 13:36:56 +0200                         *
\*************************************************************************/

#ifndef __STONE_LOG_H__
#define __STONE_LOG_H__

// time_t
#include <sys/time.h>

// forward declarations
struct st_hashtable;
struct st_log_module;
struct st_user;


/**
 * \enum st_log_level
 * \brief Enumerate level of message
 *
 * Each level has a priority (i.e. debug > info > warning > error)
 */
enum st_log_level {
	/**
	 * \brief Reserved for debugging
	 */
	st_log_level_debug = 0x3,

	/**
	 * \brief Should be used to alert errors
	 */
	st_log_level_error = 0x0,

	/**
	 * \brief Should be used to inform what the server does.
	 *
	 * Example: server starts a new job
	 */
	st_log_level_info = 0x2,

	/**
	 * \brief Should be used to alert errors which can be recovered
	 *
	 * Example: dns service not available
	 */
	st_log_level_warning = 0x1,

	/**
	 * \brief Should not be used
	 *
	 * Used only by st_log_string_to_level to report an error
	 * \see st_log_string_to_level
	 */
	st_log_level_unknown = 0xF,
};

/**
 * \enum st_log_type
 * \brief Enumerate type of message
 */
enum st_log_type {
	/**
	 * \brief used by changer
	 */
	st_log_type_changer,
	/**
	 * \brief used by checksum
	 */
	st_log_type_checksum,
	/**
	 * \brief generic message from daemon
	 */
	st_log_type_daemon,
	/**
	 * \brief used by database
	 */
	st_log_type_database,
	/**
	 * \brief used by tape drive
	 */
	st_log_type_drive,
	st_log_type_job,
	/**
	 * \brief used by checksum module
	 */
	st_log_type_plugin_checksum,
	/**
	 * \brief used by database module
	 */
	st_log_type_plugin_db,
	/**
	 * \brief used by log module
	 */
	st_log_type_plugin_log,
	/**
	 * \brief used by scheduler
	 */
	st_log_type_scheduler,
	/**
	 * \brief used by user interface
	 */
	st_log_type_ui,
	st_log_type_user_message,

	/**
	 * \brief Should not be used
	 *
	 * Used only by st_log_string_to_type to report an error
	 * \see st_log_string_to_type
	 */
	st_log_type_unknown,
};


/**
 * \struct st_log_message
 * \brief This structure represents a message send to a log module
 *
 * Only usefull for log module
 */
struct st_log_message {
	/**
	 * \brief Level of message
	 */
	enum st_log_level level;
	/**
	 * \brief Kind of message
	 */
	enum st_log_type type;
	/**
	 * \brief Message
	 */
	char * message;
	/**
	 * \brief User associated of this message
	 *
	 * \note can be null
	 */
	struct st_user * user;
	/**
	 * \brief timestamp of message
	 *
	 * \note Number of seconds since \b Epoch (i.e. Thu, 01 Jan 1970 00:00:00 +0000)
	 */
	time_t timestamp;
};

/**
 * \struct st_log_driver
 * \brief Structure used used only one time by each log module
 *
 * \note This structure should be staticaly allocated and passed to function st_log_register_driver
 * \see st_log_register_driver
 */
struct st_log_driver {
	/**
	 * \brief Name of driver
	 *
	 * \note Should be unique and equals to liblog-name.so where name is the name of driver.
	 */
	const char * name;
	/**
	 * \brief Add a module to this driver
	 *
	 * \param[in] alias : name of module
	 * \param[in] level : level of message
	 * \param[in] params : addictional parameters
	 * \returns 0 if \b ok
	 *
	 * \pre
	 * \li alias is not null and should be duplicated
	 * \li level is not equal to st_log_level_unknown
	 * \li params is not null
	 */
	int (*add)(const char * alias, enum st_log_level level, const struct st_hashtable * params);

	/**
	 * \brief cookie
	 *
	 * Is a value returns by dlopen and should not be used nor released
	 */
	void * cookie;
	/**
	 * \brief api level supported by this driver
	 *
	 * Should be define by using STONE_LOG_API_LEVEL only
	 */
	const unsigned int api_level;

	/**
	 * \struct st_log_module
	 * \brief A log module
	 */
	struct st_log_module {
		/**
		 * \brief name of module
		 */
		char * alias;
		/**
		 * \brief Minimal level
		 */
		enum st_log_level level;
		/**
		 * \struct st_log_module_ops
		 * \brief Functions associated to a log module
		 */
		struct st_log_module_ops {
			/**
			 * \brief release a module
			 *
			 * \param[in] module : release this module
			 */
			void (*free)(struct st_log_module * module);
			/**
			 * \brief Write a message to a module
			 *
			 * \param[in] module : write to this module
			 * \param[in] message : write this message
			 */
			void (*write)(struct st_log_module * module, const struct st_log_message * message);
		} * ops;

		/**
		 * \brief Private data of a log module
		 */
		void * data;
	} * modules;
	/**
	 * \brief Numbers of modules associated to this driver
	 */
	unsigned int nb_modules;
};

/**
 * \def STONE_LOG_API_LEVEL
 * \brief Current api level
 *
 * Will increment with new version of struct st_log_driver or struct st_log
 */
#define STONE_LOG_API_LEVEL 1


/**
 * \brief Disable display remains messages.
 *
 * Default behaviour is to display all messages into terminal
 * if no log modules are loaded at exit of process.
 */
void st_log_disable_display_log(void);

/**
 * \brief Get a log driver
 *
 * \param module : driver's name
 * \return 0 if failed
 * \note if the driver is not loaded, st_log_get_driver will try to load it
 *
 * \pre module should not be null
 */
struct st_log_driver * st_log_get_driver(const char * module);

/**
 * \brief Each log driver should call this function only one time
 *
 * \param driver : a static allocated structure
 * \code
 * \_\_attribute\_\_((constructor))
 * static void log_myLog_init() {
 *    log_register_driver(&log_myLog_module);
 * }
 * \endcode
 */
void st_log_register_driver(struct st_log_driver * driver);

/**
 * \brief Convert an enumeration to a statically allocated string
 *
 * \param level : one log level
 * \return a statically allocated string
 * \note returned value should not be released
 */
const char * st_log_level_to_string(enum st_log_level level);

/**
 * \brief Start thread which write messages to log modules
 *
 * \note Should be used only one time
 * \note This function is thread-safe
 */
void st_log_start_logger(void);

void st_log_stop_logger();

/**
 * \brief Convert a c string to an enumeration
 *
 * \param string : one string level
 * \return an enumeration
 */
enum st_log_level st_log_string_to_level(const char * string);

/**
 * \brief Convert a c string to an enum st_log_type
 *
 * \param string : one string type
 * \return an enumeration
 */
enum st_log_type st_log_string_to_type(const char * string);

/**
 * \brief Convert an enum st_log_type to a statically allocated c string
 *
 * \param[in] type : a log type
 * \return a statically allocated c string
 *
 * \note st_log_type_to_string never returns NULL and you <b>SHOULD NOT RELEASE</b> returned value
 */
const char * st_log_type_to_string(enum st_log_type type);

/**
 * \brief Write a message to all log modules
 *
 * \param[in] level : level of message
 * \param[in] type : type of message
 * \param[in] format : message with printf-like syntax
 *
 * \note Message can be wrote after that this function has returned.
 * \note This function is thread-safe
 */
void st_log_write_all(enum st_log_level level, enum st_log_type type, const char * format, ...) __attribute__ ((format (printf, 3, 4)));

/**
 * \brief Write a message to all log modules
 *
 * \param[in] level : level of message
 * \param[in] type : type of message
 * \param[in] user : user associted to this message
 * \param[in] format : message with printf-like syntax
 *
 * \note Message can be wrote after that this function has returned.
 * \note This function is thread-safe
 */
void st_log_write_all2(enum st_log_level level, enum st_log_type type, struct st_user * user, const char * format, ...) __attribute__ ((format (printf, 4, 5)));

//void st_log_write_to(const char * alias, enum st_log_type type, enum st_log_level level, const char * format, ...) __attribute__ ((format (printf, 4, 5)));

#endif

