#!/usr/bin/perl

use strict;
use warnings;

$| = 1;

my %characterBidiClass = (
    AN  => 'so_string_bidi_class_arabic_number',
    BN  => 'so_string_bidi_class_boundary_neutral',
    CS  => 'so_string_bidi_class_common_number_separator',
    EN  => 'so_string_bidi_class_european_number',
    ES  => 'so_string_bidi_class_european_number_separator',
    ET  => 'so_string_bidi_class_european_number_terminator',
    FSI => 'so_string_bidi_class_first_strong_isolate',
    L   => 'so_string_bidi_class_left_to_right',
    LRE => 'so_string_bidi_class_left_to_right_embedding',
    LRI => 'so_string_bidi_class_left_to_right_isolate',
    LRO => 'so_string_bidi_class_left_to_right_override',
    NSM => 'so_string_bidi_class_non_spacing_mark',
    ON  => 'so_string_bidi_class_other_neutrals',
    B   => 'so_string_bidi_class_paragraph_separator',
    PDF => 'so_string_bidi_class_pop_directional_format',
    PDI => 'so_string_bidi_class_pop_directional_isolate',
    R   => 'so_string_bidi_class_right_to_left',
    AL  => 'so_string_bidi_class_right_to_left_arabic',
    RLE => 'so_string_bidi_class_right_to_left_embedding',
    RLI => 'so_string_bidi_class_right_to_left_isolate',
    RLO => 'so_string_bidi_class_right_to_left_override',
    S   => 'so_string_bidi_class_segment_separator',
    WS  => 'so_string_bidi_class_whitespace',
);
my %characterCategory = (
    L => 'so_string_character_category_letter',
    M => 'so_string_character_category_mark',
    N => 'so_string_character_category_number',
    C => 'so_string_character_category_other',
    P => 'so_string_character_category_punctuation',
    Z => 'so_string_character_category_separator',
    S => 'so_string_character_category_symbol',
);
my %characterSubCategory = (
    Ll => 'so_string_character_subcategory_letter_lowercase',
    Lm => 'so_string_character_subcategory_letter_modifier',
    Lo => 'so_string_character_subcategory_letter_other',
    Lt => 'so_string_character_subcategory_letter_titlecase',
    Lu => 'so_string_character_subcategory_letter_uppercase',
    Me => 'so_string_character_subcategory_mark_enclosing',
    Mn => 'so_string_character_subcategory_mark_nonspacing',
    Mc => 'so_string_character_subcategory_mark_spacing_combining',
    Nd => 'so_string_character_subcategory_number_decimal_digit',
    Nl => 'so_string_character_subcategory_number_letter',
    No => 'so_string_character_subcategory_number_other',
    Cc => 'so_string_character_subcategory_other_control',
    Cf => 'so_string_character_subcategory_other_format',
    Cn => 'so_string_character_subcategory_other_not_assigned',
    Co => 'so_string_character_subcategory_other_private_use',
    Cs => 'so_string_character_subcategory_other_surrogate',
    Pe => 'so_string_character_subcategory_punctuation_close',
    Pc => 'so_string_character_subcategory_punctuation_connector',
    Pd => 'so_string_character_subcategory_punctuation_dash',
    Pf => 'so_string_character_subcategory_punctuation_finial_quote',
    Pi => 'so_string_character_subcategory_punctuation_initial_quote',
    Ps => 'so_string_character_subcategory_punctuation_open',
    Po => 'so_string_character_subcategory_punctuation_other',
    Zl => 'so_string_character_subcategory_separator_line',
    Zp => 'so_string_character_subcategory_separator_paragraph',
    Zs => 'so_string_character_subcategory_separator_space',
    Sc => 'so_string_character_subcategory_symbol_currency',
    Sm => 'so_string_character_subcategory_symbol_math',
    Sk => 'so_string_character_subcategory_symbol_modifier',
    So => 'so_string_character_subcategory_symbol_other',
);

sub computeName {
    my $name = shift;
    if ($name) {
        return '"' . $name . '"';
    }
    else {
        return 'NULL';
    }
}

my $unicodeFilename = "util/unicode/UnicodeData.txt";

my $index     = 0;
my @lstBlocks = ();
open( my $fd_unifile, $unicodeFilename )
    or die "Can not open file [$unicodeFilename]";

while (<$fd_unifile>) {
    chomp;
    if (/^([^;]*);([^;]*);([^;]*);([^;]*);([^;]*);([^;]*);([^;]*);([^;]*);([^;]*);([^;]*);([^;]*);([^;]*);([^;]*);([^;]*);(.*)/
        )
    {
        push @lstBlocks,
            {
            unicode               => int hex $1,
            name                  => computeName($2),
            generalCategory       => $3,
            combilingClass        => $4,
            bidirectionalCategory => $5,
            decomposition         => $6,
            decimalDigitValue     => $7,
            digitValue            => $8,
            numericValue          => $9,
            mirrored              => $10,
            unicode10Name         => computeName($11),
            comment               => $12,
            uppercaseMapping      => int hex $13,
            lowercaseMapping      => int hex $14,
            titlecaseMapping      => int hex $15,
            };
    }
    $index++;
}
close $fd_unifile;

my $outputfile = "build/libstoriqone/unicode.h";

open my $fd_out, '>', $outputfile;

###
# level 3 tabs
###
my $current_index = -1;
my $last_unicode  = -1;
my @tabs          = ();

foreach my $character (@lstBlocks) {
    my $index = int( $character->{'unicode'} / 32 );
    if ( $current_index != $index ) {
        if ( $index > 0 ) {
            my $last_index = 32 * ( $current_index + 1 );
            while ( $last_unicode < $last_index ) {
                printf {$fd_out}
                    "\t{NULL, NULL, so_string_character_category_other, so_string_character_subcategory_other_not_assigned, so_string_bidi_class_other_neutrals, false, 0}, // %d 0x%x\n",
                    $last_unicode, $last_unicode;
                $last_unicode++;
            }
            print {$fd_out} "};\n\n";
        }

        $current_index = $index;
        $last_unicode  = 32 * $index;

        printf {$fd_out}
            "static const struct so_string_character so_string_characters_l2_%04x[] = {\n",
            $index;

        push @tabs, $index;
    }

    while ( $last_unicode < $character->{'unicode'} ) {
        printf {$fd_out}
            "\t{NULL, NULL, so_string_character_category_other, so_string_character_subcategory_other_not_assigned, so_string_bidi_class_other_neutrals, false, 0}, // %d 0x%x\n",
            $last_unicode, $last_unicode;
        $last_unicode++;
    }
    $last_unicode = $character->{'unicode'} + 1;

    my $generalCategory = $character->{'generalCategory'};
    my $mirrored        = undef;
    if ( $character->{'mirrored'} =~ m/Y/ ) {
        $mirrored = "true";
    }
    else {
        $mirrored = "false";
    }
    my $offset = 0;
    if ( $character->{'uppercaseMapping'} ) {
        $offset = $character->{'uppercaseMapping'} - $character->{'unicode'};
    }
    elsif ( $character->{'lowercaseMapping'} ) {
        $offset = $character->{'lowercaseMapping'} - $character->{'unicode'};
    }

    printf {$fd_out} "\t{%s, %s, %s, %s, %s, %s, %d}, // %d 0x%x\n",
        $character->{'name'}, $character->{'unicode10Name'},
        $characterCategory{ substr( $generalCategory, 0, 1 ) },
        $characterSubCategory{$generalCategory},
        $characterBidiClass{ $character->{'bidirectionalCategory'} },
        $mirrored, $offset, $character->{'unicode'}, $character->{'unicode'};
}

my $last_index = 32 * ( $current_index + 1 );
while ( $last_unicode < $last_index ) {
    printf {$fd_out}
        "\t{NULL, NULL, so_string_character_category_other, so_string_character_subcategory_other_not_assigned, so_string_bidi_class_other_neutrals, false, 0}, // %d\n",
        $last_unicode;
    $last_unicode++;
}
print {$fd_out} "};\n\n";

###
# level 2 tabs
###
$current_index = -1;
my $previous = 0;
my @tabs2    = ();

foreach my $tab (@tabs) {
    my $index = int( $tab / 32 );
    if ( $current_index != $index ) {
        if ( $index > 0 ) {
            my $last_index = 32 * ( $current_index + 1 );
            while ( $previous < $last_index ) {
				printf {$fd_out} "\tNULL,\n";
                $previous++;
            }
            print {$fd_out} "};\n\n";
        }

        $current_index = $index;
        $previous      = 32 * $index;

        printf {$fd_out}
            "static const struct so_string_character * so_string_characters_l1_%03x[] = {\n",
            $index;

        push @tabs2, $index;
    }

    while ( $previous < $tab ) {
		printf {$fd_out} "\tNULL,\n";
        $previous++;
    }

    printf {$fd_out} "\tso_string_characters_l2_%04x,\n", $tab;
    $previous++;
}

$last_index = 32 * ( $current_index + 1 );
while ( $previous < $last_index ) {
	printf {$fd_out} "\tNULL,\n";
    $previous++;
}
print {$fd_out} "};\n\n";

###
# level 1 tabs
###
$current_index = -1;
$previous      = 0;
my @tabs3 = ();

foreach my $tab (@tabs2) {
    my $index = int( $tab / 32 );
    if ( $current_index != $index ) {
        if ( $index > 0 ) {
            my $last_index = 32 * ( $current_index + 1 );
            while ( $previous < $last_index ) {
				printf {$fd_out} "\tNULL,\n";
                $previous++;
            }
            print {$fd_out} "};\n\n";
        }

        $current_index = $index;
        $previous      = 32 * $index;

        printf {$fd_out}
            "static const struct so_string_character ** so_string_characters_l0_%02x[] = {\n",
            $index;

        push @tabs3, $index;
    }

    while ( $previous < $tab ) {
		printf {$fd_out} "\tNULL,\n";
        $previous++;
    }

    printf {$fd_out} "\tso_string_characters_l1_%03x,\n", $tab;
    $previous++;
}

$last_index = 32 * ( $current_index + 1 );
while ( $previous < $last_index ) {
	printf {$fd_out} "\tNULL,\n";
    $previous++;
}
print {$fd_out} "};\n\n";

###
# Global tabs
###
$previous = 0;

printf {$fd_out}
    "static const struct so_string_character *** so_string_characters[] = {\n";
foreach my $tab (@tabs3) {
    while ( $previous < $tab ) {
        printf {$fd_out} "\tNULL,\n";
        $previous++;
    }

    printf {$fd_out} "\tso_string_characters_l0_%02x,\n", $tab;
    $previous++;
}
print {$fd_out} "};\n\n";

close $fd_out;

