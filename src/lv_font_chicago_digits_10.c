/*******************************************************************************
 * Size: 10 px
 * Bpp: 1
 * Opts: --font data/pixChicago.ttf --size 10 --bpp 1 --format lvgl --output src/lv_font_chicago_digits_10.c --range 0x20,0x30-0x39
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef LV_FONT_CHICAGO_DIGITS_10
#define LV_FONT_CHICAGO_DIGITS_10 1
#endif

#if LV_FONT_CHICAGO_DIGITS_10

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0030 "0" */
    0x7c, 0x8b, 0x1e, 0x7c, 0x78, 0xff, 0xfb, 0xc7,
    0x8f, 0x1b, 0xe0,

    /* U+0031 "1" */
    0x5f, 0xff, 0xff,

    /* U+0032 "2" */
    0x7c, 0xa, 0x18, 0x30, 0x60, 0xc3, 0x3a, 0x61,
    0x83, 0x7, 0xf0,

    /* U+0033 "3" */
    0xfe, 0x18, 0x30, 0xc7, 0xc0, 0x81, 0x83, 0x6,
    0xe, 0x1b, 0xe0,

    /* U+0034 "4" */
    0x6, 0x6, 0x1e, 0x36, 0x46, 0xc6, 0xc6, 0xff,
    0x6, 0x6, 0x6, 0x6,

    /* U+0035 "5" */
    0xff, 0x83, 0x6, 0xf, 0xc0, 0x81, 0x83, 0x6,
    0xe, 0x1b, 0xe0,

    /* U+0036 "6" */
    0x3c, 0xc1, 0x86, 0xf, 0xc0, 0xb1, 0xe3, 0xc7,
    0x8f, 0x1b, 0xe0,

    /* U+0037 "7" */
    0xff, 0x80, 0xc0, 0x60, 0x30, 0x60, 0x30, 0x18,
    0x18, 0xc, 0x6, 0x3, 0x1, 0x80,

    /* U+0038 "8" */
    0x7c, 0x8b, 0x1e, 0x3c, 0x78, 0xc6, 0x63, 0xc7,
    0x8f, 0x1b, 0xe0,

    /* U+0039 "9" */
    0x7c, 0x8b, 0x1e, 0x3c, 0x78, 0xf1, 0xbf, 0x6,
    0x18, 0x23, 0xc0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 80, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 140, .box_w = 7, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 12, .adv_w = 60, .box_w = 2, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 15, .adv_w = 140, .box_w = 7, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 26, .adv_w = 140, .box_w = 7, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 37, .adv_w = 160, .box_w = 8, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 49, .adv_w = 140, .box_w = 7, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 60, .adv_w = 140, .box_w = 7, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 71, .adv_w = 160, .box_w = 9, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 85, .adv_w = 140, .box_w = 7, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 96, .adv_w = 140, .box_w = 7, .box_h = 12, .ofs_x = 0, .ofs_y = 0}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 1, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    },
    {
        .range_start = 48, .range_length = 10, .glyph_id_start = 2,
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
    .cmap_num = 2,
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
const lv_font_t lv_font_chicago_digits_10 = {
#else
lv_font_t lv_font_chicago_digits_10 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 12,          /*The maximum line height required by the font*/
    .base_line = 0,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if LV_FONT_CHICAGO_DIGITS_10*/
