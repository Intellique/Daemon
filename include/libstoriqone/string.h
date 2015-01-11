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
*  Copyright (C) 2015, Guillaume Clercin <gclercin@intellique.com>           *
\****************************************************************************/

#ifndef __LIBSTORIQONE_STRING_H__
#define __LIBSTORIQONE_STRING_H__

struct so_value;

// bool
#include <stdbool.h>
// size_t
#include <sys/types.h>

enum so_string_bidi_class {
	so_string_bidi_class_arabic_number,              // AN
	so_string_bidi_class_boundary_neutral,           // BM
	so_string_bidi_class_common_number_separator,    // CS
	so_string_bidi_class_european_number,            // EN
	so_string_bidi_class_european_number_separator,  // ES
	so_string_bidi_class_european_number_terminator, // ET
	so_string_bidi_class_first_strong_isolate,       // FSI
	so_string_bidi_class_left_to_right,              // L
	so_string_bidi_class_left_to_right_embedding,    // LRE
	so_string_bidi_class_left_to_right_isolate,      // LRI
	so_string_bidi_class_left_to_right_override,     // LRO
	so_string_bidi_class_non_spacing_mark,           // NSM
	so_string_bidi_class_other_neutrals,             // ON
	so_string_bidi_class_paragraph_separator,        // B
	so_string_bidi_class_pop_directional_format,     // PDF
	so_string_bidi_class_pop_directional_isolate,    // PDI
	so_string_bidi_class_right_to_left,              // R
	so_string_bidi_class_right_to_left_arabic,       // AL
	so_string_bidi_class_right_to_left_embedding,    // RLE
	so_string_bidi_class_right_to_left_isolate,      // RLI
	so_string_bidi_class_right_to_left_override,     // RLO
	so_string_bidi_class_segment_separator,          // S
	so_string_bidi_class_whitespace,                 // WS
};

enum so_string_character_category {
	so_string_character_category_letter,      // L
	so_string_character_category_mark,        // M
	so_string_character_category_number,      // N
	so_string_character_category_other,       // C
	so_string_character_category_punctuation, // P
	so_string_character_category_separator,   // Z
	so_string_character_category_symbol,      // S
};

enum so_string_character_subcategory {
	so_string_character_subcategory_letter_lowercase,          // Ll
	so_string_character_subcategory_letter_modifier,           // Lm
	so_string_character_subcategory_letter_other,              // Lo
	so_string_character_subcategory_letter_titlecase,          // Lt
	so_string_character_subcategory_letter_uppercase,          // Lu
	so_string_character_subcategory_mark_enclosing,            // Me
	so_string_character_subcategory_mark_nonspacing,           // Mn
	so_string_character_subcategory_mark_spacing_combining,    // Mc
	so_string_character_subcategory_number_decimal_digit,      // Nd
	so_string_character_subcategory_number_letter,             // Nl
	so_string_character_subcategory_number_other,              // No
	so_string_character_subcategory_other_control,             // Cc
	so_string_character_subcategory_other_format,              // Cf
	so_string_character_subcategory_other_not_assigned,        // Cn
	so_string_character_subcategory_other_private_use,         // Co
	so_string_character_subcategory_other_surrogate,           // Cs
	so_string_character_subcategory_punctuation_close,         // Pe
	so_string_character_subcategory_punctuation_connector,     // Pc
	so_string_character_subcategory_punctuation_dash,          // Pd
	so_string_character_subcategory_punctuation_finial_quote,  // Pf
	so_string_character_subcategory_punctuation_initial_quote, // Pi
	so_string_character_subcategory_punctuation_open,          // Ps
	so_string_character_subcategory_punctuation_other,         // Po
	so_string_character_subcategory_separator_line,            // Zl
	so_string_character_subcategory_separator_paragraph,       // Zp
	so_string_character_subcategory_separator_space,           // Zs
	so_string_character_subcategory_symbol_currency,           // Sc
	so_string_character_subcategory_symbol_math,               // Sm
	so_string_character_subcategory_symbol_modifier,           // Sk
	so_string_character_subcategory_symbol_other,              // So
};

struct so_string_character {
	char * name;
	char * unicode_name;
	enum so_string_character_category category:5;
	enum so_string_character_subcategory sub_category:5;
	enum so_string_bidi_class bidirectonal_class:5;
	bool mirrored:1;
	int offset:24;
} __attribute__((packed));

/**
 * \brief Check if \a string is a valid utf8 string
 *
 * \param[in] string : a utf8 string
 * \returns \b 1 if ok else 0
 */
bool so_string_check_valid_utf8(const char * string);

/**
 * \brief Compute hash of key
 *
 * \param[in] key : a c string
 * \returns computed hash
 *
 * \see so_hashtable_new
 */
unsigned long long so_string_compute_hash(const struct so_value * value);
unsigned long long so_string_compute_hash2(const char * value);
unsigned long long so_string_compute_hash3(const char * value);

bool so_string_convert_unicode_to_utf8(unsigned int unicode, char * string, size_t length, bool end_string);
unsigned int so_string_convert_utf8_to_unicode(const char * character);

/**
 * \brief Remove from \a str a sequence of two or more of character \a delete_char
 *
 * \param[in,out] str : a string
 * \param[in] delete_char : a character
 */
void so_string_delete_double_char(char * str, char delete_char);

const struct so_string_character * so_string_get_character_info(unsigned int unicode_character);

void so_string_middle_elipsis(char * string, size_t length);

/**
 * \brief Remove characters \a trim at the end of \a str
 *
 * \param[in,out] str : a string
 * \param[in] trim : a character
 *
 * \see so_util_string_trim
 */
void so_string_rtrim(char * str, char trim);

void so_string_to_lowercase(char * str);

void so_string_to_uppercase(char * str);

/**
 * \brief Remove characters \a trim at the beginning and at the end of \a str
 *
 * \param[in,out] str : a string
 * \param[in] trim : a character
 */
void so_string_trim(char * str, char trim);

size_t so_string_unicode_length(unsigned int unicode);

size_t so_string_utf8_length(const char * str);

#endif

