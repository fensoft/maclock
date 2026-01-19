/*******************************************************************************
 * Size: 8 px
 * Bpp: 1
 * Opts: --font data/pixChicago.ttf --size 8 --bpp 1 --format lvgl --output src/lv_font_chicago_8.c --range 0x20-0xFF
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef LV_FONT_CHICAGO_8
#define LV_FONT_CHICAGO_8 1
#endif

#if LV_FONT_CHICAGO_8

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xff, 0xf3, 0xc0,

    /* U+0022 "\"" */
    0xb6, 0x80,

    /* U+0023 "#" */
    0x12, 0x24, 0xb1, 0x22, 0x44, 0xbf, 0xa4,

    /* U+0024 "$" */
    0x23, 0xab, 0x4e, 0x38, 0xe5, 0xab, 0x88,

    /* U+0025 "%" */
    0x6e, 0x49, 0x25, 0xd, 0x80, 0x80, 0x98, 0x52,
    0x49, 0x23, 0x0,

    /* U+0026 "&" */
    0x7c, 0xcc, 0xc1, 0x61, 0x61, 0xce, 0xcc, 0xcc,
    0x78,

    /* U+0027 "'" */
    0xe0,

    /* U+0028 "(" */
    0x7b, 0x6d, 0xb6, 0xd9, 0x80,

    /* U+0029 ")" */
    0xcd, 0xb6, 0xdb, 0x6f, 0x0,

    /* U+002A "*" */
    0x25, 0x5c, 0xea, 0x90,

    /* U+002B "+" */
    0x10, 0x23, 0xf8, 0x81, 0x0,

    /* U+002C "," */
    0xf6,

    /* U+002D "-" */
    0xf8,

    /* U+002E "." */
    0xf0,

    /* U+002F "/" */
    0x8, 0x84, 0x22, 0x11, 0x8, 0x44, 0x0,

    /* U+0030 "0" */
    0x7b, 0x3d, 0xf7, 0xff, 0xbc, 0xf3, 0x78,

    /* U+0031 "1" */
    0x7f, 0xff, 0xc0,

    /* U+0032 "2" */
    0x7a, 0x30, 0xc3, 0x18, 0xc6, 0x30, 0xfc,

    /* U+0033 "3" */
    0xfc, 0x63, 0x1e, 0xc, 0x30, 0xe3, 0x78,

    /* U+0034 "4" */
    0xc, 0x38, 0xb2, 0x6c, 0xdf, 0xc3, 0x6, 0xc,

    /* U+0035 "5" */
    0xff, 0xc, 0x3e, 0xc, 0x30, 0xe3, 0x78,

    /* U+0036 "6" */
    0x39, 0x8c, 0x3e, 0xcf, 0x3c, 0xf3, 0x78,

    /* U+0037 "7" */
    0xfe, 0xc, 0x18, 0x60, 0xc3, 0x6, 0xc, 0x18,

    /* U+0038 "8" */
    0x7b, 0x3c, 0xf3, 0x33, 0x3c, 0xf3, 0x78,

    /* U+0039 "9" */
    0x7b, 0x3c, 0xf3, 0xcd, 0xf0, 0xc6, 0x70,

    /* U+003A ":" */
    0xf0, 0x3c,

    /* U+003B ";" */
    0xf0, 0x3d, 0x80,

    /* U+003C "<" */
    0x19, 0x99, 0x86, 0x18, 0x60,

    /* U+003D "=" */
    0xfc, 0xf, 0xc0,

    /* U+003E ">" */
    0x82, 0x8, 0x32, 0x22, 0x0,

    /* U+003F "?" */
    0x7a, 0x30, 0xc6, 0x30, 0xc0, 0xc, 0x30,

    /* U+0040 "@" */
    0x3e, 0x20, 0xa7, 0x34, 0x9a, 0x4d, 0x26, 0x7e,
    0x80, 0x3e, 0x0,

    /* U+0041 "A" */
    0x7b, 0x3c, 0xf3, 0xff, 0x3c, 0xf3, 0xcc,

    /* U+0042 "B" */
    0xfb, 0x3c, 0xf3, 0xf3, 0x3c, 0xf3, 0xf8,

    /* U+0043 "C" */
    0x7b, 0x1c, 0x30, 0xc3, 0xc, 0x31, 0x78,

    /* U+0044 "D" */
    0xfb, 0x3c, 0xf3, 0xcf, 0x3c, 0xf3, 0xf8,

    /* U+0045 "E" */
    0xfe, 0x31, 0x8f, 0x63, 0x18, 0xf8,

    /* U+0046 "F" */
    0xfe, 0x31, 0x8f, 0x63, 0x18, 0xc0,

    /* U+0047 "G" */
    0x7b, 0x1c, 0x30, 0xcf, 0x3c, 0xf3, 0x78,

    /* U+0048 "H" */
    0xcf, 0x3c, 0xf3, 0xff, 0x3c, 0xf3, 0xcc,

    /* U+0049 "I" */
    0xff, 0xff, 0xc0,

    /* U+004A "J" */
    0xc, 0x30, 0xc3, 0xf, 0x3c, 0xf3, 0x78,

    /* U+004B "K" */
    0xc7, 0x2d, 0x38, 0xe3, 0x8d, 0x32, 0xc4,

    /* U+004C "L" */
    0xc6, 0x31, 0x8c, 0x63, 0x18, 0xf8,

    /* U+004D "M" */
    0x80, 0xf0, 0x7e, 0x3f, 0xdf, 0xbe, 0xe7, 0x38,
    0x8e, 0x3, 0x80, 0xc0,

    /* U+004E "N" */
    0x83, 0x87, 0x8f, 0x9b, 0xb3, 0xe3, 0xc3, 0x82,

    /* U+004F "O" */
    0x7b, 0x3c, 0xf3, 0xcf, 0x3c, 0xf3, 0x78,

    /* U+0050 "P" */
    0xfb, 0x3c, 0xf3, 0xfb, 0xc, 0x30, 0xc0,

    /* U+0051 "Q" */
    0x7b, 0x3c, 0xf3, 0xcf, 0x3c, 0xf3, 0x78, 0x30,

    /* U+0052 "R" */
    0xfb, 0x3c, 0xf3, 0xfb, 0x3c, 0xf3, 0xcc,

    /* U+0053 "S" */
    0x76, 0x71, 0xc7, 0x1c, 0x73, 0x70,

    /* U+0054 "T" */
    0xff, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
    0x18,

    /* U+0055 "U" */
    0xcf, 0x3c, 0xf3, 0xcf, 0x3c, 0xf3, 0x78,

    /* U+0056 "V" */
    0xcf, 0x3c, 0xf3, 0xcf, 0x3c, 0xf2, 0xf0,

    /* U+0057 "W" */
    0xcc, 0xf3, 0x3c, 0xcf, 0x33, 0xcc, 0xf3, 0x3c,
    0xcf, 0x32, 0xff, 0x0,

    /* U+0058 "X" */
    0xcf, 0x3c, 0xf3, 0x33, 0x3c, 0xf3, 0xcc,

    /* U+0059 "Y" */
    0xcf, 0x3c, 0xf3, 0x78, 0xc3, 0xc, 0x30,

    /* U+005A "Z" */
    0xfc, 0x71, 0x8e, 0x31, 0x86, 0x30, 0xfc,

    /* U+005B "[" */
    0xff, 0xff, 0xfc,

    /* U+005C "\\" */
    0x82, 0x10, 0x82, 0x10, 0x42, 0x10, 0x40,

    /* U+005D "]" */
    0xff, 0xff, 0xfc,

    /* U+005E "^" */
    0x31, 0xec, 0xc0,

    /* U+005F "_" */
    0xff,

    /* U+0060 "`" */
    0x90,

    /* U+0061 "a" */
    0x7a, 0x30, 0xdf, 0xcf, 0x37, 0xc0,

    /* U+0062 "b" */
    0xc3, 0xf, 0xb3, 0xcf, 0x3c, 0xf3, 0xf8,

    /* U+0063 "c" */
    0x76, 0x71, 0x8c, 0x65, 0xc0,

    /* U+0064 "d" */
    0xc, 0x37, 0xf3, 0xcf, 0x3c, 0xf3, 0x7c,

    /* U+0065 "e" */
    0x7b, 0x3c, 0xff, 0xc3, 0x17, 0x80,

    /* U+0066 "f" */
    0x3b, 0x3c, 0xc6, 0x31, 0x8c, 0x60,

    /* U+0067 "g" */
    0x7f, 0x3c, 0xf3, 0xcf, 0x37, 0xc3, 0x8d, 0xe0,

    /* U+0068 "h" */
    0xc3, 0xf, 0xb3, 0xcf, 0x3c, 0xf3, 0xcc,

    /* U+0069 "i" */
    0xcf, 0xff, 0xc0,

    /* U+006A "j" */
    0x18, 0x6, 0x31, 0x8c, 0x63, 0x1c, 0xdc,

    /* U+006B "k" */
    0xc3, 0xc, 0xf6, 0xf3, 0xf, 0x36, 0xcc,

    /* U+006C "l" */
    0xff, 0xff, 0xc0,

    /* U+006D "m" */
    0xfb, 0xb3, 0x3c, 0xcf, 0x33, 0xcc, 0xf3, 0x3c,
    0xcc,

    /* U+006E "n" */
    0xfb, 0x3c, 0xf3, 0xcf, 0x3c, 0xc0,

    /* U+006F "o" */
    0x7b, 0x3c, 0xf3, 0xcf, 0x37, 0x80,

    /* U+0070 "p" */
    0xfb, 0x3c, 0xf3, 0xcf, 0x3f, 0xb0, 0xc0,

    /* U+0071 "q" */
    0x7f, 0x3c, 0xf3, 0xcf, 0x37, 0xc3, 0xc,

    /* U+0072 "r" */
    0xdf, 0xf1, 0x8c, 0x63, 0x0,

    /* U+0073 "s" */
    0x76, 0x78, 0xe3, 0xcd, 0xc0,

    /* U+0074 "t" */
    0x66, 0xf6, 0x66, 0x66, 0x30,

    /* U+0075 "u" */
    0xcf, 0x3c, 0xf3, 0xcf, 0x37, 0xc0,

    /* U+0076 "v" */
    0xcf, 0x3c, 0xf3, 0xcf, 0x2f, 0x0,

    /* U+0077 "w" */
    0xcc, 0xf3, 0x3c, 0xcf, 0x33, 0xcc, 0xf3, 0x2f,
    0xf0,

    /* U+0078 "x" */
    0xcf, 0x3c, 0xcc, 0xcf, 0x3c, 0xc0,

    /* U+0079 "y" */
    0xcf, 0x3c, 0xf3, 0xcf, 0x37, 0xc3, 0x8d, 0xe0,

    /* U+007A "z" */
    0xfc, 0x21, 0x8, 0x43, 0xf, 0xc0,

    /* U+007B "{" */
    0x29, 0x25, 0x12, 0x48, 0x80,

    /* U+007C "|" */
    0xff, 0xe0,

    /* U+007D "}" */
    0x89, 0x24, 0x52, 0x4a, 0x0,

    /* U+007E "~" */
    0x66, 0x60,

    /* U+007F "" */
    0xfc, 0x63, 0x18, 0xc6, 0x31, 0x8c, 0x63, 0xf0,

    /* U+0080 "" */
    0xfc, 0x63, 0x18, 0xc6, 0x31, 0x8c, 0x63, 0xf0,

    /* U+0081 "" */
    0xfc, 0x63, 0x18, 0xc6, 0x31, 0x8c, 0x63, 0xf0,

    /* U+008D "" */
    0xfc, 0x63, 0x18, 0xc6, 0x31, 0x8c, 0x63, 0xf0,

    /* U+008E "" */
    0x78, 0xc0, 0x3f, 0x1c, 0x63, 0x8c, 0x61, 0x8c,
    0x3f,

    /* U+008F "" */
    0xfc, 0x63, 0x18, 0xc6, 0x31, 0x8c, 0x63, 0xf0,

    /* U+0090 "" */
    0xfc, 0x63, 0x18, 0xc6, 0x31, 0x8c, 0x63, 0xf0,

    /* U+009D "" */
    0xfc, 0x63, 0x18, 0xc6, 0x31, 0x8c, 0x63, 0xf0,

    /* U+009E "" */
    0x78, 0xc0, 0x3f, 0x8, 0x42, 0x10, 0xc3, 0xf0,

    /* U+00A0 " " */
    0x0,

    /* U+00A1 "¡" */
    0xf3, 0xff, 0xc0,

    /* U+00A2 "¢" */
    0x27, 0xba, 0xab, 0x72,

    /* U+00A3 "£" */
    0x38, 0xc9, 0x83, 0xf, 0xc, 0x18, 0x31, 0xfc,

    /* U+00A4 "¤" */
    0x85, 0xe4, 0x92, 0x7a, 0x10,

    /* U+00A5 "¥" */
    0xcf, 0x3c, 0xde, 0xfc, 0xcf, 0xcc, 0x30,

    /* U+00A6 "¦" */
    0xfb, 0xe0,

    /* U+00A7 "§" */
    0x74, 0x60, 0x8a, 0x49, 0x25, 0x10, 0x62, 0xe0,

    /* U+00A8 "¨" */
    0x90,

    /* U+00A9 "©" */
    0x7e, 0xc3, 0x99, 0xa5, 0xa1, 0xa5, 0x99, 0xc3,
    0x7e,

    /* U+00AA "ª" */
    0x74, 0xdf, 0xbd, 0xbc, 0x1f,

    /* U+00AB "«" */
    0x12, 0x49, 0x24, 0x84, 0x84, 0x84, 0x80,

    /* U+00AC "¬" */
    0xf8, 0x42,

    /* U+00AD "­" */
    0xfc,

    /* U+00AE "®" */
    0x7e, 0xc3, 0xbd, 0xa5, 0xb9, 0xa5, 0xa5, 0xc3,
    0x7e,

    /* U+00AF "¯" */
    0xf0,

    /* U+00B0 "°" */
    0x69, 0x96,

    /* U+00B1 "±" */
    0x10, 0x23, 0xf8, 0x81, 0x0, 0x3f, 0x80,

    /* U+00B2 "²" */
    0x74, 0xc6, 0x66, 0x63, 0xe0,

    /* U+00B3 "³" */
    0xf8, 0xc8, 0xf1, 0xcf, 0xc0,

    /* U+00B4 "´" */
    0x60,

    /* U+00B5 "µ" */
    0x33, 0x76, 0x66, 0x66, 0x66, 0x66, 0xfb, 0xc0,
    0xc0,

    /* U+00B6 "¶" */
    0x7f, 0xc5, 0xc5, 0xc5, 0x7d, 0x5, 0x5, 0x5,
    0x5,

    /* U+00B7 "·" */
    0xf0,

    /* U+00B8 "¸" */
    0xdc,

    /* U+00B9 "¹" */
    0x7f, 0xfc,

    /* U+00BA "º" */
    0x76, 0xf7, 0xbd, 0xb8, 0x1f,

    /* U+00BB "»" */
    0x91, 0xb1, 0xb1, 0xb6, 0xdb, 0x24, 0x0,

    /* U+00BC "¼" */
    0x44, 0x19, 0x83, 0x27, 0x64, 0xed, 0xad, 0xad,
    0xb5, 0xf9, 0x86, 0x20, 0xc0,

    /* U+00BD "½" */
    0x44, 0x19, 0x83, 0x27, 0x65, 0x3d, 0x87, 0xa1,
    0xb4, 0x61, 0x98, 0x23, 0xe0,

    /* U+00BE "¾" */
    0xf8, 0x80, 0x66, 0x2, 0x13, 0x9e, 0x4e, 0x1b,
    0x5a, 0x6b, 0x6f, 0x2f, 0xc1, 0x86, 0x4, 0x18,

    /* U+00BF "¿" */
    0x30, 0xc0, 0xc, 0x31, 0x8c, 0x31, 0x78,

    /* U+00C0 "À" */
    0x20, 0x40, 0x1e, 0xcf, 0x3c, 0xff, 0xcf, 0x3c,
    0xf3,

    /* U+00C1 "Á" */
    0x10, 0x80, 0x1e, 0xcf, 0x3c, 0xff, 0xcf, 0x3c,
    0xf3,

    /* U+00C2 "Â" */
    0x31, 0xe0, 0x1e, 0xcf, 0x3c, 0xff, 0xcf, 0x3c,
    0xf3,

    /* U+00C3 "Ã" */
    0x66, 0x60, 0x1e, 0xcf, 0x3c, 0xff, 0xcf, 0x3c,
    0xf3,

    /* U+00C4 "Ä" */
    0x48, 0x7, 0xb3, 0xcf, 0x3f, 0xf3, 0xcf, 0x3c,
    0xc0,

    /* U+00C5 "Å" */
    0x31, 0x24, 0x9e, 0xcf, 0x3c, 0xff, 0xcf, 0x3c,
    0xf3,

    /* U+00C6 "Æ" */
    0x7f, 0xe6, 0x33, 0x19, 0x8f, 0xf6, 0x63, 0x31,
    0x98, 0xcf, 0x80,

    /* U+00C7 "Ç" */
    0x7b, 0x1c, 0x30, 0xc3, 0xc, 0x31, 0x78, 0xe0,
    0x86,

    /* U+00C8 "È" */
    0x41, 0x1, 0xfc, 0x63, 0x1e, 0xc6, 0x31, 0xf0,

    /* U+00C9 "É" */
    0x11, 0x1, 0xfc, 0x63, 0x1e, 0xc6, 0x31, 0xf0,

    /* U+00CA "Ê" */
    0x67, 0x81, 0xfc, 0x63, 0x1e, 0xc6, 0x31, 0xf0,

    /* U+00CB "Ë" */
    0x90, 0x3f, 0x8c, 0x63, 0xd8, 0xc6, 0x3e,

    /* U+00CC "Ì" */
    0x93, 0xff, 0xff,

    /* U+00CD "Í" */
    0x63, 0xff, 0xff,

    /* U+00CE "Î" */
    0x6f, 0x6, 0x66, 0x66, 0x66, 0x66,

    /* U+00CF "Ï" */
    0x90, 0x66, 0x66, 0x66, 0x66, 0x60,

    /* U+00D0 "Ð" */
    0x7c, 0xcd, 0x9b, 0x3f, 0x6c, 0xd9, 0xb3, 0x7c,

    /* U+00D1 "Ñ" */
    0x32, 0x98, 0x4, 0x1c, 0x3c, 0x7c, 0xdd, 0x9f,
    0x1e, 0x1c, 0x10,

    /* U+00D2 "Ò" */
    0x20, 0x40, 0x1e, 0xcf, 0x3c, 0xf3, 0xcf, 0x3c,
    0xde,

    /* U+00D3 "Ó" */
    0x10, 0x80, 0x1e, 0xcf, 0x3c, 0xf3, 0xcf, 0x3c,
    0xde,

    /* U+00D4 "Ô" */
    0x31, 0xe0, 0x1e, 0xcf, 0x3c, 0xf3, 0xcf, 0x3c,
    0xde,

    /* U+00D5 "Õ" */
    0x66, 0x60, 0x1e, 0xcf, 0x3c, 0xf3, 0xcf, 0x3c,
    0xde,

    /* U+00D6 "Ö" */
    0x48, 0x7, 0xb3, 0xcf, 0x3c, 0xf3, 0xcf, 0x37,
    0x80,

    /* U+00D7 "×" */
    0x87, 0x27, 0xc, 0x6b, 0x10,

    /* U+00D8 "Ø" */
    0x7f, 0x3d, 0xf7, 0xff, 0xbc, 0xf3, 0xf8,

    /* U+00D9 "Ù" */
    0x20, 0x40, 0x33, 0xcf, 0x3c, 0xf3, 0xcf, 0x3c,
    0xde,

    /* U+00DA "Ú" */
    0x10, 0x80, 0x33, 0xcf, 0x3c, 0xf3, 0xcf, 0x3c,
    0xde,

    /* U+00DB "Û" */
    0x31, 0xe0, 0x33, 0xcf, 0x3c, 0xf3, 0xcf, 0x3c,
    0xde,

    /* U+00DC "Ü" */
    0x48, 0xc, 0xf3, 0xcf, 0x3c, 0xf3, 0xcf, 0x37,
    0x80,

    /* U+00DD "Ý" */
    0x10, 0x80, 0x33, 0xcf, 0x3c, 0xde, 0x30, 0xc3,
    0xc,

    /* U+00DE "Þ" */
    0xc3, 0xf, 0xb3, 0xcf, 0x3f, 0xb0, 0xc0,

    /* U+00DF "ß" */
    0x7b, 0x3c, 0xf6, 0xdb, 0x3c, 0xf3, 0xd8,

    /* U+00E0 "à" */
    0x20, 0x40, 0x1e, 0x8c, 0x37, 0xf3, 0xcd, 0xf0,

    /* U+00E1 "á" */
    0x10, 0x80, 0x1e, 0x8c, 0x37, 0xf3, 0xcd, 0xf0,

    /* U+00E2 "â" */
    0x31, 0xe0, 0x1e, 0x8c, 0x37, 0xf3, 0xcd, 0xf0,

    /* U+00E3 "ã" */
    0x66, 0x60, 0x1e, 0x8c, 0x37, 0xf3, 0xcd, 0xf0,

    /* U+00E4 "ä" */
    0x48, 0x7, 0xa3, 0xd, 0xfc, 0xf3, 0x7c,

    /* U+00E5 "å" */
    0x31, 0x24, 0x8c, 0x1, 0xe8, 0xc3, 0x7f, 0x3c,
    0xdf,

    /* U+00E6 "æ" */
    0x7f, 0xa3, 0x30, 0xcd, 0xff, 0xcc, 0x33, 0x17,
    0xf8,

    /* U+00E7 "ç" */
    0x76, 0x71, 0x8c, 0x65, 0xc6, 0x11, 0x80,

    /* U+00E8 "è" */
    0x20, 0x40, 0x1e, 0xcf, 0x3f, 0xf0, 0xc5, 0xe0,

    /* U+00E9 "é" */
    0x10, 0x80, 0x1e, 0xcf, 0x3f, 0xf0, 0xc5, 0xe0,

    /* U+00EA "ê" */
    0x31, 0xe0, 0x1e, 0xcf, 0x3f, 0xf0, 0xc5, 0xe0,

    /* U+00EB "ë" */
    0x48, 0x7, 0xb3, 0xcf, 0xfc, 0x31, 0x78,

    /* U+00EC "ì" */
    0x93, 0xff, 0xf0,

    /* U+00ED "í" */
    0x63, 0xff, 0xf0,

    /* U+00EE "î" */
    0x6f, 0x6, 0x66, 0x66, 0x66,

    /* U+00EF "ï" */
    0x90, 0x66, 0x66, 0x66, 0x60,

    /* U+00F0 "ð" */
    0x78, 0xe0, 0xdf, 0xcf, 0x3c, 0xf3, 0x78,

    /* U+00F1 "ñ" */
    0x66, 0x60, 0x3e, 0xcf, 0x3c, 0xf3, 0xcf, 0x30,

    /* U+00F2 "ò" */
    0x20, 0x40, 0x1e, 0xcf, 0x3c, 0xf3, 0xcd, 0xe0,

    /* U+00F3 "ó" */
    0x10, 0x80, 0x1e, 0xcf, 0x3c, 0xf3, 0xcd, 0xe0,

    /* U+00F4 "ô" */
    0x31, 0xe0, 0x1e, 0xcf, 0x3c, 0xf3, 0xcd, 0xe0,

    /* U+00F5 "õ" */
    0x66, 0x60, 0x1e, 0xcf, 0x3c, 0xf3, 0xcd, 0xe0,

    /* U+00F6 "ö" */
    0x48, 0x7, 0xb3, 0xcf, 0x3c, 0xf3, 0x78,

    /* U+00F7 "÷" */
    0x10, 0x3, 0xf8, 0x1, 0x0,

    /* U+00F8 "ø" */
    0x7f, 0x3d, 0xfb, 0xef, 0x3f, 0x80,

    /* U+00F9 "ù" */
    0x20, 0x40, 0x33, 0xcf, 0x3c, 0xf3, 0xcd, 0xf0,

    /* U+00FA "ú" */
    0x10, 0x80, 0x33, 0xcf, 0x3c, 0xf3, 0xcd, 0xf0,

    /* U+00FB "û" */
    0x31, 0xe0, 0x33, 0xcf, 0x3c, 0xf3, 0xcd, 0xf0,

    /* U+00FC "ü" */
    0x48, 0xc, 0xf3, 0xcf, 0x3c, 0xf3, 0x7c,

    /* U+00FD "ý" */
    0x10, 0x80, 0x33, 0xcf, 0x3c, 0xf3, 0xcd, 0xf0,
    0xe3, 0x78,

    /* U+00FE "þ" */
    0xc3, 0xf, 0xb3, 0xcf, 0x3c, 0xf3, 0xfb, 0xc,
    0x0,

    /* U+00FF "ÿ" */
    0x48, 0xc, 0xf3, 0xcf, 0x3c, 0xf3, 0x7c, 0x38,
    0xde
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 64, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 48, .box_w = 2, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 4, .adv_w = 64, .box_w = 3, .box_h = 3, .ofs_x = 0, .ofs_y = 6},
    {.bitmap_index = 6, .adv_w = 144, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 13, .adv_w = 96, .box_w = 5, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 20, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 31, .adv_w = 144, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 40, .adv_w = 32, .box_w = 1, .box_h = 3, .ofs_x = 0, .ofs_y = 6},
    {.bitmap_index = 41, .adv_w = 64, .box_w = 3, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 46, .adv_w = 64, .box_w = 3, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 51, .adv_w = 96, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 55, .adv_w = 128, .box_w = 7, .box_h = 5, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 60, .adv_w = 48, .box_w = 2, .box_h = 4, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 61, .adv_w = 96, .box_w = 5, .box_h = 1, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 62, .adv_w = 48, .box_w = 2, .box_h = 2, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 63, .adv_w = 96, .box_w = 5, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 70, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 77, .adv_w = 48, .box_w = 2, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 80, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 87, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 94, .adv_w = 128, .box_w = 7, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 102, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 109, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 116, .adv_w = 128, .box_w = 7, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 124, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 131, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 138, .adv_w = 48, .box_w = 2, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 140, .adv_w = 48, .box_w = 2, .box_h = 9, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 143, .adv_w = 112, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 148, .adv_w = 112, .box_w = 6, .box_h = 3, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 151, .adv_w = 112, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 156, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 163, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 174, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 181, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 188, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 195, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 202, .adv_w = 96, .box_w = 5, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 208, .adv_w = 96, .box_w = 5, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 214, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 221, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 228, .adv_w = 48, .box_w = 2, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 231, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 238, .adv_w = 128, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 245, .adv_w = 96, .box_w = 5, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 251, .adv_w = 176, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 263, .adv_w = 128, .box_w = 7, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 271, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 278, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 285, .adv_w = 112, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 293, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 300, .adv_w = 96, .box_w = 5, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 306, .adv_w = 144, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 315, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 322, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 329, .adv_w = 176, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 341, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 348, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 355, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 362, .adv_w = 48, .box_w = 2, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 365, .adv_w = 96, .box_w = 5, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 372, .adv_w = 48, .box_w = 2, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 375, .adv_w = 112, .box_w = 6, .box_h = 3, .ofs_x = 0, .ofs_y = 6},
    {.bitmap_index = 378, .adv_w = 144, .box_w = 8, .box_h = 1, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 379, .adv_w = 48, .box_w = 2, .box_h = 2, .ofs_x = 0, .ofs_y = 8},
    {.bitmap_index = 380, .adv_w = 112, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 386, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 393, .adv_w = 96, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 398, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 405, .adv_w = 112, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 411, .adv_w = 96, .box_w = 5, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 417, .adv_w = 112, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 425, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 432, .adv_w = 48, .box_w = 2, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 435, .adv_w = 96, .box_w = 5, .box_h = 11, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 442, .adv_w = 128, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 449, .adv_w = 48, .box_w = 2, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 452, .adv_w = 176, .box_w = 10, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 461, .adv_w = 112, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 467, .adv_w = 112, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 473, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 480, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 487, .adv_w = 96, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 492, .adv_w = 96, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 497, .adv_w = 80, .box_w = 4, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 502, .adv_w = 112, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 508, .adv_w = 112, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 514, .adv_w = 176, .box_w = 10, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 523, .adv_w = 112, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 529, .adv_w = 112, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 537, .adv_w = 112, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 543, .adv_w = 64, .box_w = 3, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 548, .adv_w = 32, .box_w = 1, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 550, .adv_w = 64, .box_w = 3, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 555, .adv_w = 112, .box_w = 6, .box_h = 2, .ofs_x = 0, .ofs_y = 5},
    {.bitmap_index = 557, .adv_w = 96, .box_w = 5, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 565, .adv_w = 96, .box_w = 5, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 573, .adv_w = 96, .box_w = 5, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 581, .adv_w = 96, .box_w = 5, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 589, .adv_w = 112, .box_w = 6, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 598, .adv_w = 96, .box_w = 5, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 606, .adv_w = 96, .box_w = 5, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 614, .adv_w = 96, .box_w = 5, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 622, .adv_w = 112, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 630, .adv_w = 64, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 631, .adv_w = 48, .box_w = 2, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 634, .adv_w = 80, .box_w = 4, .box_h = 8, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 638, .adv_w = 128, .box_w = 7, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 646, .adv_w = 112, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 651, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 658, .adv_w = 32, .box_w = 1, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 660, .adv_w = 96, .box_w = 5, .box_h = 12, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 668, .adv_w = 80, .box_w = 4, .box_h = 1, .ofs_x = 0, .ofs_y = 8},
    {.bitmap_index = 669, .adv_w = 144, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 678, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 683, .adv_w = 128, .box_w = 7, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 690, .adv_w = 96, .box_w = 5, .box_h = 3, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 692, .adv_w = 112, .box_w = 6, .box_h = 1, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 693, .adv_w = 144, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 702, .adv_w = 80, .box_w = 4, .box_h = 1, .ofs_x = 0, .ofs_y = 8},
    {.bitmap_index = 703, .adv_w = 80, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 5},
    {.bitmap_index = 705, .adv_w = 128, .box_w = 7, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 712, .adv_w = 96, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 717, .adv_w = 96, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 722, .adv_w = 48, .box_w = 2, .box_h = 2, .ofs_x = 0, .ofs_y = 8},
    {.bitmap_index = 723, .adv_w = 144, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 732, .adv_w = 144, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 741, .adv_w = 48, .box_w = 2, .box_h = 2, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 742, .adv_w = 48, .box_w = 2, .box_h = 3, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 743, .adv_w = 48, .box_w = 2, .box_h = 7, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 745, .adv_w = 96, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 750, .adv_w = 128, .box_w = 7, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 757, .adv_w = 192, .box_w = 11, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 770, .adv_w = 192, .box_w = 11, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 783, .adv_w = 240, .box_w = 14, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 799, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 806, .adv_w = 112, .box_w = 6, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 815, .adv_w = 112, .box_w = 6, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 824, .adv_w = 112, .box_w = 6, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 833, .adv_w = 112, .box_w = 6, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 842, .adv_w = 112, .box_w = 6, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 851, .adv_w = 112, .box_w = 6, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 860, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 871, .adv_w = 112, .box_w = 6, .box_h = 12, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 880, .adv_w = 96, .box_w = 5, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 888, .adv_w = 96, .box_w = 5, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 896, .adv_w = 96, .box_w = 5, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 904, .adv_w = 96, .box_w = 5, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 911, .adv_w = 48, .box_w = 2, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 914, .adv_w = 48, .box_w = 2, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 917, .adv_w = 80, .box_w = 4, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 923, .adv_w = 80, .box_w = 4, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 929, .adv_w = 128, .box_w = 7, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 937, .adv_w = 128, .box_w = 7, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 948, .adv_w = 112, .box_w = 6, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 957, .adv_w = 112, .box_w = 6, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 966, .adv_w = 112, .box_w = 6, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 975, .adv_w = 112, .box_w = 6, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 984, .adv_w = 112, .box_w = 6, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 993, .adv_w = 112, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 998, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1005, .adv_w = 112, .box_w = 6, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1014, .adv_w = 112, .box_w = 6, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1023, .adv_w = 112, .box_w = 6, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1032, .adv_w = 112, .box_w = 6, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1041, .adv_w = 112, .box_w = 6, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1050, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1057, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1064, .adv_w = 112, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1072, .adv_w = 112, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1080, .adv_w = 112, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1088, .adv_w = 112, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1096, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1103, .adv_w = 112, .box_w = 6, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1112, .adv_w = 176, .box_w = 10, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1121, .adv_w = 96, .box_w = 5, .box_h = 10, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 1128, .adv_w = 112, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1136, .adv_w = 112, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1144, .adv_w = 112, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1152, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1159, .adv_w = 48, .box_w = 2, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1162, .adv_w = 48, .box_w = 2, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1165, .adv_w = 80, .box_w = 4, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1170, .adv_w = 80, .box_w = 4, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1175, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1182, .adv_w = 112, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1190, .adv_w = 112, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1198, .adv_w = 112, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1206, .adv_w = 112, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1214, .adv_w = 112, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1222, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1229, .adv_w = 128, .box_w = 7, .box_h = 5, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 1234, .adv_w = 112, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1240, .adv_w = 112, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1248, .adv_w = 112, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1256, .adv_w = 112, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1264, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1271, .adv_w = 112, .box_w = 6, .box_h = 13, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 1281, .adv_w = 112, .box_w = 6, .box_h = 11, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 1290, .adv_w = 112, .box_w = 6, .box_h = 12, .ofs_x = 0, .ofs_y = -3}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_1[] = {
    0x0, 0x1, 0x2, 0x3, 0x10, 0x11
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 98, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    },
    {
        .range_start = 141, .range_length = 18, .glyph_id_start = 99,
        .unicode_list = unicode_list_1, .glyph_id_ofs_list = NULL, .list_length = 6, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    },
    {
        .range_start = 160, .range_length = 96, .glyph_id_start = 105,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 3,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t lv_font_chicago_8 = {
#else
lv_font_t lv_font_chicago_8 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 15,          /*The maximum line height required by the font*/
    .base_line = 3,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 0,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if LV_FONT_CHICAGO_8*/

